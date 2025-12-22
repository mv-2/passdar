#include <fstream>
#include <iostream>
#include <jsoncpp/json/json.h>
#include <sdrplay_api.h>
#include <sdrplay_api_tuner.h>
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
   * Static method to read config to Json::Value
   */
  static Json::Value load_config(std::string cfg_path);
};
