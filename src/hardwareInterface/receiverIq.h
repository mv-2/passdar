#ifndef RECEIVERIQ_H
#define RECEIVERIQ_H

#include <array>
#include <deque>
#include <mutex>

/**
 * @brief Stores Raw IQ values output from RSPDuo device
 * @details Class stores incoming IQ values by pushing and popping from a set
 * size using a std::deque
 */
struct ReceiverRawIQ {
  /*
   * @brief ReceiverRawIQ constructor
   * @param buffer_size number of points stored at any time in buffers
   */
  ReceiverRawIQ(unsigned int buffer_size);

  /*
   * @brief Update data from USB data packet
   *
   * @param *xi I/real sample buffer from RSPDuo
   * @param *xq Q/imaginary sample buffer from RSPDuo
   * @param numSamples buffer length
   */
  void update_data(short *xi, short *xq, unsigned int numSamples);

  /// Object mutex
  std::mutex mutex_lock;

  /**
   * @brief Sample buffer
   * @details std::deque of std::array<double, 2> to store complex numbers for
   * conversion to fftw_complex values
   */
  std::deque<std::array<double, 2>> samples;

  /// Number of samples to be recorded in buffer
  unsigned int buffer_size;
};
#endif
