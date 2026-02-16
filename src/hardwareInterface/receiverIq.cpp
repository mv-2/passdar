#include "receiverIq.h"
#include <fftw3.h>

ReceiverRawIQ::ReceiverRawIQ(unsigned int _buffer_size) {
  buffer_size = _buffer_size;
  real_samples = std::deque<double>(buffer_size);
  imag_samples = std::deque<double>(buffer_size);
}
