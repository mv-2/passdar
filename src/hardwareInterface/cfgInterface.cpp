#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <sdrplay_api_tuner.h>
#include <unordered_map>

#include "cfgInterface.h"

template <typename T1, typename T2>
constexpr std::unordered_map<T2, T1>
reverse_map(std::unordered_map<T1, T2> fwd_map) {
  std::unordered_map<T2, T1> rev_map;
  for (const auto &pair : fwd_map) {
    rev_map[pair.second] = pair.first;
  }
  return rev_map;
}

const std::unordered_map<int, sdrplay_api_If_kHzT> cfgInterface::ifType_map = {
    {-1, sdrplay_api_IF_Undefined},
    {0, sdrplay_api_IF_Zero},
    {450, sdrplay_api_IF_0_450},
    {1620, sdrplay_api_IF_1_620},
    {2048, sdrplay_api_IF_2_048}};

const std::unordered_map<sdrplay_api_If_kHzT, int> cfgInterface::ifNum_map =
    reverse_map(cfgInterface::ifType_map);

const std::unordered_map<int, sdrplay_api_Bw_MHzT> cfgInterface::bwType_map = {
    {-1, sdrplay_api_BW_Undefined}, {200, sdrplay_api_BW_0_200},
    {300, sdrplay_api_BW_0_300},    {600, sdrplay_api_BW_0_600},
    {1536, sdrplay_api_BW_1_536},   {5000, sdrplay_api_BW_5_000},
    {6000, sdrplay_api_BW_6_000},   {7000, sdrplay_api_BW_7_000},
    {8000, sdrplay_api_BW_8_000}};

const std::unordered_map<sdrplay_api_Bw_MHzT, int> cfgInterface::bwNum_map =
    reverse_map(cfgInterface::bwType_map);

const std::unordered_map<std::string, sdrplay_api_LoModeT>
    cfgInterface::loType_map = {{"-1", sdrplay_api_LO_Undefined},
                                {"Auto", sdrplay_api_LO_Auto},
                                {"120", sdrplay_api_LO_120MHz},
                                {"144", sdrplay_api_LO_144MHz},
                                {"168", sdrplay_api_LO_168MHz}};

const std::unordered_map<sdrplay_api_LoModeT, std::string>
    cfgInterface::loStr_map = reverse_map(cfgInterface::loType_map);

const std::unordered_map<std::string, DftWindow> window_map = {
    {"Rectangular", DftWindow::Rectangular}, {"Hanning", DftWindow::Hanning}};

nlohmann::json cfgInterface::load_config(std::string cfg_path) {
  std::ifstream cfg_file(cfg_path);
  if (!cfg_file.is_open()) {
    std::cerr << "Failed to open config file" << std::endl;
    exit(1);
  }
  return nlohmann::json::parse(cfg_file);
}

ReceiverConfig::ReceiverConfig(nlohmann::json json_rcv) {
  static const int SAMPLE_FREQUENCY_DEFAULT = 6000000;
  dec_factor = json_rcv["dec_factor"];
  fc = json_rcv["fc"];
  fs = SAMPLE_FREQUENCY_DEFAULT / dec_factor;
  agc_bandwidth_nr = json_rcv["agc_bandwidth_nr"];
  agc_set_point_nr = json_rcv["agc_set_point_nr"];
  gRdB_A = json_rcv["gRdB_A"];
  gRdB_B = json_rcv["gRdB_B"];
  lna_state = json_rcv["lna_state"];
  ifType = cfgInterface::ifType_map.at(json_rcv["if_kHz"]);
  bwType = cfgInterface::bwType_map.at(json_rcv["bw_MHz"]);
  loType = cfgInterface::loType_map.at(json_rcv["lo_MHz"]);
  rf_notch_enable = json_rcv["rf_notch_enable"];
  dab_notch_enable = json_rcv["dab_notch_enable"];
}

ReceiverConfig::ReceiverConfig() {
  // static const int SAMPLE_FREQUENCY_DEFAULT = 6000000;
  fc = 0;
  fs = 0;
  agc_bandwidth_nr = 0;
  agc_set_point_nr = 0;
  gRdB_A = 0;
  gRdB_B = 0;
  lna_state = 0;
  dec_factor = 0;
  ifType = sdrplay_api_IF_Undefined;
  bwType = sdrplay_api_BW_Undefined;
  loType = sdrplay_api_LO_Undefined;
  rf_notch_enable = 0;
  dab_notch_enable = 0;
}

ProcessConfig::ProcessConfig(nlohmann::json json_prcs) {
  buffer_size = json_prcs["buffer_size"];
  max_range = json_prcs["max_range"];
  max_speed = json_prcs["max_speed"];
  dft_window = window_map.at(json_prcs["dft_window"]);
  _win_str = json_prcs["dft_window"];
}

ProcessConfig::ProcessConfig() {
  buffer_size = 0.0;
  max_range = 0.0;
  max_speed = 0.0;
  dft_window = DftWindow::Rectangular;
  _win_str = "Rectangular";
}

Config::Config(std::string cfg_path) : receiver_cfg(), process_cfg() {
  nlohmann::json json_cfg = cfgInterface::load_config(cfg_path);
  receiver_cfg = ReceiverConfig(json_cfg["receiver"]);
  process_cfg = ProcessConfig(json_cfg["processing"]);
}

Config::Config() : receiver_cfg(), process_cfg() {
  receiver_cfg = ReceiverConfig();
  process_cfg = ProcessConfig();
}
