#include <GL/gl.h>
#include <GLFW/glfw3.h>
#include <complex.h>
#include <iostream>
#include <thread>

#include "../hardwareInterface/cfgInterface.h"
#include "../hardwareInterface/sdrCapture.h"
#include "../processing/radarData.h"
#include "../util/leftRightData.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "implot.h"
#include "radarApp.h"

// Display constants
const ImPlotColormap SPECTRUM_COLOUR_MAP = ImPlotColormap_Spectral;
const ImPlotColormap AMBIGUITY_COLOUR_MAP = ImPlotColormap_Jet;
const ImPlotColormap DETECTION_COLOUR_MAP = ImPlotColormap_Greys;
const int COLOURBAR_WIDTH = 100;

RadarApp::RadarApp() : cfg(Config()) {}

RadarApp::RadarApp(Config _cfg) {
  // config
  cfg = _cfg;
}

void RadarApp::setup(void) {
  // Initialise capture & processing objects
  receiver = new Receiver(cfg.receiver_cfg);
  stream_a_data = new SpecData(cfg);
  stream_b_data = new SpecData(cfg);
  radar_data = new RadarData(cfg, stream_a_data, stream_b_data);

  // Preallocate vectors
  range_slice.resize(radar_data->ambiguity_columns);
  speed_slice.resize(radar_data->n_range);

  // Initialise slider values
  range_slice_slider = 0;
  speed_slice_slider = 0;

  // Set ambiguity scales label
  switch (cfg.process_cfg.ambiguity_scale) {
  case DisplayScale::Linear:
    ambiguity_label = "Ambiguity [-]";
    break;
  case DisplayScale::dB:
    ambiguity_label = "Ambiguity [dB]";
  };

  // Set flag
  restart.store(false);
}

void spinner(float radius, ImVec2 pos) {
  // ImGui spinner for initialisation

  // Set size and drawing canvas
  ImVec2 size = ImVec2(radius * 2, radius * 2);
  ImGui::Dummy(size);
  ImDrawList *DrawList = ImGui::GetWindowDrawList();
  DrawList->PathClear();

  // Draw
  int num_segments = 30;
  float start = fabsf(sinf(ImGui::GetTime() * 1.8f) * (num_segments - 5));
  float a_min = M_PI * 2.0f * start / num_segments;
  float a_max = M_PI * 2.0f * (num_segments - 3) / num_segments;
  for (int i = 0; i <= num_segments; i++) {
    float a = a_min + (i / (float)num_segments) * (a_max - a_min);
    DrawList->PathLineTo(
        ImVec2(pos.x + cosf(a + ImGui::GetTime() * 8) * radius,
               pos.y + sinf(a + ImGui::GetTime() * 8) * radius));
  }

  DrawList->PathStroke(IM_COL32(255, 255, 255, 255), 1.0, ImDrawListFlags_None);

  // Centred text
  ImVec2 text_size = ImGui::CalcTextSize("Initialising");
  ImGui::SetCursorPos({pos.x - text_size.x / 2, pos.y - text_size.y / 2});
  ImGui::Text("Initialising");
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
  std::atomic<bool> exit_flag;

  // Window open flag
  bool show_window = true;

  // Mutex to ensure thread safety when generating fftw plans
  std::mutex fftw_plan_mutex;

  // Run and restart until window closed
  while (show_window && !glfwWindowShouldClose(window)) {
    // reset all fields
    exit_flag.store(false);
    setup();

    // Initialise threads
    captureThread = std::thread([&] {
      receiver->run_capture(stream_a_data, stream_b_data, &exit_flag);
    });
    spectrumThread_A = std::thread(
        [&] { stream_a_data->process_spectrum(&exit_flag, &fftw_plan_mutex); });
    spectrumThread_B = std::thread(
        [&] { stream_b_data->process_spectrum(&exit_flag, &fftw_plan_mutex); });
    ambiguityThread = std::thread(
        [&] { radar_data->radar_process(&exit_flag, &fftw_plan_mutex); });

    // Update to rounded settings
    // NOTE: This isn't thread safe which is fine in this case unless computer
    // too fast
    cfg.process_cfg.max_range = radar_data->range_vals.back();
    speed_step = radar_data->speed_step;
    n_speed = radar_data->n_speed;
    range_step = radar_data->range_step;

    // Assign ids for sliders
    std::string lo_str = cfgInterface::loStr_map.at(cfg.receiver_cfg.loType);
    for (unsigned int i = 0; i < LO_vals.size(); i++) {
      if (strcmp(LO_vals[i].c_str(), lo_str.c_str()) == 0) {
        LO_id = i;
        break;
      }
    }
    int bw_num = cfgInterface::bwNum_map.at(cfg.receiver_cfg.bwType);
    for (unsigned int i = 0; i < bw_vals.size(); i++) {
      if (bw_vals[i] == bw_num) {
        bw_id = i;
        break;
      }
    }
    int if_num = cfgInterface::ifNum_map.at(cfg.receiver_cfg.ifType);
    for (unsigned int i = 0; i < if_vals.size(); i++) {
      if (if_vals[i] == if_num) {
        IF_id = i;
        break;
      }
    }

    // Run and update frames
    while (show_window && !glfwWindowShouldClose(window) && !restart.load()) {
      update_window(window, &show_window);
    }

    // Join threads to end processes
    exit_flag.store(true);
    ambiguityThread.join();
    spectrumThread_A.join();
    spectrumThread_B.join();
    captureThread.join();
  }

  // Cleanup
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImPlot::DestroyContext();
  ImGui::DestroyContext();
  glfwDestroyWindow(window);
  glfwTerminate();
}

