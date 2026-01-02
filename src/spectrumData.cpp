#include <atomic>
#include <fftw3.h>
#include <unistd.h>

#include "cfgInterface.h"
#include "spectrumData.h"

const std::unordered_map<std::string, double> SpecData::bwNumMap = {
    {"sdrplay_api_BW_0_200", 200.0},  {"sdrplay_api_BW_0_300", 300.0},
    {"sdrplay_api_BW_0_600", 600.0},  {"sdrplay_api_BW_1_536", 1536.0},
    {"sdrplay_api_BW_5_000", 5000.0}, {"sdrplay_api_BW_6_000", 6000.0},
    {"sdrplay_api_BW_7_000", 7000.0}, {"sdrplay_api_BW_8_000", 8000.0}};

SpecData::SpecData(Config cfg) {
  max_length = cfg.process_cfg.buffer_size;
  data_iq = new ReceiverRawIQ(max_length);
  spectrum_internal =
      (fftw_complex *)fftw_malloc(sizeof(fftw_complex) * max_length);
  std::vector<double> spec(max_length);
  spectrum.resize(max_length);
  sample_buffer =
      (fftw_complex *)fftw_malloc(sizeof(fftw_complex) * max_length);
  fft_plan = fftw_plan_dft_1d(max_length, sample_buffer, spectrum_internal,
                              FFTW_FORWARD, FFTW_ESTIMATE);
  double bandwidth = cfg.receiver_cfg.bwType;
  for (int i = -static_cast<int>(max_length) / 2;
       i < static_cast<int>(max_length) / 2; i++) {
    frequency.push_back((static_cast<double>(i) * bandwidth +
                         static_cast<double>(cfg.receiver_cfg.fc)) /
                        1000.0);
  }
}

void SpecData::update_data(short *xi, short *xq, unsigned int numSamples) {
  // Hold mutex to ensure no other threads read from buffer
  data_iq->mutex_lock.lock();
  // Update and rotate buffer
  for (unsigned int i = 0; i < numSamples; i++) {
    data_iq->samples.pop_front();
    data_iq->samples.push_back(std::complex<double>(
        static_cast<double>(xi[i]), static_cast<double>(xq[i])));
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
    // Lock mutex for SpecData so plotting thread does not read spectrum during
    // FFTW process. Perform FFTshift on assignment
    int id_swap;
    mutex_lock.lock();
    calc_dft();
    for (int i = 0; i < max_length; i++) {
      id_swap = (i + max_length / 2 - 1) % max_length;
      spectrum[i] = 20.0 * log10(std::sqrt(spectrum_internal[id_swap][0] *
                                               spectrum_internal[id_swap][0] +
                                           spectrum_internal[id_swap][1] *
                                               spectrum_internal[id_swap][1]));
    }
    mutex_lock.unlock();
    sleep(1);
  }

  // Free resources
  fftw_destroy_plan(fft_plan);
  fftw_free(spectrum_internal);
  fftw_free(sample_buffer);
}
