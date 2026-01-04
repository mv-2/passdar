#include <GL/gl.h>
#include <GLFW/glfw3.h>
#include <complex.h>
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
  ambiguity.resize(n_range * n_speed);

  // data copy preallocations
  data_a_copy.resize(cfg.process_cfg.buffer_size);
  data_b_copy.resize(cfg.process_cfg.buffer_size);
}

void RadarData::plot_spectra(std::atomic<bool> *exit_flag) {
  // GLFW Window Initialisation
  if (!glfwInit()) {
    return;
  }

  std::vector<double> ambiguity_copy(n_range * n_speed, 0.0);

  // Set GLSL Version
  const char *glsl_version = "#version 130";
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

  // Create window
  float main_scale =
      ImGui_ImplGlfw_GetContentScaleForMonitor(glfwGetPrimaryMonitor());
  GLFWwindow *window =
      glfwCreateWindow((int)(1280 * main_scale), (int)(800 * main_scale),
                       "passdar", nullptr, nullptr);
  if (window == nullptr) {
    return;
  }
  glfwMaximizeWindow(window);
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
    const ImGuiViewport *viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    ImGui::Begin("Spectra");

    // Tab bar of relevant plots
    if (ImGui::BeginTabBar("Data Plots")) {
      // Receiver spectra tab
      if (ImGui::BeginTabItem("Receiver Spectra")) {
        // Receiver subplots
        if (ImPlot::BeginSubplots("Receiver Spectra", 2, 1, ImVec2(-1, -1))) {
          // Receiver A plot
          if (ImPlot::BeginPlot("Receiver A")) {
            ImPlot::SetupAxes("Frequency [kHz]", "Amplitude [dB]");
            stream_a_data->mutex_lock.lock();
            ImPlot::PlotLine("Receiver A", stream_a_data->frequency.data(),
                             stream_a_data->spectrum.data(),
                             stream_a_data->frequency.size());
            stream_a_data->mutex_lock.unlock();
            ImPlot::EndPlot();
          }
          // Receiver B plot
          if (ImPlot::BeginPlot("Receiver B")) {
            ImPlot::SetupAxes("Frequency [kHz]", "Amplitude [dB]");
            stream_b_data->mutex_lock.lock();
            ImPlot::PlotLine("Receiver B", stream_b_data->frequency.data(),
                             stream_b_data->spectrum.data(),
                             stream_b_data->frequency.size());
            stream_b_data->mutex_lock.unlock();
            ImPlot::EndPlot();
          }
          ImPlot::EndSubplots();
        }
        ImGui::EndTabItem();
      }
      // Range - Doppler heatmap tab
      if (ImGui::BeginTabItem("Range - Doppler")) {
        // Range - Doppler plot
        if (ImPlot::BeginPlot("Range - Doppler", ImVec2(-1, -1))) {
          ImPlot::SetupAxes("Speed [m/s]", "Range [m]");
          if (ambiguity_mutex.try_lock()) {
            ImPlot::PlotHeatmap("Range - Doppler", ambiguity.data(), n_speed,
                                n_range, 0, 0, NULL);
            ambiguity_copy = ambiguity;
            ambiguity_mutex.unlock();
          } else {
            ImPlot::PlotHeatmap("Range - Doppler", ambiguity_copy.data(),
                                n_speed, n_range, 0, 0, NULL);
          }
          ImPlot::EndPlot();
        }
        ImGui::EndTabItem();
      }
      ImGui::EndTabBar();
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

void RadarData::process_ambiguity(std::atomic<bool> *exit_flag) {
  // Temp value for storing integral
  std::complex<double> int_temp;

  while (!exit_flag->load()) {
    // await next sample block
    sleep(1);

    // lock iqdata and copy samples
    stream_a_data->data_iq->mutex_lock.lock();
    stream_b_data->data_iq->mutex_lock.lock();
    for (unsigned int i = 0; i < data_a_copy.size(); i++) {
      data_a_copy[i] = stream_a_data->data_iq->samples[i];
      data_b_copy[i] = stream_b_data->data_iq->samples[i];
    }
    stream_a_data->data_iq->mutex_lock.unlock();
    stream_b_data->data_iq->mutex_lock.unlock();

    // Calculate ambiguity
    ambiguity_mutex.lock();
    for (int v_id = 0; v_id < n_speed; v_id++) {
      for (int r_id = 0; r_id < n_range; r_id++) {
        int_temp = 0.0;
        for (unsigned int i = 0; i < (data_a_copy.size() - n_range); i++) {
          int_temp +=
              data_a_copy[i + r_id] * std::conj(data_b_copy[i]) *
              std::polar(1.0, -2 * r_id * M_PI * i / data_a_copy.size());
        }
        ambiguity[v_id * n_range + r_id] =
            std::abs(int_temp) / data_a_copy.size();
      }
    }
    ambiguity_mutex.unlock();
  }
}
