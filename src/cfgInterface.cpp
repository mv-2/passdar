#include <fstream>
#include <iostream>
#include <jsoncpp/json/reader.h>
#include <jsoncpp/json/value.h>
#include <sdrplay_api_tuner.h>

#include "cfgInterface.h"

const std::unordered_map<std::string, sdrplay_api_If_kHzT>
    cfgInterface::ifType_map = {
        {"sdrplay_api_IF_Undefined", sdrplay_api_IF_Undefined},
        {"sdrplay_api_IF_Zero", sdrplay_api_IF_Zero},
        {"sdrplay_api_IF_0_450", sdrplay_api_IF_0_450},
        {"sdrplay_api_IF_1_620", sdrplay_api_IF_1_620},
        {"sdrplay_api_IF_2_048", sdrplay_api_IF_2_048}};

const std::unordered_map<std::string, sdrplay_api_Bw_MHzT>
    cfgInterface::bwType_map = {
        {"sdrplay_api_BW_Undefined", sdrplay_api_BW_Undefined},
        {"sdrplay_api_BW_0_200", sdrplay_api_BW_0_200},
        {"sdrplay_api_BW_0_300", sdrplay_api_BW_0_300},
        {"sdrplay_api_BW_0_600", sdrplay_api_BW_0_600},
        {"sdrplay_api_BW_1_536", sdrplay_api_BW_1_536},
        {"sdrplay_api_BW_5_000", sdrplay_api_BW_5_000},
        {"sdrplay_api_BW_6_000", sdrplay_api_BW_6_000},
        {"sdrplay_api_BW_7_000", sdrplay_api_BW_7_000},
        {"sdrplay_api_BW_8_000", sdrplay_api_BW_8_000}};

const std::unordered_map<std::string, sdrplay_api_LoModeT>
    cfgInterface::loType_map = {
        {"sdrplay_api_LO_Undefined", sdrplay_api_LO_Undefined},
        {"sdrplay_api_LO_Auto", sdrplay_api_LO_Auto},
        {"sdrplay_api_LO_120MHz", sdrplay_api_LO_120MHz},
        {"sdrplay_api_LO_144MHz", sdrplay_api_LO_144MHz},
        {"sdrplay_api_LO_168MHz", sdrplay_api_LO_168MHz}};

Json::Value cfgInterface::load_config(std::string cfg_path) {
  Json::Value root;
  std::ifstream cfg_file(cfg_path);
  Json::CharReaderBuilder builder;
  std::string errs;

  // check if file Opened
  if (!cfg_file.is_open()) {
    std::cerr << "Config File Not Opened" << std::endl;
    exit(1);
  }
  // Parse and handle error
  if (!Json::parseFromStream(builder, cfg_file, &root, &errs)) {
    std::cerr << "Error Parsing Config File" << std::endl;
    cfg_file.close();
    exit(1);
  }
  return root;
}

ReceiverConfig::ReceiverConfig(Json::Value json_rcv) {
  fc = json_rcv["fc"].asUInt();
  fs = json_rcv["fs"].asUInt();
  agc_bandwidth_nr = json_rcv["agc_bandwidth_nr"].asInt();
  agc_set_point_nr = json_rcv["agc_set_point_nr"].asInt();
  gRdB_A = json_rcv["gRdB_A"].asInt();
  gRdB_B = json_rcv["gRdB_B"].asInt();
  lna_state = json_rcv["lna_state"].asInt();
  dec_factor = json_rcv["dec_factor"].asInt();
  ifType = cfgInterface::ifType_map.at(json_rcv["ifType"].asString());
  bwType = cfgInterface::bwType_map.at(json_rcv["bwType"].asString());
  loType = cfgInterface::loType_map.at(json_rcv["loType"].asString());
  rf_notch_enable = json_rcv["rf_notch_enable"].asBool();
  dab_notch_enable = json_rcv["dab_notch_enable"].asBool();
}

ReceiverConfig::ReceiverConfig() {
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

ProcessConfig::ProcessConfig(Json::Value json_prcs) {
  buffer_size = json_prcs["buffer_size"].asUInt();
  speed_step = json_prcs["speed_step"].asDouble();
  max_range = json_prcs["max_range"].asDouble();
  max_speed = json_prcs["max_speed"].asDouble();
}

ProcessConfig::ProcessConfig() {
  buffer_size = 0.0;
  speed_step = 0.0;
  max_range = 0.0;
  max_speed = 0.0;
}

Config::Config(std::string cfg_path) : receiver_cfg(), process_cfg() {
  Json::Value json_cfg = cfgInterface::load_config(cfg_path);
  receiver_cfg = ReceiverConfig(json_cfg["receiver"]);
  process_cfg = ProcessConfig(json_cfg["processing"]);
}

Config::Config() : receiver_cfg(), process_cfg() {
  receiver_cfg = ReceiverConfig();
  process_cfg = ProcessConfig();
}
