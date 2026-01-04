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
#include "radarData.h"
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
  // Get config file location
  if (argc < 2) {
    std::cerr << "No config file name detected" << std::endl;
    return 0;
  }

  // Create config struct
  Config cfg = Config(argv[1]);

  // Create receiver and data objects
  Receiver *receiver = new Receiver(cfg.receiver_cfg);
  SpecData *stream_a_data = new SpecData(cfg);
  SpecData *stream_b_data = new SpecData(cfg);
  RadarData *radar_data = new RadarData(cfg, stream_a_data, stream_b_data);

  // Thread exit flag
  std::atomic<bool> exit_flag(false);

  // Capture Thread
  std::thread captureThread(
      [&] { receiver->run_capture(stream_a_data, stream_b_data, &exit_flag); });

  // Processing threads
  std::thread spectrumThread_A(
      [&] { stream_a_data->process_spectrum(&exit_flag); });
  std::thread spectrumThread_B(
      [&] { stream_b_data->process_spectrum(&exit_flag); });
  std::thread ambiguity([&] { radar_data->process_ambiguity(&exit_flag); });

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
  ambiguity.join();
  spectrumThread_A.join();
  spectrumThread_B.join();
  captureThread.join();

  return 0;
}