void RadarApp::receiver_spectra_frame_update(void) {
  float fps = ImGui::GetIO().Framerate;
  ImGui::Text("Window FPS  %.1f", fps);
  // Receiver subplots
  if (ImPlot::BeginSubplots("Receiver Spectra", 2, 1, ImVec2(-1, -1),
                            ImPlotSubplotFlags_LinkAllX |
                                ImPlotSubplotFlags_LinkAllY)) {
    // Receiver A plot
    ImPlot::PushColormap(SPECTRUM_COLOUR_MAP);
    ImPlotSpec stair_spec;
    stair_spec.FillAlpha = 0.5f;
    stair_spec.Flags = ImPlotStairsFlags_Shaded;
    if (ImPlot::BeginPlot("Receiver A", ImVec2(-1, 0), ImPlotFlags_NoLegend)) {

      // Setup and labels
      ImPlot::SetupAxes("Frequency [MHz]", "Amplitude [dB]");
      ImPlot::SetupAxisLimits(ImAxis_X1, stream_a_data->frequency.front(),
                              stream_a_data->frequency.back(), ImPlotCond_Once);
      ImPlot::SetupAxisLimits(ImAxis_Y1, 30, 120, ImPlotCond_Once);

      // Plot
      stream_a_data->mutex_lock.lock();

      ImPlot::PlotStairs("Receiver A", stream_a_data->frequency.data(),
                         stream_a_data->spectrum.data(),
                         stream_a_data->frequency.size(), stair_spec);
      stream_a_data->mutex_lock.unlock();

      ImPlot::EndPlot();
    }

    // Receiver B plot
    if (ImPlot::BeginPlot("Receiver B", ImVec2(-1, 0), ImPlotFlags_NoLegend)) {

      // Setup and labels
      ImPlot::SetupAxes("Frequency [MHz]", "Amplitude [dB]");
      ImPlot::SetupAxisLimits(ImAxis_X1, stream_b_data->frequency.front(),
                              stream_b_data->frequency.back(), ImPlotCond_Once);
      ImPlot::SetupAxisLimits(ImAxis_Y1, 30, 120, ImPlotCond_Once);

      // Plot
      stream_b_data->mutex_lock.lock();
      ImPlot::PlotStairs("Receiver B", stream_b_data->frequency.data(),
                         stream_b_data->spectrum.data(),
                         stream_b_data->frequency.size(), stair_spec);
      stream_b_data->mutex_lock.unlock();

      ImPlot::EndPlot();
    }

    ImPlot::PopColormap();
    ImPlot::EndSubplots();
  }
}

