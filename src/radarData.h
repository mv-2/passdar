#ifndef RADARDATA_H
#define RADARDATA_H

#include <atomic>
#include <mutex>
#include <unistd.h>

#include "cfgInterface.h"
#include "spectrumData.h"

/*
 * Stores data of both streams required for RADAR processing
 */
class RadarData {
public:
  // Mutex
  std::mutex ambiguity_mutex;

  // RSPDuo stream A
  SpecData *stream_a_data;

  // RSPDuo stream B
  SpecData *stream_b_data;

  // Range step in m
  double range_step;

  // Speed step in m
  double speed_step;

  // Number of speed points
  int n_speed;

  // Number of range points
  int n_range;

  // ambiguity surface
  std::vector<double> ambiguity;

  /*
   * Constructor for RadarData class
   *
   * @param stream_a_data Pointer to SpecData object for stream A
   * @param stream_B_data Pointer to SpecData object for stream B
   */
  RadarData(Config cfg, SpecData *stream_a_data, SpecData *stream_b_data);

  /*
   * Calculates ambiguity surface between receivers.
   */
  void process_ambiguity(std::atomic<bool> *exit_flag);

private:
  // Copied data samples
  std::vector<std::complex<double>> data_a_copy;
  std::vector<std::complex<double>> data_b_copy;

  // sample frequency
  unsigned int sample_frequency;
};
#endif
