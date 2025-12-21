#include "spectrumData.h"

const std::complex<double> I = std::complex<double>(0, 1.0);
const std::complex<double> COMPLEX_ZERO = std::complex<double>(0.0, 0.0);

// TODO: Figure out what I'm doing here. Maybe NFFT as arg?
SpecData::SpecData(int _max_length) {
  max_length = _max_length;
  data = new std::deque<std::complex<double>>;
  data->resize(max_length, COMPLEX_ZERO);
  std::vector<std::complex<double>> _spectrum(max_length);
  spectrum = _spectrum;
}

void SpecData::update_data(short *xi, short *xq, unsigned int numSamples) {
  mutex_lock.lock();
  for (unsigned int i = 0; i < numSamples; i++) {
    data->pop_front();
    data->push_back(std::complex<double>((double)xi[i], (double)xq[i]));
  }
  mutex_lock.unlock();
}

void SpecData::calc_dft() {
  for (int k = 0; k < max_length; k++) {
    spectrum[k] = COMPLEX_ZERO;
    for (int n = 0; n < max_length; n++) {
      spectrum[k] += data->at((int)n) *
                     std::polar(1.0, -2.0 * M_PI * (double)k * (double)n /
                                         (double)max_length);
    }
  }
}