void RadarApp::range_doppler_frame_update(void) {
  float fps = ImGui::GetIO().Framerate;
  ImGui::Text("Window FPS %.1f, Ambiguity FPS %.2f", fps,
              radar_data->ambiguity_rate);
  int plot_width = ImGui::GetContentRegionAvail().x - COLOURBAR_WIDTH -
                   ImGui::GetStyle().ItemSpacing.x;
  // Set heatmap style
  ImPlot::PushColormap(AMBIGUITY_COLOUR_MAP);

  // Range - Doppler plot
  if (ImPlot::BeginPlot("Range - Doppler", ImVec2(plot_width, -1),
                        ImPlotFlags_NoLegend)) {
    ImPlot::SetupAxes("Speed [m/s]", "Range [m]");

    // Plot ambiguity copy
    ImPlot::PlotHeatmap("Range - Doppler", radar_data->ambiguity.read().data(),
                        radar_data->n_range, radar_data->ambiguity_columns,
                        cfg.process_cfg.ambiguity_lims[0],
                        cfg.process_cfg.ambiguity_lims[1], NULL,
                        {-radar_data->max_speed, 0},
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
  float fps = ImGui::GetIO().Framerate;
  ImGui::Text("Window FPS  %.1f", fps);
  // Get available areas
  int window_width = ImGui::GetContentRegionAvail().x;
  int half_width =
      ImGui::GetContentRegionAvail().x / 2 - ImGui::GetStyle().ItemSpacing.x;

  // TEST: is this sane?
  // Assign speed values at range point
  for (int i = 0; i < radar_data->ambiguity_columns; i++) {
    range_slice[i] = radar_data->ambiguity.read(
        range_slice_slider * radar_data->ambiguity_columns + i);
  }

  // Assign range values at speed point
  for (int i = 0; i < radar_data->n_range; i++) {
    speed_slice[i] =
        radar_data->ambiguity.read(i * radar_data->ambiguity_columns +
                                   speed_slice_slider + radar_data->n_speed);
  }

  // Range slider label
  std::string label_text = std::format(
      "Range Selection: %.0f [m]", range_slice_slider * radar_data->range_step);
  float text_width = ImGui::CalcTextSize(label_text.c_str()).x;
  ImGui::SetCursorPosX((window_width - text_width) / 4);
  ImGui::Text("Range Selection: %.0f [m]",
              range_slice_slider * radar_data->range_step);
  ImGui::SameLine();

  // Speed slider label
  label_text = std::format("Speed Selection: %.1f [m/s]",
                           range_slice_slider * radar_data->range_step);
  text_width = ImGui::CalcTextSize(label_text.c_str()).x;
  ImGui::SetCursorPosX((window_width - text_width) * 3 / 4);
  ImGui::Text("Speed Selection: %.1f [m/s]",
              speed_slice_slider * radar_data->speed_step);

  // Range slider
  ImGui::PushItemWidth(half_width);
  ImGui::SliderInt("##Range ID", &range_slice_slider, 0,
                   radar_data->n_range - 1, "");

  ImGui::PopItemWidth();

  // Speed Slider
  ImGui::SameLine();
  ImGui::PushItemWidth(half_width);
  ImGui::SliderInt("##Speed ID", &speed_slice_slider, -radar_data->n_speed,
                   radar_data->n_speed, "");
  ImGui::PopItemWidth();

  // Slice Subplots
  if (ImPlot::BeginSubplots("Ambiguity Slices", 2, 1, ImVec2(-1, -1))) {
    ImPlotSpec stair_spec;
    stair_spec.FillAlpha = 0.5f;
    stair_spec.Flags = ImPlotStairsFlags_Shaded;
    // Range Slice plot
    ImPlot::PushColormap(SPECTRUM_COLOUR_MAP);
    if (ImPlot::BeginPlot("Range Slice", ImVec2(-1, -1),
                          ImPlotFlags_NoLegend)) {

      // Axes limits and labels
      ImPlot::SetupAxes("Speed [m/s]", ambiguity_label.c_str());
      ImPlot::SetupAxisLimits(ImAxis_X1, radar_data->speed_vals.front(),
                              radar_data->speed_vals.back(), ImGuiCond_Always);
      ImPlot::SetupAxisLimits(ImAxis_Y1, cfg.process_cfg.ambiguity_lims[0],
                              cfg.process_cfg.ambiguity_lims[1],
                              ImGuiCond_Once);

      // Plot
      ImPlot::PlotStairs("Range Slice", radar_data->speed_vals.data(),
                         range_slice.data(), range_slice.size(), stair_spec);

      ImPlot::EndPlot();
    }

    // Speed Slice plot
    if (ImPlot::BeginPlot("Speed Slice", ImVec2(-1, -1),
                          ImPlotFlags_NoLegend)) {

      // Axes limits and labels
      ImPlot::SetupAxes("Range [m]", ambiguity_label.c_str());
      ImPlot::SetupAxisLimits(ImAxis_X1, radar_data->range_vals.front(),
                              radar_data->range_vals.back(), ImGuiCond_Always);
      ImPlot::SetupAxisLimits(ImAxis_Y1, cfg.process_cfg.ambiguity_lims[0],
                              cfg.process_cfg.ambiguity_lims[1],
                              ImGuiCond_Once);

      // plot
      ImPlot::PlotStairs("Speed Slice", radar_data->range_vals.data(),
                         speed_slice.data(), speed_slice.size(), stair_spec);

      ImPlot::EndPlot();
    }

    ImPlot::PopColormap();
    ImPlot::EndSubplots();
  }
}

void RadarApp::settings_frame_update(void) {
  float fps = ImGui::GetIO().Framerate;
  ImGui::Text("Window FPS  %.1f", fps);

  // Config settings
  ImGuiTableFlags table_flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg;
  if (ImGui::BeginTable("Settings", 3, table_flags)) {

    ImGui::TableSetupColumn("Receiver");
    ImGui::TableSetupColumn("Processing");
    ImGui::TableSetupColumn("Detection");
    ImGui::TableHeadersRow();

    // Row 1
    // centre frequency
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    if (ImGui::InputInt("Centre Frequency [Hz]", &cfg.receiver_cfg.fc, 100000,
                        100000)) {
      // Ensure speed steps are consistent with buffer size and sample rate
      speed_step = (static_cast<double>(cfg.receiver_cfg.fs) /
                    cfg.process_cfg.buffer_size) *
                   PHASE_VELOCITY / (2 * cfg.receiver_cfg.fc);

      // Set exact max_speed
      cfg.process_cfg.max_speed = n_speed * speed_step;
    }

    // Buffer size
    ImGui::TableNextColumn();
    ImGui::InputInt("Sample Buffer Length", &cfg.process_cfg.buffer_size, 1,
                    1000);
    if (ImGui::IsItemDeactivatedAfterEdit()) {
      // Ensure buffer is divisible by n_speed
      cfg.process_cfg.buffer_size -= cfg.process_cfg.buffer_size % n_speed;

      // Ensure speed steps are consistent with buffer size and sample rate
      speed_step = (static_cast<double>(cfg.receiver_cfg.fs) /
                    cfg.process_cfg.buffer_size) *
                   PHASE_VELOCITY / (2 * cfg.receiver_cfg.fc);

      // Set exact max_speed
      cfg.process_cfg.max_speed = n_speed * speed_step;
    }

    // CFAR multiplier
    ImGui::TableNextColumn();
    ImGui::InputDouble("CFAR Multiplier", &cfg.detection_config.cfar_multiplier,
                       0.05, 0.1, "%.3f");

    // Row 2
    // Sample frequency
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    if (ImGui::InputInt("Sample Frequency [Hz]", &cfg.receiver_cfg.fs, 1, 1,
                        ImGuiInputTextFlags_ReadOnly)) {
      // Ensure range variables are consistent
      update_range_vars();

      // Ensure speed steps are consistent with buffer size and sample rate
      speed_step = (static_cast<double>(cfg.receiver_cfg.fs) /
                    cfg.process_cfg.buffer_size) *
                   PHASE_VELOCITY / (2 * cfg.receiver_cfg.fc);

      // Set number of speed steps
      n_speed = cfg.process_cfg.max_speed / speed_step;

      // Set exact max_speed
      cfg.process_cfg.max_speed = n_speed * speed_step;
    }

    // DFT Window
    ImGui::TableNextColumn();
    ImGui::Text("DFT window");
    ImGui::SameLine();
    if (ImGui::Button(cfg.process_cfg.dft_window == DftWindow::Hanning
                          ? "Hanning"
                          : "Rectangular")) {
      switch (cfg.process_cfg.dft_window) {
      case DftWindow::Hanning: {
        cfg.process_cfg.dft_window = DftWindow::Rectangular;
        break;
      }
      case DftWindow::Rectangular: {
        cfg.process_cfg.dft_window = DftWindow::Hanning;
        break;
      }
      }
    }

    // Range window size
    ImGui::TableNextColumn();
    ImGui::InputInt("Range Window Size", &cfg.detection_config.range_window, 1,
                    1);
    if (ImGui::IsItemDeactivatedAfterEdit()) {
      cfg.detection_config.range_window =
          std::max(cfg.detection_config.range_window, 1);
      cfg.detection_config.range_guard =
          std::min(cfg.detection_config.range_guard,
                   cfg.detection_config.range_window - 1);
    }

    // Row 3
    // agc_bandwidth_nr
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::SliderInt("##AGC BW Slider", &agc_bw_id, 0, agc_bw_vals.size() - 1,
                     "");
    if (ImGui::IsItemDeactivatedAfterEdit()) {
      cfg.receiver_cfg.agc_bandwidth_nr = agc_bw_vals[agc_bw_id];
    }
    ImGui::SameLine();
    ImGui::Text("AGC Bandwidth: %d Hz", agc_bw_vals[agc_bw_id]);

    // Max range
    ImGui::TableNextColumn();
    ImGui::InputDouble("Maximum range [m]", &cfg.process_cfg.max_range, 500.0,
                       1000.0);
    if (ImGui::IsItemDeactivatedAfterEdit()) {
      // Ensure range variables are consistent
      update_range_vars();
    }

    // Range guard cells
    ImGui::TableNextColumn();
    ImGui::InputInt("Range Guard Size", &cfg.detection_config.range_guard, 1,
                    1);
    if (ImGui::IsItemDeactivatedAfterEdit()) {
      cfg.detection_config.range_guard =
          std::clamp(cfg.detection_config.range_guard, 0,
                     cfg.detection_config.range_window - 1);
    }

    // Row 4
    // agc_set_point_nr
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::InputInt("AGC Set Point [dB]", &cfg.receiver_cfg.agc_set_point_nr, 1,
                    1);

    // Range step
    ImGui::TableNextColumn();
    ImGui::InputDouble("Range Step [m]", &range_step, 1, 1, "%.1f",
                       ImGuiInputTextFlags_ReadOnly);

    // Speed window size
    ImGui::TableNextColumn();
    ImGui::InputInt("Speed Window Size", &cfg.detection_config.speed_window, 1,
                    1);
    if (ImGui::IsItemDeactivatedAfterEdit()) {
      cfg.detection_config.speed_window =
          std::max(cfg.detection_config.speed_window, 1);
      cfg.detection_config.speed_guard =
          std::min(cfg.detection_config.speed_guard,
                   cfg.detection_config.speed_window - 1);
    }

    // Row 5
    // Gain reduction receiver A
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::InputInt("Receiver A gain reduction [dB]", &cfg.receiver_cfg.gRdB_A,
                    1, 1);

    // Max speed
    ImGui::TableNextColumn();
    ImGui::InputDouble("Maximum Speed [m/s]", &cfg.process_cfg.max_speed, 1.0,
                       10.0);
    if (ImGui::IsItemDeactivatedAfterEdit()) {
      // Set number of speed steps
      n_speed = cfg.process_cfg.max_speed / speed_step;

      // Set exact max_speed
      cfg.process_cfg.max_speed = n_speed * speed_step;

      // Ensure buffer_size is still consistent with n_speed
      cfg.process_cfg.buffer_size =
          n_speed * std::round(cfg.process_cfg.buffer_size / n_speed);
    }

    // Speed Guard Cells
    ImGui::TableNextColumn();
    ImGui::InputInt("Speed Guard Size", &cfg.detection_config.speed_guard, 1,
                    1);
    if (ImGui::IsItemDeactivatedAfterEdit()) {
      cfg.detection_config.speed_guard =
          std::clamp(cfg.detection_config.speed_guard, 0,
                     cfg.detection_config.speed_window - 1);
    }

    // Row 6
    // Gain reduction receiver B
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::InputInt("Receiver B gain reduction [dB]", &cfg.receiver_cfg.gRdB_B,
                    1, 1);

    // Speed Step
    ImGui::TableNextColumn();
    ImGui::InputDouble("Speed Step [m/s]", &speed_step, 1, 1, "%.2f",
                       ImGuiInputTextFlags_ReadOnly);

    // Row 7
    // LNA State
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::SliderInt("## LNA State", &cfg.receiver_cfg.lna_state, 0, 9, "%d");
    ImGui::SameLine();
    ImGui::Text("LNA State: %d", cfg.receiver_cfg.lna_state);

    // Ambiguity scale
    ImGui::TableNextColumn();
    if (ImGui::Button(cfg.process_cfg.ambiguity_scale == DisplayScale::Linear
                          ? "Linear Ambiguity Scale"
                          : "dB Ambiguity Scale")) {
      switch (cfg.process_cfg.ambiguity_scale) {
      case DisplayScale::Linear:
        cfg.process_cfg.ambiguity_scale = DisplayScale::dB;
        break;
      case DisplayScale::dB:
        cfg.process_cfg.ambiguity_scale = DisplayScale::Linear;
        break;
      }
    }

    // Row 8
    // Decimation Factor
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::InputInt("Decimation Factor", &cfg.receiver_cfg.dec_factor, 1, 1);
    if (ImGui::IsItemDeactivatedAfterEdit()) {
      cfg.receiver_cfg.dec_factor =
          std::clamp(cfg.receiver_cfg.dec_factor, 1, 20);
      cfg.receiver_cfg.fs =
          SAMPLE_FREQUENCY_DEFAULT / cfg.receiver_cfg.dec_factor;
      // Ensure Range variables are consistent
      update_range_vars();

      // Ensure speed steps are consistent with buffer size and sample rate
      speed_step = (static_cast<double>(cfg.receiver_cfg.fs) /
                    cfg.process_cfg.buffer_size) *
                   PHASE_VELOCITY / (2 * cfg.receiver_cfg.fc);

      // Set number of speed steps
      n_speed = cfg.process_cfg.max_speed / speed_step;

      // Set exact max_speed
      cfg.process_cfg.max_speed = n_speed * speed_step;

      // Ensure buffer_size is still consistent with n_speed
      cfg.process_cfg.buffer_size -= cfg.process_cfg.buffer_size % n_speed;
    }

    // Ambiguity scale min
    ImGui::TableNextColumn();
    ImGui::InputDouble("Ambiguity Minimum", &cfg.process_cfg.ambiguity_lims[0],
                       1, 100, "%.1f");

    // Row 9
    // IF type
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::SliderInt("##IF BW Slider", &IF_id, 0, if_vals.size() - 1, "");
    if (ImGui::IsItemDeactivatedAfterEdit()) {
      cfg.receiver_cfg.ifType = cfgInterface::ifType_map.at(if_vals[IF_id]);
    }
    ImGui::SameLine();
    ImGui::Text("IF Bandwidth: %.3f kHz",
                static_cast<double>(if_vals[IF_id]) / 1000);

    // Ambiguity Scale Max
    ImGui::TableNextColumn();
    ImGui::InputDouble("Ambiguity Maximum", &cfg.process_cfg.ambiguity_lims[1],
                       1, 100, "%.1f");

    // Row 10
    // BW type
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::SliderInt("##BW Slider", &bw_id, 0, bw_vals.size() - 1, "");
    if (ImGui::IsItemDeactivatedAfterEdit()) {
      cfg.receiver_cfg.bwType = cfgInterface::bwType_map.at(bw_vals[bw_id]);
    }
    ImGui::SameLine();
    ImGui::Text("Receiver Bandwidth: %.3f MHz",
                static_cast<double>(bw_vals[bw_id]) / 1000);

    // Ambiguity FFT type
    ImGui::TableNextColumn();
    if (ImGui::Button(cfg.process_cfg.amb_fft_type == AmbiguityType::Full
                          ? "Full FFT for Ambiguity"
                          : "Pruned FFT for Ambiguity")) {
      switch (cfg.process_cfg.amb_fft_type) {
      case AmbiguityType::Full:
        cfg.process_cfg.amb_fft_type = AmbiguityType::Pruned;
        break;
      case AmbiguityType::Pruned:
        cfg.process_cfg.amb_fft_type = AmbiguityType::Full;
      }
    }

    // Row 11
    // LO type
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::SliderInt("##LO Slider", &LO_id, 0, LO_vals.size() - 1, "");
    if (ImGui::IsItemDeactivatedAfterEdit()) {
      cfg.receiver_cfg.loType = cfgInterface::loType_map.at(LO_vals[LO_id]);
    }
    ImGui::SameLine();
    ImGui::Text("LO Bandwidth: %s", LO_disp_vals[LO_id].c_str());

    // Row 12
    // RF Notch
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    if (ImGui::Button(cfg.receiver_cfg.rf_notch_enable ? "RF Notch Enabled"
                                                       : "RF Notch Disabled")) {
      cfg.receiver_cfg.rf_notch_enable = !cfg.receiver_cfg.rf_notch_enable;
    }

    // Row 13
    // DAB Notch
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    if (ImGui::Button(cfg.receiver_cfg.dab_notch_enable
                          ? "DAB Notch Enabled"
                          : "DAB Notch Disabled")) {
      cfg.receiver_cfg.dab_notch_enable = !cfg.receiver_cfg.dab_notch_enable;
    }

    ImGui::EndTable();
  }
}

void RadarApp::detection_frame_update(void) {
  float fps = ImGui::GetIO().Framerate;
  ImGui::Text("Window FPS  %.1f", fps);
  // Set heatmap style
  ImPlot::PushColormap(DETECTION_COLOUR_MAP);

  // Range - Doppler plot
  if (ImPlot::BeginPlot("Detection", ImVec2(-1, -1), ImPlotFlags_NoLegend)) {
    ImPlot::SetupAxes("Speed [m/s]", "Range [m]");

    // Plot ambiguity copy
    ImPlot::PlotHeatmap("Detection", radar_data->detection.read().data(),
                        radar_data->n_range, radar_data->ambiguity_columns, 1,
                        0, NULL, {-radar_data->max_speed, 0},
                        {radar_data->max_speed, cfg.process_cfg.max_range});

    ImPlot::EndPlot();
  }

  // Pop style stack
  ImPlot::PopColormap();
}

void RadarApp::update_window(GLFWwindow *window, bool *show_window) {

  // Event polling
  glfwPollEvents();

  // ImGui Frame
  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();
  const ImGuiViewport *viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(viewport->Pos);
  ImGui::SetNextWindowSize(viewport->Size);
  ImGui::Begin("Passdar", show_window);

  if (!receiver->ready_flag.load() || !stream_a_data->ready_flag.load() ||
      !stream_b_data->ready_flag.load() || !radar_data->ready_flag.load()) {
    // Draw spinner until all threads are ready
    ImVec2 pos;
    pos.x = ImGui::GetContentRegionAvail().x / 2;
    pos.y = ImGui::GetContentRegionAvail().y / 2;
    spinner(100, pos);
  } else {

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

      // Ambiguity slice tab
      if (ImGui::BeginTabItem("Ambiguity Slices")) {
        ambiguity_slice_frame_update();
        ImGui::EndTabItem();
      }

      // Detection tab
      if (ImGui::BeginTabItem("Detection")) {
        detection_frame_update();
        ImGui::EndTabItem();
      }

      // Config settings tab
      if (ImGui::BeginTabItem("Settings")) {
        settings_frame_update();
        ImGui::EndTabItem();
      }
      ImGui::EndTabBar();
    }
    if (ImGui::Button("Restart", ImVec2(-1, -1))) {
      restart.store(true);
    }
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

inline void RadarApp::update_range_vars(void) {
  // Ensure range_step is consistent with sample rate
  range_step = PHASE_VELOCITY / cfg.receiver_cfg.fs;

  // Ensure max_range is consistent with range_stepping values
  cfg.process_cfg.max_range =
      range_step * static_cast<int>(cfg.process_cfg.max_range / range_step) + 1;
}
