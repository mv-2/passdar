#include <GL/gl.h>
#include <GLFW/glfw3.h>
#include <complex.h>
#include <iostream>
#include <thread>

#include "../external/imgui/backends/imgui_impl_glfw.h"
#include "../external/imgui/backends/imgui_impl_opengl3.h"
#include "cfgInterface.h"
#include "imgui.h"
#include "implot.h"
#include "radarApp.h"
#include "sdrCapture.h"

const ImVec4 SPECTRUM_LINE_COLOUR = ImVec4(0.2f, 1.0f, 0.6f, 1.0f);
const ImPlotColormap AMBIGUITY_COLOUR_MAP = ImPlotColormap_Jet;
const int COLOURBAR_WIDTH = 100;

RadarApp::RadarApp(Config _cfg) {
  receiver = new Receiver(_cfg.receiver_cfg);
  stream_a_data = new SpecData(_cfg);
  stream_b_data = new SpecData(_cfg);
  radar_data = new RadarData(_cfg, stream_a_data, stream_b_data);
  cfg = _cfg;
  ambiguity_copy.resize(radar_data->n_range * radar_data->n_speed);
}

GLFWwindow *RadarApp::init_window() {
  // Initialise window
  if (!glfwInit()) {
    return nullptr;
  }

  // Set GLSL version
  const char *glsl_version = "#version 130";
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
  glfwWindowHint(GLFW_DECORATED, 1);
  GLFWwindow *window = glfwCreateWindow(1, 1, "passdar", nullptr, nullptr);
  if (window == nullptr) {
    return nullptr;
  }
  glfwMaximizeWindow(window);
  glfwMakeContextCurrent(window);
  glfwSwapInterval(1);

  // ImGui Context
  float main_scale =
      ImGui_ImplGlfw_GetContentScaleForMonitor(glfwGetPrimaryMonitor());
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  static_cast<void>(io);
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
  io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;
  ImGui::StyleColorsDark();
  ImGuiStyle &style = ImGui::GetStyle();
  style.ScaleAllSizes(main_scale);
  style.FontScaleDpi = main_scale;

  // ImPlot context & backend
  ImPlot::CreateContext();
  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init(glsl_version);
  ImPlot::GetStyle().Colormap = AMBIGUITY_COLOUR_MAP;

  return window;
}

void RadarApp::run() {
  // Initialise window
  GLFWwindow *window = init_window();
  if (window == nullptr) {
    std::cerr << "Window is null" << std::endl;
    return;
  }

  // process end flag
  std::atomic<bool> exit_flag(false);

  // Initialise threads
  captureThread = std::thread(
      [&] { receiver->run_capture(stream_a_data, stream_b_data, &exit_flag); });
  spectrumThread_A =
      std::thread([&] { stream_a_data->process_spectrum(&exit_flag); });
  spectrumThread_B =
      std::thread([&] { stream_b_data->process_spectrum(&exit_flag); });
  ambiguityThread =
      std::thread([&] { radar_data->process_ambiguity(&exit_flag); });

  // Window open flag
  bool show_window = true;

  // Heatmap parameters
  ImPlotHeatmapFlags hm_flags = 0;
  ImVec2 hm_bound_min = ImVec2(-radar_data->max_speed, 0);
  ImVec2 hm_bound_max =
      ImVec2(radar_data->max_speed, cfg.process_cfg.max_range);

  while (show_window && !glfwWindowShouldClose(window)) {
    update_window(window, &show_window, hm_bound_min, hm_bound_max, hm_flags);
  }

  // Cleanup
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImPlot::DestroyContext();
  ImGui::DestroyContext();
  glfwDestroyWindow(window);
  glfwTerminate();

  // Join threads to end processes
  exit_flag.store(true);
  ambiguityThread.join();
  spectrumThread_A.join();
  spectrumThread_B.join();
  captureThread.join();
}

