#include <atomic>
#include <fftw3.h>
#include <iostream>
#include <thread>
#include <unistd.h>

#include "../hardwareInterface/cfgInterface.h"
#include "spectrumData.h"

const std::string SPECTRUM_WISDOM_FILENAME = "cfg/spectrum.wisdom";

SpecData::SpecData(Config cfg) {
  // Not ready
  ready_flag.store(false);

  // Define buffer sizes
  sample_block_size = cfg.process_cfg.sample_block_size;
  total_buffer_size = cfg.receiver_cfg.fs / cfg.process_cfg.sample_block_size;

  // Pointer to raw data object
  spectrum_iq = new ReceiverRawIQ(sample_block_size);
  ambiguity_iq = new ReceiverRawIQ(sample_block_size);

  // Assign all spectrum containers
  spectrum.resize(total_buffer_size);
  spectrum_internal.resize(total_buffer_size);
  sample_buffer.resize(sample_block_size);

  // Set frequency vector values
  double sample_frequency = static_cast<double>(cfg.receiver_cfg.fs) / 1e6;
  for (int i = -static_cast<int>(total_buffer_size) / 2;
       i < static_cast<int>(total_buffer_size) / 2; i++) {
    frequency.push_back((static_cast<double>(i) - 0.5) * sample_frequency /
                            (static_cast<int>(total_buffer_size)) +
                        static_cast<double>(cfg.receiver_cfg.fc) / 1e6);
  }

  // Assign DFT window
  switch (cfg.process_cfg.dft_window) {
  case DftWindow::Hanning: {
    dft_window.reserve(sample_block_size);
    for (unsigned int i = 0; i < sample_block_size; i++) {
      dft_window[i] = 0.5 * (1.0 - cos(2 * M_PI * i / (sample_block_size - 1)));
    }
    break;
  }
  case DftWindow::Rectangular: {
    dft_window.resize(sample_block_size, 1.0);
    break;
  }
  default: {
    std::cerr << "Error assigning window" << std::endl;
  }
  }
}

void SpecData::initialise_fftw_plan(void) {

  // Load wisdom file if available
  if (fftw_import_wisdom_from_filename(SPECTRUM_WISDOM_FILENAME.c_str()) == 0) {
    std::cout << "Failed to load " << SPECTRUM_WISDOM_FILENAME << " file"
              << std::endl;
  }

  // Create FFTW plan
  fft_plan = fftw_plan_dft_1d(
      total_buffer_size, reinterpret_cast<fftw_complex *>(sample_buffer.data()),
      reinterpret_cast<fftw_complex *>(spectrum_internal.data()), FFTW_FORWARD,
      FFTW_EXHAUSTIVE);

  // Export wisdom
  if (fftw_export_wisdom_to_filename(SPECTRUM_WISDOM_FILENAME.c_str()) == 0) {
    std::cout << "Failed to export " << SPECTRUM_WISDOM_FILENAME << " file"
              << std::endl;
  }
}

void SpecData::update_data(short *xi, short *xq, unsigned int numSamples) {
  // cast vars
  double xi_d, xq_d;

  // Hold mutex to ensure no other threads read from buffer
  spectrum_iq->mutex_lock.lock();
  ambiguity_iq->mutex_lock.lock();
  // Update and rotate buffer
  for (unsigned int i = 0; i < numSamples; i++) {
    xi_d = static_cast<double>(xi[i]);
    xq_d = static_cast<double>(xq[i]);
    spectrum_iq->samples.pop_front();
    spectrum_iq->samples.push_back({xi_d, xq_d});

    ambiguity_iq->samples.pop_front();
    ambiguity_iq->samples.push_back({xi_d, xq_d});
  }
  spectrum_iq->mutex_lock.unlock();
  ambiguity_iq->mutex_lock.unlock();
}

void SpecData::calc_dft() {
  // Hold mutex to ensure no other threads read from buffer
  spectrum_iq->mutex_lock.lock();
  // Copy data and apply window
  for (unsigned int i = 0; i < sample_block_size; i++) {
    sample_buffer[i] = dft_window[i] * std::complex(spectrum_iq->samples[i][0],
                                                    spectrum_iq->samples[i][1]);
  }
  spectrum_iq->mutex_lock.unlock();
  // Execute FFTW plan
  fftw_execute(fft_plan);
}

void SpecData::process_spectrum(std::atomic<bool> *exit_flag,
                                std::mutex *fftw_plan_mutex) {
  // Initialise fftw plan
  fftw_plan_mutex->lock();
  initialise_fftw_plan();
  fftw_plan_mutex->unlock();

  while (!exit_flag->load()) {
    // Lock mutex for SpecData so plotting thread does not read spectrum during
    // FFTW process. Perform FFTshift on assignment
    int id_swap;
    mutex_lock.lock();
    calc_dft();
    for (unsigned int i = 0; i < total_buffer_size; i++) {
      id_swap = (i + total_buffer_size / 2 - 1) % total_buffer_size;
      spectrum[i] = 20.0 * log10(std::abs(spectrum_internal[id_swap]));
    }
    mutex_lock.unlock();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Ready after first loop
    ready_flag.store(true);
  }

  // Free resources
  fftw_destroy_plan(fft_plan);
}
