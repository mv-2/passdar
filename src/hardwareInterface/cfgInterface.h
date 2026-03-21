#ifndef CFGINTERFACE_H
#define CFGINTERFACE_H

#include <nlohmann/json.hpp>
#include <sdrplay_api_tuner.h>
#include <sys/types.h>
#include <unordered_map>

/**
 * @brief DFT windowing enum
 */
enum class DftWindow { Rectangular, Hanning };

/**
 * @brief Ambiguity calculation scale enum
 */
enum class DisplayScale { Linear, dB };

// Default sample frequency constant
/**
 * @brief Default sample frequency set by SDRplay RSPDuo
 * @details Sample rate is not mutable and may only be changed by decimating
 * data
 */
const double SAMPLE_FREQUENCY_DEFAULT = 6000000;

/**
 * @brief Class of static functions and variables for config parsing
 */
class cfgInterface {
public:
  /**
   * @brief Read config file to Json object
   */
  static nlohmann::json load_config(std::string cfg_path);

  /// @name SDRplay enum to native data type maps
  /// @{
  static const std::unordered_map<int, sdrplay_api_If_kHzT>
      ifType_map; /// unordered_map containing string to enum mappings for all
                  /// IF filter values
  static const std::unordered_map<sdrplay_api_If_kHzT, int>
      ifNum_map; /// unordered_map containing IF filter value to int mappings
  static const std::unordered_map<int, sdrplay_api_Bw_MHzT>
      bwType_map; ///  unordered_map containing string to enum mappings for all
                  ///  receiver bandwidth values
  static const std::unordered_map<sdrplay_api_Bw_MHzT, int>
      bwNum_map; /// unordered_map containing receiver bandwidth value to int
                 /// mappings
  static const std::unordered_map<std::string, sdrplay_api_LoModeT>
      loType_map; /// unordered_map containing string to enum mappings for all
                  /// LO filter values
  static const std::unordered_map<sdrplay_api_LoModeT, std::string>
      loStr_map; ///  unordered_map containing LO filter value to string
                 ///  mappings
  /// @}
};

/**
 * @brief Detection parameters
 */
struct DetectionConfig {
  double cfar_multiplier; /// CFAR coefficient to multiply threshold
  int range_window;       /// Number of cells to include rangewise
  int range_guard;        /// Number of cells to exclude rangewise
  int speed_window;       /// Number of cells to include speedwise
  int speed_guard;        /// Number of cells to exclude speedwise

  DetectionConfig(nlohmann::json json_det); /// Constructor from Json object
  DetectionConfig();                        /// Default constructor
};

/**
 * @brief Receiver parameters
 */
struct ReceiverConfig {
  int fc;                     /// Centre frequency
  int fs;                     /// Sampling Frequency
  int agc_bandwidth_nr;       /// AGC bandwidth
  int agc_set_point_nr;       /// AGC set point
  int gRdB_A;                 /// Gain reduction receiver A
  int gRdB_B;                 /// Gain reduction receiver B
  int lna_state;              /// LNA state
  int dec_factor;             /// Decimation factor
  sdrplay_api_If_kHzT ifType; /// IF filter type
  sdrplay_api_Bw_MHzT bwType; /// Receiver bandwidth filter type
  sdrplay_api_LoModeT loType; /// LO filter type
  bool rf_notch_enable;       /// RF notch flag
  bool dab_notch_enable;      /// DAB notch flag

  ReceiverConfig(nlohmann::json json_rcv); /// Constructor from Json object
  ReceiverConfig();                        /// Default constructor
};

/**
 * @brief Processing parameters
 */
struct ProcessConfig {
  double max_range;             /// Maximum range/time-delay to interrogate
  double max_speed;             /// Maximum speed to calculate
  int buffer_size;              /// Length of sample buffer
  DftWindow dft_window;         /// DFT window (applied to spectra only)
  std::string _win_str;         /// String to parse & display window type
  DisplayScale ambiguity_scale; /// Scale to calculate ambiguity surface
  double ambiguity_lims[2]; /// [min, max] limits to display ambiguity surface

  ProcessConfig(nlohmann::json json_prcs); /// Constructor from Json object
  ProcessConfig();                         /// Default constructor
};

/**
 * @brief Overall config struct
 * @details Stores ReceiverConfig, ProcessConfig and DetectionConfig in one
 * struct
 */
struct Config {
  ReceiverConfig receiver_cfg;      /// ReceiverConfig struct
  ProcessConfig process_cfg;        /// ProcessConfig struct
  DetectionConfig detection_config; /// DetectionConfig struct

  Config(std::string cfg_path); /// Constructor from Json object
  Config();                     /// Default constructor
};
#endif