void RadarApp::update_window(GLFWwindow *window, bool *show_window,
                             ImVec2 hm_bound_min, ImVec2 hm_bound_max,
                             ImPlotHeatmapFlags hm_flags) {
  glfwPollEvents();

  // ImGui Frame
  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();
  const ImGuiViewport *viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(viewport->Pos);
  ImGui::SetNextWindowSize(viewport->Size);
  ImGui::Begin("Passdar", show_window);

  int heatmap_width = ImGui::GetContentRegionAvail().x - COLOURBAR_WIDTH -
                      ImGui::GetStyle().ItemSpacing.x;

  // Tab bar to select each function
  if (ImGui::BeginTabBar("Passdar")) {
    // Receiver spectra tab
    if (ImGui::BeginTabItem("Receiver Spectra")) {
      // Receiver subplots
      if (ImPlot::BeginSubplots("Receiver Spectra", 2, 1, ImVec2(-1, -1))) {
        // Set line colour separate to heatmap style
        ImPlot::PushStyleColor(ImPlotCol_Line, SPECTRUM_LINE_COLOUR);
        // Receiver A plot
        if (ImPlot::BeginPlot("Receiver A")) {
          ImPlot::SetupAxes("Frequency [MHz]", "Amplitude [dB]");
          stream_a_data->mutex_lock.lock();
          ImPlot::PlotLine("Receiver A", stream_a_data->frequency.data(),
                           stream_a_data->spectrum.data(),
                           stream_a_data->frequency.size());
          stream_a_data->mutex_lock.unlock();
          ImPlot::EndPlot();
        }
        // Receiver B plot
        if (ImPlot::BeginPlot("Receiver B")) {
          ImPlot::SetupAxes("Frequency [MHz]", "Amplitude [dB]");
          stream_b_data->mutex_lock.lock();
          ImPlot::PlotLine("Receiver B", stream_b_data->frequency.data(),
                           stream_b_data->spectrum.data(),
                           stream_b_data->frequency.size());
          stream_b_data->mutex_lock.unlock();
          ImPlot::EndPlot();
        }
        ImPlot::PopStyleColor();
        ImPlot::EndSubplots();
      }
      ImGui::EndTabItem();
    }

    // Range - Doppler heatmap tab
    if (ImGui::BeginTabItem("Range - Doppler")) {
      // Range - Doppler plot

      // Set heatmap style
      ImPlot::PushColormap(AMBIGUITY_COLOUR_MAP);
      if (ImPlot::BeginPlot("Range - Doppler", ImVec2(heatmap_width, -1))) {
        ImPlot::SetupAxes("Speed [m/s]", "Range [m]");
        // Set ambiguity to latest data if available
        if (radar_data->ambiguity_mutex.try_lock()) {
          ambiguity_copy = radar_data->ambiguity;
          radar_data->ambiguity_mutex.unlock();
        }
        // Plot ambiguity copy
        ImPlot::PlotHeatmap("Range - Doppler", ambiguity_copy.data(),
                            radar_data->n_range, radar_data->n_speed, 0, 0,
                            NULL, hm_bound_min, hm_bound_max, hm_flags);
        ImPlot::EndPlot();
      }

      // Display Colorbar
      ImGui::SameLine();
      ImPlot::ColormapScale("Ambiguity [dB]", 0.0f, 70.0f, ImVec2(-1, -1));

      // Pop style stack
      ImPlot::PopColormap();

      ImGui::EndTabItem();
    }

    // Config settings tab
    if (ImGui::BeginTabItem("Settings")) {
      // Config settings
      ImGuiTableFlags table_flags =
          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg;
      if (ImGui::BeginTable("Receiver Settings", 2, table_flags)) {
        ImGui::TableSetupColumn("Receiver Setting");
        ImGui::TableSetupColumn("Value");
        ImGui::TableHeadersRow();

        // centre frequency
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("Centre Frequency");
        ImGui::TableNextColumn();
        ImGui::Text("%d MHz", cfg.receiver_cfg.fc / 1000000);

        // Sample frequency
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("Sample Frequency");
        ImGui::TableNextColumn();
        ImGui::Text("%d kHz", cfg.receiver_cfg.fs / 1000);

        // agc_bandwidth_nr
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("AGC Bandwidth");
        ImGui::TableNextColumn();
        ImGui::Text("%d", cfg.receiver_cfg.agc_bandwidth_nr);

        // agc_set_point_nr
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("AGC Set Point");
        ImGui::TableNextColumn();
        ImGui::Text("%d", cfg.receiver_cfg.agc_set_point_nr);

        // Gain reduction receiver A
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("Gain Reduction Receiver A");
        ImGui::TableNextColumn();
        ImGui::Text("%d dB", cfg.receiver_cfg.gRdB_A);

        // Gain reduction receiver B
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("Gain Reduction Receiver B");
        ImGui::TableNextColumn();
        ImGui::Text("%d dB", cfg.receiver_cfg.gRdB_B);

        // LNA State
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("LNA State");
        ImGui::TableNextColumn();
        ImGui::Text("%d", cfg.receiver_cfg.lna_state);

        // Decimation Factor
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("Decimation Factor");
        ImGui::TableNextColumn();
        ImGui::Text("%d", cfg.receiver_cfg.dec_factor);

        // IF type
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("IF");
        ImGui::TableNextColumn();
        ImGui::Text("%d kHz",
                    cfgInterface::ifNum_map.at(cfg.receiver_cfg.ifType));

        // BW type
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("Bandwidth");
        ImGui::TableNextColumn();
        ImGui::Text("%d MHz",
                    cfgInterface::bwNum_map.at(cfg.receiver_cfg.bwType));

        // LO type
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("LO");
        ImGui::TableNextColumn();
        ImGui::Text(
            "%s MHz",
            cfgInterface::loStr_map.at(cfg.receiver_cfg.loType).c_str());

        // RF Notch
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("RF Notch Filter");
        ImGui::TableNextColumn();
        ImGui::Text("%s",
                    cfg.receiver_cfg.rf_notch_enable ? "Enabled" : "Disabled");

        // DAB Notch
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("DAB Notch Filter");
        ImGui::TableNextColumn();
        ImGui::Text("%s",
                    cfg.receiver_cfg.dab_notch_enable ? "Enabled" : "Disabled");
        ImGui::EndTable();
      }
      if (ImGui::BeginTable("Processing Settings", 2, table_flags)) {
        ImGui::TableSetupColumn("Processing Setting");
        ImGui::TableSetupColumn("Value");
        ImGui::TableHeadersRow();

        // Buffer size
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("Sample Buffer Size");
        ImGui::TableNextColumn();
        ImGui::Text("%d", cfg.process_cfg.buffer_size);

        // Window
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("Window");
        ImGui::TableNextColumn();
        ImGui::Text("%s", cfg.process_cfg._win_str.c_str());

        // Max range
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("Max Range");
        ImGui::TableNextColumn();
        ImGui::Text("%.1lf m", cfg.process_cfg.max_range);

        // Range step
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("Range Step");
        ImGui::TableNextColumn();
        ImGui::Text("%.1lf m", radar_data->range_step);

        // Max Speed
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("Max Speed");
        ImGui::TableNextColumn();
        ImGui::Text("%.1lf m/s", cfg.process_cfg.max_speed);

        // Speed step
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("Speed Step");
        ImGui::TableNextColumn();
        ImGui::Text("%.1lf m/s", radar_data->speed_step);

        ImGui::EndTable();
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
