#include "radarData.h"

RadarData::RadarData(SpecData *_stream_a_data, SpecData *_stream_b_data) {
  stream_a_data = _stream_a_data;
  stream_b_data = _stream_b_data;
}

void RadarData::plot_spectra(std::atomic<bool> *exit_flag) {
  // Initialise plot window
  FILE *plot_pipe = popen("gnuplot -persist", "w");

  // Reset data block and replot each second
  while (!(exit_flag->load())) {
    sleep(1);
    // Set datablock values
    stream_a_data->set_plot_datablock(plot_pipe, 1);
    stream_b_data->set_plot_datablock(plot_pipe, 2);

    // Create multiplot layout
    fprintf(plot_pipe,
            "set multiplot layout 2,1 rowsfirst title \"Spectra\"\n");

    // Receiver A plot
    fprintf(plot_pipe, "set title \"Receiver A\"\n");
    fprintf(plot_pipe, "set xlabel \"Frequency [kHz]\"\n");
    fprintf(plot_pipe, "set ylabel \"Amplitude\"\n");
    fprintf(plot_pipe, "unset key\n");
    fprintf(plot_pipe, "plot $data_1 with lines\n");

    // Receiver B plot
    fprintf(plot_pipe, "set title \"Receiver B\"\n");
    fprintf(plot_pipe, "set xlabel \"Frequency [kHz]\"\n");
    fprintf(plot_pipe, "set ylabel \"Amplitude\"\n");
    fprintf(plot_pipe, "unset key\n");
    fprintf(plot_pipe, "plot $data_2 with lines\n");
  }
}

void RadarData::process_ambiguity() {}
