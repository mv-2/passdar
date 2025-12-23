#include "cfgInterface.h"
#include <jsoncpp/json/reader.h>
#include <sdrplay_api_tuner.h>

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
