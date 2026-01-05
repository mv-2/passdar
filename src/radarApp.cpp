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

RadarApp::RadarApp(Config _cfg) {
  receiver = new Receiver(_cfg.receiver_cfg);
  stream_a_data = new SpecData(_cfg);
  stream_b_data = new SpecData(_cfg);
  radar_data = new RadarData(_cfg, stream_a_data, stream_b_data);
  cfg = _cfg;
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
    return window;
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
  ImGui::StyleColorsDark();
  ImGuiStyle &style = ImGui::GetStyle();
  style.ScaleAllSizes(main_scale);
  style.FontScaleDpi = main_scale;

  // ImPlot context & backend
  ImPlot::CreateContext();
  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init(glsl_version);

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

  // Hold ambiguity during processing
  std::vector<double> ambiguity_copy(radar_data->n_range * radar_data->n_speed);
  bool show_window = true;

  while (show_window && !glfwWindowShouldClose(window)) {
    glfwPollEvents();

    // ImGui Frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    const ImGuiViewport *viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    ImGui::Begin("Passdar", &show_window);

    if (ImGui::BeginTabBar("Passdar")) {
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
          if (radar_data->ambiguity_mutex.try_lock()) {
            ImPlot::PlotHeatmap("Range - Doppler", radar_data->ambiguity.data(),
                                radar_data->n_speed, radar_data->n_range, 0, 0,
                                NULL);
            ambiguity_copy = radar_data->ambiguity;
            radar_data->ambiguity_mutex.unlock();
          } else {
            ImPlot::PlotHeatmap("Range - Doppler", ambiguity_copy.data(),
                                radar_data->n_speed, radar_data->n_range, 0, 0,
                                NULL);
          }
          ImPlot::EndPlot();
        }
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

          // TODO: FIX THIS
          // IF type
          ImGui::TableNextRow();
          ImGui::TableNextColumn();
          ImGui::Text("IF");
          ImGui::TableNextColumn();
          ImGui::Text("%d kHz",
                      cfgInterface::ifNum_map.at(cfg.receiver_cfg.ifType));

          // TODO: FIX THIS
          // BW type
          ImGui::TableNextRow();
          ImGui::TableNextColumn();
          ImGui::Text("Bandwidth");
          ImGui::TableNextColumn();
          ImGui::Text("%d MHz",
                      cfgInterface::bwNum_map.at(cfg.receiver_cfg.bwType));

          // TODO: FIX THIS
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
          ImGui::Text("%s", cfg.receiver_cfg.rf_notch_enable ? "Enabled"
                                                             : "Disabled");

          // DAB Notch
          ImGui::TableNextRow();
          ImGui::TableNextColumn();
          ImGui::Text("DAB Notch Filter");
          ImGui::TableNextColumn();
          ImGui::Text("%s", cfg.receiver_cfg.dab_notch_enable ? "Enabled"
                                                              : "Disabled");
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

          // Speed step size
          ImGui::TableNextRow();
          ImGui::TableNextColumn();
          ImGui::Text("Speed Step");
          ImGui::TableNextColumn();
          ImGui::Text("%lf", cfg.process_cfg.speed_step);

          // Max range
          ImGui::TableNextRow();
          ImGui::TableNextColumn();
          ImGui::Text("Max Range");
          ImGui::TableNextColumn();
          ImGui::Text("%lf", cfg.process_cfg.max_range);

          // Max Speed
          ImGui::TableNextRow();
          ImGui::TableNextColumn();
          ImGui::Text("Max Speed");
          ImGui::TableNextColumn();
          ImGui::Text("%lf", cfg.process_cfg.max_speed);

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
