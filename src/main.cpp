#include <iostream>
#include <jsoncpp/json/json.h>
#include <sdrplay_api.h>
#include <sdrplay_api_tuner.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>

#include "cfgInterface.h"
#include "radarApp.h"

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
