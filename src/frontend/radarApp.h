#ifndef RADARAPP_H
#define RADARAPP_H

#include <GLFW/glfw3.h>
#include <sdrplay_api.h>
#include <sdrplay_api_tuner.h>
#include <sys/select.h>
#include <thread>
#include <unistd.h>

#include "../hardwareInterface/cfgInterface.h"
#include "../hardwareInterface/sdrCapture.h"
#include "../processing/radarData.h"

/**
 * @brief Class to intake, process and display all radar data
 */
class RadarApp {
public:
  /// @name Constructors
  /// @{
  RadarApp(Config cfg); /// Constructor from Config struct
  RadarApp();           /// Default Constructor
  /// @}

  /**
   * @brief Run app with default or config defined settings
   */
  void run(void);

  /**
   * @brief Receiver object pointing to sdrplay data capture object
   */
  Receiver *receiver;

  /**
   * @brief Receiever A data
   */
  SpecData *stream_a_data;

  /**
   * @brief Receiever B data
   */
  SpecData *stream_b_data;

  /**
   * @brief Radar ambiguity data
   * @details Holds ambiguity and detection processing outputs
   */
  RadarData *radar_data;

  /**
   * @brief Flag to restart app with updated settings
   */
  std::atomic<bool> restart;

private:
  /**
   * @brief Initial setup of all app functions and data containers
   */
  void setup(void);

  /**
   * @brief Update speed variables to ensure that buffer size is divisible by
   * n_speed for pruned FFT calculation
   */
  void update_speed_vars(void);

  /**
   * @brief Update related range variables for consistency
   */
  void update_range_vars(void);

  /**
   * @brief Create ImGui window with GLFW & OpenGL backend
   */
  GLFWwindow *init_window(void);

  // Frame update functions
  /// @name GUI frame update functions
  /// @{
  void update_window(GLFWwindow *window,
                     bool *show_window);    /// Window update wrapper
  void receiver_spectra_frame_update(void); /// Update receiver spectra tab
  void range_doppler_frame_update(void);    /// Update range-doppler tab
  void ambiguity_slice_frame_update(void);  /// Update ambiguity slice tab
  void detection_frame_update(void);        /// Update detection tab
  void settings_frame_update(void);         /// Update settings tab
  /// @}

  /**
   * @brief FFTW planning mutex
   * @details Ensure FFTW plans are generated in a thread safe manner
   */
  std::mutex fftw_plan_mutex;

  /**
   * @brief Config struct
   */
  Config cfg;

  /// Range slice data
  std::vector<double> range_slice;

  /// Speed slice data
  std::vector<double> speed_slice;

  /**
   * @brief Ambiguity scale label & units
   * @details Label will denote dB or linear scale accordingly
   */
  std::string ambiguity_label;

  /// @name Worker threads
  /// @{
  std::thread captureThread;    /// Thread listening to SDR device and updating
                                /// data buffers
  std::thread spectrumThread_A; /// Thread processing receiver A spectra
  std::thread spectrumThread_B; /// Thread processing receiver B spectra
  std::thread ambiguityThread;  /// Thread processing ambiguity surface
  /// @}

  /// Speed step size between ambiguity points
  double speed_step;

  /// Range step size between ambiguity points
  double range_step;

  /// Number of speed points calculated for ambiguity
  int n_speed;

  // TODO: Link to actual SDRplay structs and enums rather than this
  /// @name Settings slider values
  /// @{
  /// AGC bandwidth values
  std::vector<int> agc_bw_vals = {0, 5, 50, 100};
  /// IF filter values
  std::vector<int> if_vals = {0, 450, 1620, 2048};

  /// Receiver bandwidth values
  std::vector<int> bw_vals = {200, 300, 600, 1536, 5000, 6000, 7000, 8000};

  /// LO filter values
  std::vector<std::string> LO_vals = {"Auto", "120", "144", "168"};

  /// LO filter values for display
  std::vector<std::string> LO_disp_vals = {"Auto", "120 MHz", "144 MHz",
                                           "168 MHz"};
  /// Select range slice of ambiguity surface
  int range_slice_slider;

  /// Select speed slice of ambiguity surface
  int speed_slice_slider;

  /// Slider ID of AGC bandwidth value
  int agc_bw_id;

  /// Slider ID of IF filter value
  int IF_id;

  /// Slider ID of reciever bandwidth value
  int bw_id;

  /// Slider ID of LO filter value
  int LO_id;
  /// @}
};
#endif
