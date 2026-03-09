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

class RadarApp {
public:
  /*
   * RadarApp constructor
   */
  RadarApp(Config cfg);

  /*
   * Default constructor for when no config file is provided
   */
  RadarApp();

  /*
   * Receiver connection and data capture
   */
  Receiver *receiver;

  /*
   * Receiever A data stream
   */
  SpecData *stream_a_data;

  /*
   * Receiever B data stream
   */
  SpecData *stream_b_data;

  /*
   * Radar ambiguity data
   */
  RadarData *radar_data;

  /*
   * Main app running loop
   */
  void run(void);

  // Flag to restart all processing threads
  std::atomic<bool> restart;

private:
  // Object setup function
  void setup(void);

  // Threads
  std::thread captureThread;
  std::thread spectrumThread_A;
  std::thread spectrumThread_B;
  std::thread ambiguityThread;

  // Slider values
  int range_slider;
  int speed_slider;
  int agc_bw_id;
  std::vector<int> agc_bw_vals = {0, 5, 50, 100};
  int IF_id;
  std::vector<int> if_vals = {0, 450, 1620, 2048};
  int bw_id;
  std::vector<int> bw_vals = {200, 300, 600, 1536, 5000, 6000, 7000, 8000};
  int LO_id;
  std::vector<std::string> LO_vals = {"Auto", "120", "144", "168"};
  std::vector<std::string> LO_disp_vals = {"Auto", "120 MHz", "144 MHz",
                                           "168 MHz"};

  // Speed Slice
  std::vector<double> range_slice;
  std::vector<double> speed_slice;
  std::vector<double> speed_vals;
  std::vector<double> range_vals;

  // Ambiguity scale label
  std::string ambiguity_label;

  // Frame update functions
  void update(void);
  void receiver_spectra_frame_update(void);
  void range_doppler_frame_update(void);
  void ambiguity_slice_frame_update(void);
  void settings_frame_update(void);

  // Create ImGui window
  GLFWwindow *init_window(void);

  // Update window
  void update_window(GLFWwindow *window, bool *show_window);

  // Config
  Config cfg;

  // Ambiguity copy for keeping window updated
  std::vector<double> ambiguity_copy;
};
#endif
