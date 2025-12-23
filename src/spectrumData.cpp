#include <atomic>
#include <unistd.h>

#include "spectrumData.h"

const std::complex<double> I = std::complex<double>(0, 1.0);
const std::complex<double> COMPLEX_ZERO = std::complex<double>(0.0, 0.0);

const std::unordered_map<std::string, double> SpecData::bwNumMap = {
    {"sdrplay_api_BW_0_200", 200.0},  {"sdrplay_api_BW_0_300", 300.0},
    {"sdrplay_api_BW_0_600", 600.0},  {"sdrplay_api_BW_1_536", 1536.0},
    {"sdrplay_api_BW_5_000", 5000.0}, {"sdrplay_api_BW_6_000", 6000.0},
    {"sdrplay_api_BW_7_000", 7000.0}, {"sdrplay_api_BW_8_000", 8000.0}};

SpecData::SpecData(Json::Value cfg) {
  max_length = cfg["processing"]["n_samples"].asUInt();
  data_iq = new ReceiverRawIQ(max_length);
  spectrum = (fftw_complex *)fftw_malloc(sizeof(fftw_complex) * max_length);
  sample_buffer =
      (fftw_complex *)fftw_malloc(sizeof(fftw_complex) * max_length);
  fft_plan = fftw_plan_dft_1d(max_length, sample_buffer, spectrum, FFTW_FORWARD,
                              FFTW_ESTIMATE);
  double bandwidth =
      SpecData::bwNumMap.at(cfg["receiver"]["bwType"].asString());
  frequency = (double *)malloc(sizeof(double) * max_length);
  for (unsigned int i = 0; i < max_length; i++) {
    frequency[i] = 2.0 * bandwidth * ((double)i) / ((double)max_length);
  }
}

void SpecData::update_data(short *xi, short *xq, unsigned int numSamples) {
  // Hold mutex to ensure no other threads read from buffer
  data_iq->mutex_lock.lock();
  // Update and rotate buffer
  for (unsigned int i = 0; i < numSamples; i++) {
    data_iq->samples.pop_front();
    data_iq->samples.push_back(
        std::complex<double>((double)xi[i], (double)xq[i]));
  }
  data_iq->mutex_lock.unlock();
}

void SpecData::calc_dft() {
  // Hold mutex to ensure no other threads read from buffer
  data_iq->mutex_lock.lock();
  // TEST: Look into better conversion methods
  for (unsigned int i = 0; i < max_length; i++) {
    sample_buffer[i][0] = data_iq->samples.at(i).real();
    sample_buffer[i][1] = data_iq->samples.at(i).imag();
  }
  data_iq->mutex_lock.unlock();
  // Execute FFTW plan
  fftw_execute(fft_plan);
}

void SpecData::process_data(std::atomic<bool> *exit_flag) {
  while (!exit_flag->load()) {
    // Lock mute for SpecData so plotting thread does not read spectrum during
    // FFTW process
    mutex_lock.lock();
    calc_dft();
    mutex_lock.unlock();
    sleep(1);
  }

  // Free resources
  fftw_destroy_plan(fft_plan);
  fftw_free(spectrum);
  fftw_free(sample_buffer);
}

void SpecData::set_plot_datablock(FILE *plot_pipe, int id) {
  // Assign datablock to $data_<x> in gnuplot process
  fprintf(plot_pipe, "$data_%d << EOD\n", id);
  // Lock mutex to ensure spectrum is not updated during plotting process
  mutex_lock.lock();
  // Calculate offset to plot only 1 side of DFT
  unsigned int offset = max_length / 2;
  for (unsigned int i = 0; i < max_length / 2; i++) {
    // TODO: Calculate frequency vector correctly to replace i with frequency[i]
    fprintf(
        plot_pipe, "%f %f\n", frequency[i],
        std::log10(sqrt(spectrum[i + offset][0] * spectrum[i + offset][0] +
                        spectrum[i + offset][1] * spectrum[i + offset][1])));
  }
  mutex_lock.unlock();
  // End data write
  fprintf(plot_pipe, "EOD\n");
}

ReceiverRawIQ::ReceiverRawIQ(unsigned int _max_length) {
  max_length = _max_length;
  samples = std::deque<std::complex<double>>(max_length);
}

RadarData::RadarData(SpecData *_stream_a_data, SpecData *_stream_b_data) {
  stream_a_data = _stream_a_data;
  stream_b_data = _stream_b_data;
}

void RadarData::plot_spectra(std::atomic<bool> *exit_flag) {
  // Initialise plot window
  FILE *plot_pipe = popen("gnuplot -persist", "w");

  // Reset data block and replot each second
  while (!(exit_flag->load(std::memory_order_relaxed))) {
    sleep(1);
    // Set datablock values
    stream_a_data->set_plot_datablock(plot_pipe, 1);
    stream_b_data->set_plot_datablock(plot_pipe, 2);

    // Create multiplot layout
    fprintf(plot_pipe,
            "set multiplot layout 2,1 rowsfirst title \"Spectra\"\n");

    // Receiver A plot
    fprintf(plot_pipe, "set title \"Receiver A\"\n");
    fprintf(plot_pipe, "set xlabel \"Frequency [MHz]\"\n");
    fprintf(plot_pipe, "set ylabel \"Amplitude\"\n");
    fprintf(plot_pipe, "unset key\n");
    fprintf(plot_pipe, "plot $data_1 with lines\n");

    // Receiver B plot
    fprintf(plot_pipe, "set title \"Receiver B\"\n");
    fprintf(plot_pipe, "set xlabel \"Frequency [MHz]\"\n");
    fprintf(plot_pipe, "set ylabel \"Amplitude\"\n");
    fprintf(plot_pipe, "unset key\n");
    fprintf(plot_pipe, "plot $data_2 with lines\n");
  }
}
