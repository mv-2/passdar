#ifndef RADARAPP_H
#define RADARAPP_H

#include <GLFW/glfw3.h>
#include <sdrplay_api.h>
#include <sdrplay_api_tuner.h>
#include <sys/select.h>
#include <thread>
#include <unistd.h>

#include "cfgInterface.h"
#include "imgui.h"
#include "implot.h"
#include "radarData.h"
#include "sdrCapture.h"

class RadarApp {
public:
  /*
   * RadarApp constructor
   */
  RadarApp(Config cfg);

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

private:
  // Threads
  std::thread captureThread;
  std::thread spectrumThread_A;
  std::thread spectrumThread_B;
  std::thread ambiguityThread;

  // Frame upate function
  void update(void);

  // Create ImGui window
  GLFWwindow *init_window(void);

  // Update window
  void update_window(GLFWwindow *window, bool *show_window, ImVec2 hm_bound_min,
                     ImVec2 hm_bound_max, ImPlotHeatmapFlags hm_flags);

  // Config
  Config cfg;

  // Ambiguity copy for keeping window updated
  std::vector<double> ambiguity_copy;
};
#endif
