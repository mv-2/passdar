#ifndef SPECTRUMDATA_H
#define SPECTRUMDATA_H

#include <atomic>
#include <complex.h>
#include <fftw3.h>
#include <mutex>
#include <vector>

#include "../hardwareInterface/cfgInterface.h"
#include "../hardwareInterface/receiverIq.h"

/*
 * @brief Stores ones receivers IQ data and processes spectra
 */
class SpecData {
public:
  /**
   * @brief constructor
   *
   * @param cfg Config struct containing data required to set SpecData
   * parameters
   */
  SpecData(Config cfg);

  /**
   * @brief Processing function loop.
   *
   * @param exit_flag Pointer to atomic<bool> flag denoting user request to end
   * program.
   * @param fftw_plan_mutex Pointer to std::mutex ensuring thread safety when
   * generating new FFTW plan
   */
  void process_spectrum(std::atomic<bool> *exit_flag,
                        std::mutex *fftw_plan_mutex);

  /**
   * @brief Updates sample buffer with most recent samples from USB packet
   *
   * @param *xi I/real sample buffer from RSPDuo
   * @param *xq Q/imaginaryv sample buffer from RSPDuo
   * @param numSamples buffer length
   */
  void update_data(short *xi, short *xq, unsigned int numSamples);

  /// Object mutex
  std::mutex mutex_lock;

  /// Spectrum vector
  std::vector<double> spectrum;

  /// Frequency vector in MHz
  std::vector<double> frequency;

  /// Raw IQ data sent to spectrum calculation
  ReceiverRawIQ *spectrum_iq;

  /// Raw IQ data sent to ambiguity calculation
  ReceiverRawIQ *ambiguity_iq;

  /// Number of samples per block
  unsigned int sample_block_size;

  /// Buffer size for calculation spectrum
  unsigned int total_buffer_size;

  /// Flag denoting when processing data is ready to be accessed
  std::atomic<bool> ready_flag;

private:
  /**
   * @brief Calcualtes complex DFT of current sample set in data_iq
   */
  void calc_dft(void);

  /**
   * @brief set up FFTW plan
   */
  void initialise_fftw_plan(void);

  // Windowing vector
  std::vector<double> dft_window;

  // FFTW plan
  fftw_plan fft_plan;

  // Samples copied and casted from data_iq field buffers
  std::vector<std::complex<double>> sample_buffer;

  // Spectrum buffer to store FFTW3 DFT results
  std::vector<std::complex<double>> spectrum_internal;
};
#endif
