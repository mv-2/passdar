#ifndef CFGINTERFACE_H
#define CFGINTERFACE_H

#include <cstdint>
#include <nlohmann/json.hpp>
#include <sdrplay_api_tuner.h>
#include <sys/types.h>
#include <unordered_map>

// DFT windowing enum
enum class DftWindow { Rectangular, Hanning };

/*
 * Class of static functions and variables for ease of config parsing
 */
class cfgInterface {
public:
  /*
   * unordered_map containing string to enum mappings for all IF values
   */
  static const std::unordered_map<int, sdrplay_api_If_kHzT> ifType_map;

  /*
   * unordered_map containing IF value to int mappings
   */
  static const std::unordered_map<sdrplay_api_If_kHzT, int> ifNum_map;

  /*
   * unordered_map containing string to enum mappings for all BW values
   */
  static const std::unordered_map<int, sdrplay_api_Bw_MHzT> bwType_map;

  /*
   * unordered_map containing BW Value to int mappings
   */
  static const std::unordered_map<sdrplay_api_Bw_MHzT, int> bwNum_map;

  /*
   * unordered_map containing string to enum mappings for all LO values
   */
  static const std::unordered_map<std::string, sdrplay_api_LoModeT> loType_map;

  /*
   * unordered_map containing LO Value to string mappings
   */
  static const std::unordered_map<sdrplay_api_LoModeT, std::string> loStr_map;

  /*
   * Static method to read config to Json::Value
   */
  static nlohmann::json load_config(std::string cfg_path);

  /*
   * Static method to read config to Json::Value
   */
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

  static const double SAMPLE_FREQUENCY_DEFAULT;

  ReceiverConfig(nlohmann::json json_rcv);
  ReceiverConfig();
};

/*
 * Processing parameters
 */
struct ProcessConfig {
  uint32_t buffer_size;
  double max_range;
  double max_speed;
  DftWindow dft_window;
  std::string _win_str;

  ProcessConfig(nlohmann::json json_prcs);
  ProcessConfig();
};

/*
 * Stores config as nested struct
 */
struct Config {
  ReceiverConfig receiver_cfg;
  ProcessConfig process_cfg;

  Config(std::string cfg_path);
  Config();
};
#endif
