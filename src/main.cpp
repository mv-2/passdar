#include <atomic>
#include <cstdlib>
#include <iostream>
#include <jsoncpp/json/json.h>
#include <sdrplay_api.h>
#include <sdrplay_api_tuner.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <termios.h>
#include <thread>
#include <unistd.h>

#include "cfgInterface.h"
#include "sdrCapture.h"

// Keyboard functions adapted from
// <https://www.flipcode.com/archives/_kbhit_for_Linux.shtml>
int _kbhit() {
  static const int STDIN = 0;
  static bool initialised = false;

  if (!initialised) {
    termios term;
    tcgetattr(STDIN, &term);
    term.c_lflag &= ~ICANON;
    tcsetattr(STDIN, TCSANOW, &term);
    setbuf(stdin, NULL);
    initialised = true;
  }

  int bytesWaiting;
  ioctl(STDIN, FIONREAD, &bytesWaiting);
  return bytesWaiting;
}

// End when q key is pressed
bool break_loop() {
  if (_kbhit()) {
    if (getchar() == 'q') {
      return true;
    }
  }
  return false;
}

// Driver function for testing
int main(int argc, char *argv[]) {
  // Loop flag
  std::atomic<bool> exit_flag(false);

  // Get config file location
  if (argc < 2) {
    std::cerr << "No config file name detected" << std::endl;
    return 0;
  }

  // Load configs as JSON values
  Json::Value cfg = cfgInterface::load_config(argv[1]);

  // Create receiver and data objects
  Receiver *receiver = new Receiver(cfg["receiver"]);
  SpecData *stream_a_data = new SpecData(cfg);
  SpecData *stream_b_data = new SpecData(cfg);
  RadarData *radar_data = new RadarData(stream_a_data, stream_b_data);

  // Capture Thread
  std::thread captureThread(
      [&] { receiver->run_capture(stream_a_data, stream_b_data, &exit_flag); });

  // Processing threads
  std::thread processThread_A([&] { stream_a_data->process_data(&exit_flag); });
  std::thread processThread_B([&] { stream_b_data->process_data(&exit_flag); });

  // Plotting thread
  std::thread plotThread([&] { radar_data->plot_spectra(&exit_flag); });

  // User exit signal from main thread
  while (!break_loop()) {
    sleep(1);
  }

  // set flag to true when loop break condition met
  exit_flag.store(true);

  // Join threads to end processes. Changing order results in seg fault
  plotThread.join();
  processThread_A.join();
  processThread_B.join();
  captureThread.join();

  return 0;
}
