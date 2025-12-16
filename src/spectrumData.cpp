#include "spectrumData.h"

SpecData::SpecData(unsigned int _max_length) {
  max_length = _max_length;
  data = new std::deque<std::complex<double>>;
}
