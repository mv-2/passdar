#include <GL/gl.h>
#include <GLFW/glfw3.h>
#include <cmath>
#include <complex.h>
#include <cstdlib>
#include <vector>

#include "../external/imgui/backends/imgui_impl_glfw.h"
#include "../external/imgui/backends/imgui_impl_opengl3.h"
#include "../external/imgui/imgui.h"
#include "../external/implot/implot.h"
#include "cfgInterface.h"
#include "radarData.h"

// Wave propagation velocity
const double PHASE_VELOCITY = 3e8;

RadarData::RadarData(Config cfg, SpecData *_stream_a_data,
                     SpecData *_stream_b_data) {
  // assign data stream pointers
  stream_a_data = _stream_a_data;
  stream_b_data = _stream_b_data;

  // Sample Frequency
  sample_frequency = cfg.receiver_cfg.fs / cfg.receiver_cfg.dec_factor;

  // Calculate range step
  range_step = PHASE_VELOCITY / sample_frequency;

  // Calculate speed step
  speed_step = cfg.process_cfg.speed_step;

  // num speed points includes positive and negative values + 1 point for 0
  // speed case in centre
  n_speed = 2 * static_cast<int>(cfg.process_cfg.max_speed / speed_step) + 1;

  // range points ignores 0 range case
  n_range = static_cast<int>(cfg.process_cfg.max_range / range_step);

  // preallocate ambiguity
  std::vector<std::vector<double>> amb(n_speed, std::vector<double>(n_range));
  ambiguity = amb;
}

void RadarData::plot_spectra(std::atomic<bool> *exit_flag) {
  // Get axes limits
  double f_min = stream_a_data->frequency.front();
  double f_max = stream_a_data->frequency.back();
  std::cout << "F_MIN: " << f_min << std::endl;

  // GLFW Window Initialisation
  if (!glfwInit()) {
    return;
  }

  // Set GLSL Version
  const char *glsl_version = "#version 130";
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

  // Create window
  float main_scale =
      ImGui_ImplGlfw_GetContentScaleForMonitor(glfwGetPrimaryMonitor());
  GLFWwindow *window =
      glfwCreateWindow((int)(1280 * main_scale), (int)(800 * main_scale),
                       "Test Window", nullptr, nullptr);
  if (window == nullptr) {
    return;
  }
  glfwMakeContextCurrent(window);
  glfwSwapInterval(1);

  // Setup ImGui Context
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  (void)io;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

  // Setup Implot Context
  ImPlot::CreateContext();

  // Dark Mode
  ImGui::StyleColorsDark();

  // Scaling
  ImGuiStyle &style = ImGui::GetStyle();
  style.ScaleAllSizes(main_scale);
  style.FontScaleDpi = main_scale;

  // Platform backend
  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init(glsl_version);

  while (!exit_flag->load() && !glfwWindowShouldClose(window)) {
    glfwPollEvents();

    // ImGui Frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    ImGui::Begin("Spectra");

    // Implot
    if (ImPlot::BeginPlot("Spectra")) {
      // Axes limits
      ImPlot::SetupAxes("Frequency [kHz]", "Amplitude [dB]");
      ImPlot::SetupAxisLimits(ImAxis_X1, f_min, f_max, ImGuiCond_Always);
      // Plot Stream A
      ImPlot::PlotLine("Receiver A", stream_a_data->frequency.data(),
                       stream_a_data->spectrum.data(),
                       stream_a_data->frequency.size());
      stream_a_data->mutex_lock.unlock();
      // Plot Stream B
      ImPlot::PlotLine("Receiver B", stream_b_data->frequency.data(),
                       stream_b_data->spectrum.data(),
                       stream_b_data->frequency.size());
      stream_b_data->mutex_lock.unlock();
      ImPlot::EndPlot();
    }
    ImGui::End();

    // Render
    ImGui::Render();
    int display_w, display_h;
    glfwGetFramebufferSize(window, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    glClearColor(0.2f, 0.3f, 0.4f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    glfwSwapBuffers(window);
  }

  // Cleanup
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImPlot::DestroyContext();
  ImGui::DestroyContext();
  glfwDestroyWindow(window);
  glfwTerminate();

  return;
}

void RadarData::process_ambiguity() {
  // WB ambiguity time scale factor
  double time_scale;

  // Temp value for integration
  std::complex<double> int_temp;

  // delay offset
  unsigned int sample_index;

  // Copy samples out of data streams
  stream_a_data->data_iq->mutex_lock.lock();
  stream_b_data->data_iq->mutex_lock.lock();
  for (int i = 0; i < stream_b_data->max_length; i++) {
    data_a_copy[i] = stream_a_data->data_iq->samples.at(i);
    data_b_copy[i] = stream_b_data->data_iq->samples.at(i);
  }
  stream_a_data->data_iq->mutex_lock.unlock();
  stream_b_data->data_iq->mutex_lock.unlock();

  // Calculate ambiguity surface
  for (int v_id = -n_speed / 2; v_id < n_speed / 2; v_id++) {
    time_scale = (PHASE_VELOCITY + v_id * speed_step) /
                 (PHASE_VELOCITY - v_id * speed_step);
    for (int r_id = -n_range / 2; r_id < n_range / 2; r_id++) {
      int_temp = 0.0;
      for (int i = 0; i < data_a_copy.size(); i++) {
        sample_index = static_cast<unsigned int>(time_scale * (i - r_id));
        int_temp =
            int_temp + data_a_copy[i] * std::conj(data_b_copy[sample_index]);
      }
      int_temp = std::sqrt(std::abs(time_scale)) * int_temp /
                 static_cast<double>(sample_frequency);
      ambiguity[v_id][r_id] = std::abs(int_temp);
    }
  }
}

void RadarData::plot_ambiguity(std::atomic<bool> *exit_flag) {
  // Initialise plot window
  FILE *plot_pipe = popen("gnuplot -persist", "w");

  while (!(exit_flag->load())) {
    sleep(1);

    // Calculate ambiguity surface
    process_ambiguity();
  }
}
