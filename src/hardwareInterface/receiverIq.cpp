#include "receiverIq.h"
#include <fftw3.h>

ReceiverRawIQ::ReceiverRawIQ(unsigned int _buffer_size) {
  buffer_size = _buffer_size;
  samples = std::deque<std::array<double, 2>>(buffer_size);
}
