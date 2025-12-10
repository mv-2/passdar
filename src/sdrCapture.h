#ifndef SDRCAPTURE_H
#define SDRCAPTURE_H

#include "sdrplay_api.h"
#include <stdint.h>

class Receiver {
  // NOTE: CHECK WHAT IS PUBLIC LATER
public:
  uint32_t fc;

  Receiver(uint32_t fc);

  // TODO: Docstrings
  void start_api();
  void get_device();
  void set_device_parameters();
  void cleanup();
};

#endif
