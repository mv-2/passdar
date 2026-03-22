#include "receiverIq.h"
#include <fftw3.h>

ReceiverRawIQ::ReceiverRawIQ(unsigned int _buffer_size) {
  // Set buffer size and empty samples vector
  buffer_size = _buffer_size;
  samples = std::deque<std::array<double, 2>>(buffer_size);
}
