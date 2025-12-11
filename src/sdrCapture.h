#ifndef SDRCAPTURE_H
#define SDRCAPTURE_H

#include "sdrplay_api.h"
#include <cstdint>
#include <sdrplay_api_tuner.h>
#include <stdint.h>

class Receiver {
  // NOTE: CHECK WHAT IS PUBLIC LATER
public:
  uint32_t fc;
  int agc_bandwidth_nr;
  int agc_set_point_nr;
  int _gain_reduction_nr_a;
  int gRdB_A;
  int gRdB_B;
  int lna_state;
  int dec_factor;
  sdrplay_api_If_kHzT ifType;
  sdrplay_api_Bw_MHzT bwType;

  Receiver(uint32_t _fc, int _agc_bandwidth_nr, int _agc_set_point_nr,
           int _gRdB_A, int _gRdB_B, int lna_state, int _dec_factor,
           sdrplay_api_If_kHzT _ifType, sdrplay_api_Bw_MHzT _bwType);

  // TODO: Docstrings
  void start_api();
  void get_device();
  void set_device_parameters();
  void cleanup();
};

#endif
