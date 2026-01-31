#include "receiverIq.h"

ReceiverRawIQ::ReceiverRawIQ(unsigned int _buffer_size) {
  buffer_size = _buffer_size;
  samples = std::deque<std::complex<double>>(buffer_size);
}
