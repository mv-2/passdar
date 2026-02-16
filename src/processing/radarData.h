#ifndef RADARDATA_H
#define RADARDATA_H

#include <atomic>
#include <fftw3.h>
#include <mutex>
#include <unistd.h>

#include "../hardwareInterface/cfgInterface.h"
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

  // Number of speed points
  int n_speed;

  // maximum measurable speed
  double max_speed;

  // Speed step in m/s
  double speed_step;

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
  int sample_buffer_size;
  fftw_complex *data_a_copy;
  fftw_complex *data_b_copy;
  std::vector<fftw_complex *> delay_lag_product;
  int delay_lag_length;

  // sample frequency
  unsigned int sample_frequency;

  /*
   * Calculates ambiguity surface over narrowed region.
   */
  void ambiguity_thread_calc(int row);

  /*
   * FFTW3 plans for ambiguity calculation
   */
  std::vector<fftw_plan> fftw_amb_plans;

  /*
   * Result arrays for output of fftw3 ambiguity calculation
   */
  std::vector<fftw_complex *> fftw_amb_out;
};
#endif
