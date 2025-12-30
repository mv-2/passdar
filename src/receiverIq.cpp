#include "receiverIq.h"

ReceiverRawIQ::ReceiverRawIQ(unsigned int _max_length) {
  max_length = _max_length;
  samples = std::deque<std::complex<double>>(max_length);
}
