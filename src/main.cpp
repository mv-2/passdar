#include "frontend/radarApp.h"
#include "hardwareInterface/cfgInterface.h"

int main(int argc, char *argv[]) {
  RadarApp *main_app;

  if (argc < 2) {
    // Default constructor
    main_app = new RadarApp();
  } else {
    // Defined config constructor
    Config cfg = Config(argv[1]);
    main_app = new RadarApp(cfg);
  }

  // Initialise and run app class
  main_app->run();
  return 0;
}
