#ifndef CFGINTERFACE_H
#define CFGINTERFACE_H

#include <cstdint>
#include <fstream>
#include <iostream>
#include <jsoncpp/json/json.h>
#include <jsoncpp/json/value.h>
#include <sdrplay_api.h>
#include <sdrplay_api_tuner.h>
#include <sys/types.h>
#include <unordered_map>

/*
 * Class of static functions and variables for ease of config parsing
 */
class cfgInterface {
public:
  /*
   * unordered_map containing string to enum mappings for all IF values
   */
  static const std::unordered_map<std::string, sdrplay_api_If_kHzT> ifType_map;

  /*
   * unordered_map containing string to enum mappings for all BW values
   */
  static const std::unordered_map<std::string, sdrplay_api_Bw_MHzT> bwType_map;

  /*
   * unordered_map containing string to enum mappings for all LO values
   */
  static const std::unordered_map<std::string, sdrplay_api_LoModeT> loType_map;

  /*
   * Static method to read config to Json::Value
   */
  static Json::Value load_config(std::string cfg_path);
};

/*
 * Stores receiver parameters
 */
struct ReceiverConfig {
  uint32_t fc;
  uint32_t fs;
  int agc_bandwidth_nr;
  int agc_set_point_nr;
  int gRdB_A;
  int gRdB_B;
  int lna_state;
  int dec_factor;
  sdrplay_api_If_kHzT ifType;
  sdrplay_api_Bw_MHzT bwType;
  sdrplay_api_LoModeT loType;
  bool rf_notch_enable;
  bool dab_notch_enable;

  ReceiverConfig(Json::Value json_rcv);
  ReceiverConfig();
};

/*
 * Processing parameters
 */
struct ProcessConfig {
  uint32_t buffer_size;
  double speed_step;
  double max_range;
  double max_speed;

  ProcessConfig(Json::Value json_prcs);
  ProcessConfig();
};

/*
 * Stores config as nested struct
 */
struct Config {
  ReceiverConfig receiver_cfg;
  ProcessConfig process_cfg;

  Config(std::string cfg_path);
};
#endif
