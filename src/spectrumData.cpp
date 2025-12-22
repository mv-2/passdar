#include "spectrumData.h"
#include <fftw3.h>
#include <mutex>
#include <unistd.h>

const std::complex<double> I = std::complex<double>(0, 1.0);
const std::complex<double> COMPLEX_ZERO = std::complex<double>(0.0, 0.0);

// TODO: Figure out what I'm doing here. Maybe NFFT as arg?
SpecData::SpecData(int _max_length) {
  max_length = _max_length;
  data_iq = new ReceiverRawIQ(max_length);
  spectrum = (fftw_complex *)fftw_malloc(sizeof(fftw_complex) * max_length);
  sample_buffer =
      (fftw_complex *)fftw_malloc(sizeof(fftw_complex) * max_length);
  fft_plan = fftw_plan_dft_1d(max_length, sample_buffer, spectrum, FFTW_FORWARD,
                              FFTW_ESTIMATE);
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

void SpecData::process_data(bool(loop_exit)(void)) {
  while (!loop_exit()) {
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
        plot_pipe, "%d %f\n", i,
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
