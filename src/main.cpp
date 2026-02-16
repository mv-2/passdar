#include <iostream>

#include "frontend/radarApp.h"
#include "hardwareInterface/cfgInterface.h"

int main(int argc, char *argv[]) {
  // Get config file location
  if (argc < 2) {
    std::cerr << "No config file name detected" << std::endl;
    return 0;
  }

  // Create config struct
  Config cfg = Config(argv[1]);

  // Initialise and run app class
  RadarApp main_app = RadarApp(cfg);
  main_app.run();
  return 0;
}
