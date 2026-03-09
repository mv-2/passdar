#ifndef SPECTRUMDATA_H
#define SPECTRUMDATA_H

#include <atomic>
#include <fftw3.h>
#include <mutex>
#include <vector>

#include "../hardwareInterface/cfgInterface.h"
#include "../hardwareInterface/receiverIq.h"

/*
 * Stores spectrum data
 */
class SpecData {
public:
  // maximum number of samples stored by object
  unsigned int buffer_size;

  // Raw IQ data
  ReceiverRawIQ *spectrum_iq;
  ReceiverRawIQ *ambiguity_iq;

  // Spectrum vector
  std::vector<double> spectrum;

  // Frequency vector [MHz]
  std::vector<double> frequency;

  /*
   * constructor
   *
   * @param cfg Config struct containing data required to set SpecData
   * parameters
   */
  SpecData(Config cfg);

  /*
   * Processing function loop.
   *
   * @param exit_flag Pointer to atomic<bool> flag denoting user request to end
   * program.
   */
  void process_spectrum(std::atomic<bool> *exit_flag);

  /*
   * Updates sample buffer with most recent samples from USB packet
   *
   * @param *xi I/real sample buffer from RSPDuo
   * @param *xq Q/imaginaryv sample buffer from RSPDuo
   * @param numSamples buffer length
   */
  void update_data(short *xi, short *xq, unsigned int numSamples);

  // Mutex
  std::mutex mutex_lock;

  // Flag denoting when processing data is ready to be accessed
  std::atomic<bool> ready_flag;

private:
  // FFTW plan
  fftw_plan fft_plan;

  // Samples copied and casted from data_iq field buffers
  fftw_complex *sample_buffer;

  /*
   * Calcualtes complex DFT of current sample set in data_iq. Result stored in
   * spectrum field.
   */
  void calc_dft();

  // Spectrum buffer to store FFTW3 DFT results
  fftw_complex *spectrum_internal;

  // Windowing vector
  std::vector<double> dft_window;
};
#endif
