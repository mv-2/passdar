#include <GL/gl.h>
#include <GLFW/glfw3.h>
#include <complex.h>
#include <iostream>
#include <thread>

#include "../../external/imgui/backends/imgui_impl_glfw.h"
#include "../../external/imgui/backends/imgui_impl_opengl3.h"
#include "../hardwareInterface/cfgInterface.h"
#include "../hardwareInterface/sdrCapture.h"
#include "imgui.h"
#include "implot.h"
#include "radarApp.h"

// Display constants
const ImPlotColormap SPECTRUM_COLOUR_MAP = ImPlotColormap_Spectral;
const ImPlotColormap AMBIGUITY_COLOUR_MAP = ImPlotColormap_Jet;
const int COLOURBAR_WIDTH = 100;

RadarApp::RadarApp() : cfg(Config()) {
  // Assign object fields from default config
  setup();
}

RadarApp::RadarApp(Config _cfg) {
  // config
  cfg = _cfg;

  // Assign object fields from default config
  setup();
}

void RadarApp::setup(void) {
  // Initialise capture & processing objects
  receiver = new Receiver(cfg.receiver_cfg);
  stream_a_data = new SpecData(cfg);
  stream_b_data = new SpecData(cfg);
  radar_data = new RadarData(cfg, stream_a_data, stream_b_data);

  // Preallocate vectors
  ambiguity_copy.resize(radar_data->ambiguity_columns * radar_data->n_speed);
  range_slice.resize(radar_data->ambiguity_columns);
  speed_slice.resize(radar_data->n_range);

  // Initialise slider values
  range_slider = 0;
  speed_slider = 0;

  // Calculate range values
  for (int i = 0; i < radar_data->n_range; i++) {
    range_vals.push_back(static_cast<double>(i) * radar_data->range_step);
  }

  // Calculate speed values
  for (int i = -radar_data->n_speed; i < radar_data->n_speed; i++) {
    speed_vals.push_back(static_cast<double>(i) * radar_data->speed_step);
  }

  // Set ambiguity scales label
  switch (cfg.process_cfg.ambiguity_scale) {
  case DisplayScale::Linear:
    ambiguity_label = "Ambiguity [-]";
    break;
  case DisplayScale::dB:
    ambiguity_label = "Ambiguity [dB]";
  };
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

  // Run and update frames
  while (show_window && !glfwWindowShouldClose(window)) {
    update_window(window, &show_window);
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

void RadarApp::receiver_spectra_frame_update(void) {
  // Receiver subplots
  if (ImPlot::BeginSubplots("Receiver Spectra", 2, 1, ImVec2(-1, -1),
                            ImPlotSubplotFlags_LinkAllX |
                                ImPlotSubplotFlags_LinkAllY)) {
    // Receiver A plot
    ImPlot::PushColormap(SPECTRUM_COLOUR_MAP);
    if (ImPlot::BeginPlot("Receiver A", ImVec2(-1, 0), ImPlotFlags_NoLegend)) {

      // Setup and labels
      ImPlot::SetupAxes("Frequency [MHz]", "Amplitude [dB]");
      stream_a_data->mutex_lock.lock();

      // Plot
      ImPlot::PlotLine("Receiver A", stream_a_data->frequency.data(),
                       stream_a_data->spectrum.data(),
                       stream_a_data->frequency.size());
      stream_a_data->mutex_lock.unlock();

      ImPlot::EndPlot();
    }
    // Receiver B plot
    if (ImPlot::BeginPlot("Receiver B", ImVec2(-1, 0), ImPlotFlags_NoLegend)) {

      // Setup and labels
      ImPlot::SetupAxes("Frequency [MHz]", "Amplitude [dB]");
      stream_b_data->mutex_lock.lock();

      // Plot
      ImPlot::PlotLine("Receiver B", stream_b_data->frequency.data(),
                       stream_b_data->spectrum.data(),
                       stream_b_data->frequency.size());
      stream_b_data->mutex_lock.unlock();

      ImPlot::EndPlot();
    }

    ImPlot::PopColormap();
    ImPlot::EndSubplots();
  }
}

void RadarApp::range_doppler_frame_update(void) {
  int plot_width = ImGui::GetContentRegionAvail().x - COLOURBAR_WIDTH -
                   ImGui::GetStyle().ItemSpacing.x;
  // Set heatmap style
  ImPlot::PushColormap(AMBIGUITY_COLOUR_MAP);

  // Range - Doppler plot
  if (ImPlot::BeginPlot("Range - Doppler", ImVec2(plot_width, -1),
                        ImPlotFlags_NoLegend)) {
    ImPlot::SetupAxes("Speed [m/s]", "Range [m]");

    // Set ambiguity to latest data if available
    if (radar_data->ambiguity_mutex.try_lock()) {
      ambiguity_copy = radar_data->ambiguity;
      radar_data->ambiguity_mutex.unlock();
    }

    // Plot ambiguity copy
    ImPlot::PlotHeatmap(
        "Range - Doppler", ambiguity_copy.data(), radar_data->n_range,
        radar_data->ambiguity_columns, cfg.process_cfg.ambiguity_lims[0],
        cfg.process_cfg.ambiguity_lims[1], NULL, {-radar_data->max_speed, 0},
        {radar_data->max_speed, cfg.process_cfg.max_range});

    ImPlot::EndPlot();
  }

  // Display Colorbar
  ImGui::SameLine();
  ImPlot::ColormapScale(ambiguity_label.c_str(),
                        cfg.process_cfg.ambiguity_lims[0],
                        cfg.process_cfg.ambiguity_lims[1], ImVec2(-1, -1));

  // Pop style stack
  ImPlot::PopColormap();
}

void RadarApp::ambiguity_slice_frame_update(void) {

  int window_width = ImGui::GetContentRegionAvail().x;
  int half_width =
      ImGui::GetContentRegionAvail().x / 2 - ImGui::GetStyle().ItemSpacing.x;
  // Reload slices when available
  if (radar_data->ambiguity_mutex.try_lock()) {

    // Assign speed values at range point
    for (int i = 0; i < radar_data->ambiguity_columns; i++) {
      range_slice[i] =
          radar_data
              ->ambiguity[range_slider * radar_data->ambiguity_columns + i];
    }

    // Assign range values at speed point
    for (int i = 0; i < radar_data->n_range; i++) {
      speed_slice[i] =
          radar_data->ambiguity[i * radar_data->ambiguity_columns +
                                speed_slider + radar_data->n_speed];
    }

    radar_data->ambiguity_mutex.unlock();
  }

  // Range slider label
  std::string label_text = std::format("Range Selection: %.0f [m]",
                                       range_slider * radar_data->range_step);
  float text_width = ImGui::CalcTextSize(label_text.c_str()).x;
  ImGui::SetCursorPosX((window_width - text_width) / 4);
  ImGui::Text("Range Selection: %.0f [m]",
              range_slider * radar_data->range_step);
  ImGui::SameLine();

  // Speed slider label
  label_text = std::format("Speed Selection: %.1f [m/s]",
                           range_slider * radar_data->range_step);
  text_width = ImGui::CalcTextSize(label_text.c_str()).x;
  ImGui::SetCursorPosX((window_width - text_width) * 3 / 4);
  ImGui::Text("Speed Selection: %.1f [m/s]",
              speed_slider * radar_data->speed_step);

  // Range slider
  ImGui::PushItemWidth(half_width);
  ImGui::SliderInt("##Range ID", &range_slider, 0, radar_data->n_range - 1, "");

  ImGui::PopItemWidth();

  // Speed Slider
  ImGui::SameLine();
  ImGui::PushItemWidth(half_width);
  ImGui::SliderInt("##Speed ID", &speed_slider, -radar_data->n_speed,
                   radar_data->n_speed, "");
  ImGui::PopItemWidth();

  // Slice Subplots
  if (ImPlot::BeginSubplots("Ambiguity Slices", 2, 1, ImVec2(-1, -1))) {
    // Range Slice plot
    ImPlot::PushColormap(SPECTRUM_COLOUR_MAP);
    if (ImPlot::BeginPlot("Range Slice", ImVec2(-1, -1),
                          ImPlotFlags_NoLegend)) {

      // Axes limits and labels
      ImPlot::SetupAxes("Speed [m/s]", ambiguity_label.c_str());
      ImPlot::SetupAxisLimits(ImAxis_X1, speed_vals.front(), speed_vals.back(),
                              ImGuiCond_Always);
      ImPlot::SetupAxisLimits(ImAxis_Y1, cfg.process_cfg.ambiguity_lims[0],
                              cfg.process_cfg.ambiguity_lims[1],
                              ImGuiCond_Always);

      // Plot
      ImPlot::PlotLine("Range Slice", speed_vals.data(), range_slice.data(),
                       range_slice.size());

      ImPlot::EndPlot();
    }

    // Speed Slice plot
    if (ImPlot::BeginPlot("Speed Slice", ImVec2(-1, -1),
                          ImPlotFlags_NoLegend)) {

      // Axes limits and labels
      ImPlot::SetupAxes("Range [m]", ambiguity_label.c_str());
      ImPlot::SetupAxisLimits(ImAxis_X1, range_vals.front(), range_vals.back(),
                              ImGuiCond_Always);
      ImPlot::SetupAxisLimits(ImAxis_Y1, cfg.process_cfg.ambiguity_lims[0],
                              cfg.process_cfg.ambiguity_lims[1],
                              ImGuiCond_Always);

      // plot
      ImPlot::PlotLine("Speed Slice", range_vals.data(), speed_slice.data(),
                       speed_slice.size());

      ImPlot::EndPlot();
    }

    ImPlot::PopColormap();
    ImPlot::EndSubplots();
  }
}

void RadarApp::settings_frame_update(void) {

  // Config settings
  ImGuiTableFlags table_flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg;
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
    ImGui::Text("%d kHz", cfgInterface::ifNum_map.at(cfg.receiver_cfg.ifType));

    // BW type
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::Text("Bandwidth");
    ImGui::TableNextColumn();
    ImGui::Text("%d MHz", cfgInterface::bwNum_map.at(cfg.receiver_cfg.bwType));

    // LO type
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::Text("LO");
    ImGui::TableNextColumn();
    ImGui::Text("%s MHz",
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
}

void RadarApp::update_window(GLFWwindow *window, bool *show_window) {

  glfwPollEvents();

  // ImGui Frame
  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();
  const ImGuiViewport *viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(viewport->Pos);
  ImGui::SetNextWindowSize(viewport->Size);
  ImGui::Begin("Passdar", show_window);

  // Calculate required area values

  // Tab bar to select each function
  if (ImGui::BeginTabBar("Passdar")) {

    // Receiver spectra tab
    if (ImGui::BeginTabItem("Receiver Spectra")) {
      receiver_spectra_frame_update();
      ImGui::EndTabItem();
    }

    // Range - Doppler heatmap tab
    if (ImGui::BeginTabItem("Range - Doppler")) {
      range_doppler_frame_update();
      ImGui::EndTabItem();
    }

    // Ambiguity slices
    if (ImGui::BeginTabItem("Ambiguity Slices")) {
      ambiguity_slice_frame_update();
      ImGui::EndTabItem();
    }

    // Config settings tab
    if (ImGui::BeginTabItem("Settings")) {
      settings_frame_update();
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
