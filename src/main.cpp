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

void plot_data(SpecData *stream_a_data, SpecData *stream_b_data,
               bool(loop_exit)(void)) {
  // Initialise plot window
  FILE *plot_pipe = popen("gnuplot -persist", "w");

  while (!loop_exit()) {
    sleep(1);
    fprintf(plot_pipe, "unset multiplot\n");
    stream_a_data->set_plot_datablock(plot_pipe, "A");
    stream_b_data->set_plot_datablock(plot_pipe, "B");
    fprintf(plot_pipe,
            "set multiplot layout 2,1 rowsfirst title \"Spectra\"\n");

    fprintf(plot_pipe, "set title \"Receiver A\"\n");
    fprintf(plot_pipe, "set xlabel \"Frequency\"\n");
    fprintf(plot_pipe, "set ylabel \"Amplitude\"\n");
    fprintf(plot_pipe, "plot $data_A with lines\n");

    fprintf(plot_pipe, "set title \"Receiver B\"\n");
    fprintf(plot_pipe, "set xlabel \"Frequency\"\n");
    fprintf(plot_pipe, "set ylabel \"Amplitude\"\n");
    fprintf(plot_pipe, "plot $data_B with lines\n");
    fflush(plot_pipe);
  }
}

// Driver function for testing
int main(void) {
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
  SpecData *stream_a_data = new SpecData(2000);
  SpecData *stream_b_data = new SpecData(2000);

  // Create receiver
  Receiver *receiver = new Receiver(
      fc, agc_bandwidth_nr, agc_set_point_nr, gRdB_A, gRdB_B, lna_state,
      dec_factor, ifType, bwType, rf_notch_enable, dab_notch_enable);

  // Start Capture Thread
  std::thread captureThread(
      [&] { receiver->run_capture(stream_a_data, stream_b_data, break_loop); });

  // Processing threads
  std::thread processThread_A([&] { stream_a_data->process_data(break_loop); });
  std::thread processThread_B([&] { stream_b_data->process_data(break_loop); });

  // Plotting thread
  std::thread plotThread(
      [&] { plot_data(stream_a_data, stream_b_data, break_loop); });

  // Start processes
  plotThread.join();
  captureThread.join();
  processThread_A.join();
  processThread_B.join();

  return 0;
}
