#include <complex>
#include <deque>
#include <mutex>
#include <vector>

class ReceiverRawIQ {
public:
  std::deque<std::complex<double>> samples;

  std::mutex mutex_lock;

  unsigned int max_length;

  ReceiverRawIQ(unsigned int max_length);

  void update_data(short *xi, short *xq, unsigned int numSamples);
};

class SpecData {
public:
  unsigned int max_length;

  std::mutex mutex_lock;

  ReceiverRawIQ *data_iq;

  std::vector<std::complex<double>> spectrum;

  std::vector<double> frequency;

  SpecData(int max_length);

  void calc_dft();

  void update_data(short *xi, short *xq, unsigned int numSamples);

  void process_data(bool(loop_exit)(void));

  void set_plot_datablock(FILE *plot_pipe, std::string id);
};
