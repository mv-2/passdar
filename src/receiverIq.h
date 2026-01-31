#ifndef RECEIVERIQ_H
#define RECEIVERIQ_H

#include <complex.h>
#include <deque>
#include <mutex>

/*
 * Store Raw IQ values output from RSPDuo device
 */
struct ReceiverRawIQ {
  // std::deque of std::complex<double> samples
  std::deque<std::complex<double>> samples;

  // Mutex
  std::mutex mutex_lock;

  // maximum number of samples stored by object
  unsigned int buffer_size;

  /*
   * ReceiverRawIQ constructor
   *
   * @param buffer_size number of points stored at any time in buffers
   */
  ReceiverRawIQ(unsigned int buffer_size);

  /*
   * Update data from USB data packet
   *
   * @param *xi I/real sample buffer from RSPDuo
   * @param *xq Q/imaginaryv sample buffer from RSPDuo
   * @param numSamples buffer length
   */
  void update_data(short *xi, short *xq, unsigned int numSamples);
};
#endif
