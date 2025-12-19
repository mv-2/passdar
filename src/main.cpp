#include <iostream>
#include <sdrplay_api.h>
#include <sdrplay_api_tuner.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <termios.h>
#include <thread>
#include <unistd.h>

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

void process_loop(SpecData *_stream_a_data, SpecData *_stream_b_data,
                  bool(loop_exit)(void)) {
  SpecData *stream_a_data = _stream_a_data;
  SpecData *stream_b_data = _stream_b_data;
  while (true) {
    if (loop_exit()) {
      break;
    }
    std::cout << 1 << std::endl;
    stream_a_data->mutex_lock.lock();
    stream_b_data->mutex_lock.lock();
    // stream_a_data->calc_dft();
    // stream_b_data->calc_dft();
    stream_a_data->mutex_lock.unlock();
    stream_b_data->mutex_lock.unlock();
    sleep(1);
  }
}

// Driver function for testing
int main(void) {
  // vars
  char c;

  // TODO: Make this a config file
  uint32_t fc = 125000;
  uint32_t fs = 220000000;
  int agc_bandwidth_nr = 0;
  int agc_set_point_nr = 0;
  int gRdB_A = 40;
  int gRdB_B = 40;
  int lna_state = 0;
  int dec_factor = 1;
  sdrplay_api_If_kHzT ifType = sdrplay_api_IF_0_450;
  sdrplay_api_Bw_MHzT bwType = sdrplay_api_BW_1_536;
  bool rf_notch_enable = false;
  bool dab_notch_enable = false;
  SpecData *stream_a_data = new SpecData(2048);
  SpecData *stream_b_data = new SpecData(2048);

  // Create receiver
  Receiver *receiver = new Receiver(
      fc, agc_bandwidth_nr, agc_set_point_nr, gRdB_A, gRdB_B, lna_state,
      dec_factor, ifType, bwType, rf_notch_enable, dab_notch_enable);

  // Start Capture Thread
  std::thread captureThread(
      [&] { receiver->run_capture(stream_a_data, stream_b_data, break_loop); });

  // Processing thread
  std::thread processThread(
      [&] { process_loop(stream_a_data, stream_b_data, break_loop); });

  captureThread.join();
  processThread.join();

  return 0;
}
