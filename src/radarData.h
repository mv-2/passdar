#ifndef RADARDATA_H
#define RADARDATA_H

#include <atomic>
#include <unistd.h>

#include "spectrumData.h"
/*
 * Stores data of both streams required for RADAR processing
 */
class RadarData {
public:
  // RSPDuo stream A
  SpecData *stream_a_data;

  // RSPDuo stream B
  SpecData *stream_b_data;

  /*
   * Constructor for RadarData class
   *
   * @param stream_a_data Pointer to SpecData object for stream A
   * @param stream_B_data Pointer to SpecData object for stream B
   */
  RadarData(SpecData *stream_a_data, SpecData *stream_b_data);

  /*
   * Plots live spectra comparison of stream A and B using GNUPLOT
   *
   * @param exit_flag Pointer to atomic<bool> flag set to true when user ends
   * program.
   */
  void plot_spectra(std::atomic<bool> *exit_flag);

  /*
   * Calculates ambiguity surface between receivers.
   */
  void process_ambiguity();
};
#endif
