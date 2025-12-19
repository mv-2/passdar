#include "spectrumData.h"
#include <vector>

const std::complex<double> I = std::complex<double>(0, 1.0);
const std::complex<double> COMPLEX_ZERO = std::complex<double>(0.0, 0.0);
// TODO: Figure out what I'm doing here. Maybe NFFT as arg?
SpecData::SpecData(int _max_length) {
  max_length = _max_length;
  data = new std::deque<std::complex<double>>;
  std::vector<std::complex<double>> _spectrum(max_length);
  spectrum = _spectrum;
}

void SpecData::calc_dft() {
  for (unsigned int k = 0; k < max_length; k++) {
    spectrum[k] = COMPLEX_ZERO;
    for (unsigned int n = 0; n < max_length; n++) {
      spectrum[k] += data->at(n) * std::exp(-I * 2.0 * 3.14159 * (double)k *
                                            (double)n / (double)max_length);
    }
  }
}
