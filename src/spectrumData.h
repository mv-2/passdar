#ifndef SPECTRUMDATA_H
#define SPECTRUMDATA_H

#include <complex>
#include <fftw3.h>
#include <jsoncpp/json/json.h>
#include <mutex>
#include <unordered_map>
#include <vector>

#include "cfgInterface.h"
#include "receiverIq.h"

/*
 * Stores spectrum data
 */
class SpecData {
public:
  // maximum number of samples stored by object
  unsigned int max_length;

  // Raw IQ data object
  ReceiverRawIQ *data_iq;

  // Spectrum buffer to store FFTW3 DFT results
  fftw_complex *spectrum;

  // Frequency vector
  std::vector<double> frequency;

  // Unordered map of numeric bandwidths
  static const std::unordered_map<std::string, double> bwNumMap;

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
  void process_data(std::atomic<bool> *exit_flag);

  /*
   * Set datablock of name $data_<id> in gnuplot process.
   *
   * @param plot_pipe Pipe with persistent gnuplot process
   * @param id ID number of datablock $data_<id>
   */
  void set_plot_datablock(FILE *plot_pipe, int id);

  /*
   * Updates sample buffer with most recent samples from USB packet
   *
   * @param *xi I/real sample buffer from RSPDuo
   * @param *xq Q/imaginaryv sample buffer from RSPDuo
   * @param numSamples buffer length
   */
  void update_data(short *xi, short *xq, unsigned int numSamples);

private:
  // Mutex
  std::mutex mutex_lock;
  // FFTW plan
  fftw_plan fft_plan;

  // Samples copied and casted from data_iq field buffers
  fftw_complex *sample_buffer;

  /*
   * Calcualtes complex DFT of current sample set in data_iq. Result stored in
   * spectrum field.
   */
  void calc_dft();
};
#endif
