#include <atomic>
#include <fftw3.h>
#include <iostream>
#include <thread>
#include <unistd.h>

#include "cfgInterface.h"
#include "spectrumData.h"

const std::string SPECTRUM_WISDOM_FILENAME = "cfg/spectrum.wisdom";

SpecData::SpecData(Config cfg) {
  buffer_size = cfg.process_cfg.buffer_size;

  // Pointer to raw data object
  data_iq = new ReceiverRawIQ(buffer_size);

  // Assign all spectrum containers
  std::vector<double> spec(buffer_size);
  spectrum.resize(buffer_size);
  spectrum_internal = fftw_alloc_complex(buffer_size);
  sample_buffer = fftw_alloc_complex(buffer_size);

  // Load wisdom file if available
  if (fftw_import_wisdom_from_filename(SPECTRUM_WISDOM_FILENAME.c_str()) == 0) {
    std::cout << "Failed to load " << SPECTRUM_WISDOM_FILENAME << " file"
              << std::endl;
  }

  // Create FFTW plan
  fft_plan = fftw_plan_dft_1d(buffer_size, sample_buffer, spectrum_internal,
                              FFTW_FORWARD, FFTW_PATIENT);

  // Export wisdom
  if (fftw_export_wisdom_to_filename(SPECTRUM_WISDOM_FILENAME.c_str()) == 0) {
    std::cout << "Failed to export " << SPECTRUM_WISDOM_FILENAME << " file"
              << std::endl;
  }

  // Set frequency vector values
  double sample_bandwidth = static_cast<double>(cfg.receiver_cfg.fs) /
                            static_cast<double>(cfg.receiver_cfg.dec_factor);
  for (int i = -static_cast<int>(buffer_size) / 2;
       i < static_cast<int>(buffer_size) / 2; i++) {
    frequency.push_back((static_cast<double>(i) * sample_bandwidth /
                             static_cast<double>(buffer_size) +
                         static_cast<double>(cfg.receiver_cfg.fc)) /
                        1e6);
  }

  // Assign DFT window
  switch (cfg.process_cfg.dft_window) {
  case DftWindow::Hanning: {
    dft_window.reserve(buffer_size);
    for (unsigned int i = 0; i < buffer_size; i++) {
      dft_window[i] = 0.5 * (1.0 - cos(2 * M_PI * i / (buffer_size - 1)));
    }
    break;
  }
  case DftWindow::Rectangular: {
    dft_window.resize(buffer_size, 1.0);
    break;
  }
  default: {
    std::cerr << "Error assigning window" << std::endl;
  }
  }
}

void SpecData::update_data(short *xi, short *xq, unsigned int numSamples) {
  // Hold mutex to ensure no other threads read from buffer
  data_iq->mutex_lock.lock();
  // Update and rotate buffer
  for (unsigned int i = 0; i < numSamples; i++) {
    data_iq->real_samples.pop_front();
    data_iq->imag_samples.pop_front();
    data_iq->real_samples.emplace_back(static_cast<double>(xi[i]));
    data_iq->imag_samples.emplace_back(static_cast<double>(xq[i]));
  }
  data_iq->mutex_lock.unlock();
}

void SpecData::calc_dft() {
  // Hold mutex to ensure no other threads read from buffer
  data_iq->mutex_lock.lock();
  // Copy data and apply window
  for (unsigned int i = 0; i < buffer_size; i++) {
    // real
    sample_buffer[i][0] = dft_window[i] * data_iq->real_samples[i];
    // imag
    sample_buffer[i][1] = dft_window[i] * data_iq->imag_samples[i];
  }
  data_iq->mutex_lock.unlock();
  // Execute FFTW plan
  fftw_execute(fft_plan);
}

void SpecData::process_spectrum(std::atomic<bool> *exit_flag) {
  while (!exit_flag->load()) {
    // Lock mutex for SpecData so plotting thread does not read spectrum during
    // FFTW process. Perform FFTshift on assignment
    int id_swap;
    mutex_lock.lock();
    calc_dft();
    for (unsigned int i = 0; i < buffer_size; i++) {
      id_swap = (i + buffer_size / 2 - 1) % buffer_size;
      spectrum[i] = 20.0 * log10(std::sqrt(spectrum_internal[id_swap][0] *
                                               spectrum_internal[id_swap][0] +
                                           spectrum_internal[id_swap][1] *
                                               spectrum_internal[id_swap][1]));
    }
    mutex_lock.unlock();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  // Free resources
  fftw_destroy_plan(fft_plan);
  fftw_free(spectrum_internal);
  fftw_free(sample_buffer);
}
