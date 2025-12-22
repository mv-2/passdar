#include <complex>
#include <deque>
#include <fftw3.h>
#include <jsoncpp/json/json.h>
#include <mutex>
#include <vector>
/*
 * Store Raw IQ values output from RSPDuo device
 */
class ReceiverRawIQ {
public:
  // std::deque of std::complex<double> samples
  std::deque<std::complex<double>> samples;

  // Mutex
  std::mutex mutex_lock;

  // maximum number of samples stored by object
  unsigned int max_length;

  /*
   * ReceiverRawIQ constructor
   *
   * @param max_length number of points stored at any time in buffers
   */
  ReceiverRawIQ(unsigned int max_length);

  /*
   * Update data from USB data packet
   *
   * @param *xi I/real sample buffer from RSPDuo
   * @param *xq Q/imaginaryv sample buffer from RSPDuo
   * @param numSamples buffer length
   */
  void update_data(short *xi, short *xq, unsigned int numSamples);
};

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

  /*
   * constructor
   *
   * @param max_length Maximum number of samples stored in sample buffers
   */
  SpecData(Json::Value processingCfg);

  /*
   * Processing function loop.
   *
   * @param loop_exit Function with no arguments which returns true on condition
   * to break looping.
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

/*
 * Stores data of both streams required for RADAR processing
 */
class RadarData {
public:
  // RSPDuo stream A
  SpecData *stream_a_data;

  // RSPDuo stream B
  SpecData *stream_b_data;

  /*
   * Constructor for RadarData class
   *
   * @param stream_a_data Pointer to SpecData object for stream A
   * @param stream_B_data Pointer to SpecData object for stream B
   */
  RadarData(SpecData *stream_a_data, SpecData *stream_b_data);

  /*
   * Plots live spectra comparison of stream A and B using GNUPLOT
   *
   * @param loop_exit Function with no arguments returning true when exit
   * condition for loop is met
   */
  void plot_spectra(std::atomic<bool> *exit_flag);
};
