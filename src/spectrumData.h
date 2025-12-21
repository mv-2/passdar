#include <complex>
#include <deque>
#include <mutex>
#include <vector>

class SpecData {
public:
  unsigned int max_length;

  std::mutex mutex_lock;

  std::deque<std::complex<double>> *data;

  std::vector<std::complex<double>> spectrum;

  std::vector<double> frequency;

  SpecData(int max_length);

  void calc_dft();

  void update_data(short *xi, short *xq, unsigned int numSamples);
};
