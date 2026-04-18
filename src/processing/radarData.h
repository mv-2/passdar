#ifndef RADARDATA_H
#define RADARDATA_H

#include <atomic>
#include <chrono>
#include <complex.h>
#include <fftw3.h>
#include <mutex>
#include <unistd.h>

#include "../hardwareInterface/cfgInterface.h"
#include "../util/leftRightData.h"
#include "spectrumData.h"

// TODO: Make consts class?
/*
 * @brief Wave propagation velocity (I know we aren't in a vacuum)
 */
const double PHASE_VELOCITY = 3e8;

/*
 * @brief Stores data of both streams required for RADAR processing
 */
class RadarData {
public:
  /*
   * @brief  Constructor for RadarData class
   *
   * @param cfg Config struct containing required settings
   * @param stream_a_data Pointer to SpecData object for stream A
   * @param stream_B_data Pointer to SpecData object for stream B
   */
  RadarData(Config cfg, SpecData *stream_a_data, SpecData *stream_b_data);

  /*
   * @brief Calculates cross-ambiguity surface between receivers.
   */
  void radar_process(std::atomic<bool> *exit_flag, std::mutex *fftw_plan_mutex);

  /// Vector containing all range values ambiguity is calculated for
  std::vector<double> range_vals;

  /// Vector containing all speed values ambiguity is calculated for
  std::vector<double> speed_vals;

  /// flattened array of 2D ambiguity surface
  LeftRight<double> ambiguity;

  /// flattened array of 2D detection surface
  LeftRight<double> detection;

  /// Receiver A data
  SpecData *stream_a_data;

  /// Receiver B data
  SpecData *stream_b_data;

  /// range step resolution in metres
  double range_step;

  /// Maximum speed to be calculated
  double max_speed;

  /// speed step resolution in metres per second
  double speed_step;

  /// Number of ambuity surfaces generated per s
  double ambiguity_rate;

  /// Ambiguity spectrum type
  AmbiguityType amb_type;

  /// Number of range points
  int n_range;

  /// Number of speed points either side of 0 speed
  int n_speed;

  /// number of speed columns in ambiguity surface
  int ambiguity_columns;

  std::atomic<bool>
      ready_flag; /// Flag denoting when processing data is ready to be accessed

private:
  /**
   * @brief Cell averaged CFAR processing
   */
  void CA_CFAR();

  /*
   * @brief thread safe FFTW plan creation
   */
  void initialise_fftw_plans(void);

  /*
   * @brief Calculates ambiguity surface over single range row
   */
  void ambiguity_row_calc(int row);

  // Detection config
  DetectionConfig detection_config;

  /// Vector to signals of various time delay products
  std::vector<fftw_complex *> delay_lag_product;

  /// Result array for each row of FFTW ambiguity calculation
  std::vector<fftw_complex *> fftw_amb_out;

  /// Vector of FFTW plans for each row of ambiguity calculation
  std::vector<fftw_plan> fftw_amb_plans;

  /// Copy of Receiver A buffer in fftw_complex type
  fftw_complex *data_a_copy;

  /// Copy of Receiver B buffer in fftw_complex type
  fftw_complex *data_b_copy;

  /// twiddle factors for pruned fft results
  fftw_complex *twiddle_factors;

  /// Last time
  std::chrono::time_point<std::chrono::high_resolution_clock> last_time;

  /// Length of sample buffer
  int sample_buffer_size;

  /// Number of points used for cross-correlations in ambiguity calculation
  int delay_lag_length;

  /// Real sample frequency with decimation applied
  unsigned int sample_frequency;

  /// Scaling for ambiguity calculation
  DisplayScale ambiguity_scale;
};
#endif
