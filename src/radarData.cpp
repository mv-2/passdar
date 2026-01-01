#include <cmath>
#include <complex.h>
#include <cstdlib>
#include <matplot/freestanding/axes_functions.h>
#include <matplot/matplot.h>

#include "cfgInterface.h"
#include "radarData.h"

// Wave propagation velocity
const double PHASE_VELOCITY = 3e8;

RadarData::RadarData(Config cfg, SpecData *_stream_a_data,
                     SpecData *_stream_b_data) {
  // assign data stream pointers
  stream_a_data = _stream_a_data;
  stream_b_data = _stream_b_data;

  // Sample Frequency
  sample_frequency = cfg.receiver_cfg.fs / cfg.receiver_cfg.dec_factor;

  // Calculate range step
  range_step = PHASE_VELOCITY / sample_frequency;

  // Calculate speed step
  speed_step = cfg.process_cfg.speed_step;

  // num speed points includes positive and negative values + 1 point for 0
  // speed case in centre
  n_speed = 2 * static_cast<int>(cfg.process_cfg.max_speed / speed_step) + 1;

  // range points ignores 0 range case
  n_range = static_cast<int>(cfg.process_cfg.max_range / range_step);

  // Preallocate memory for ambiguity
  ambiguity.reserve(n_speed);
  for (int i = 0; i < n_speed; i++) {
    ambiguity[i].reserve(n_range);
  }
}

void RadarData::plot_spectra(std::atomic<bool> *exit_flag) {
  auto fig = matplot::figure(false);

  matplot::tiledlayout(2, 1);
  auto ax1 = matplot::nexttile();
  matplot::plot(ax1, stream_a_data->frequency, stream_a_data->spectrum);
  matplot::title(ax1, "Receiver A");
  matplot::ylabel("Amplitude [dB]");
  matplot::xlabel("Frequency [kHz]");

  auto ax2 = matplot::nexttile();
  matplot::plot(ax2, stream_b_data->frequency, stream_b_data->spectrum);
  matplot::title(ax1, "Receiver B");
  matplot::ylabel("Amplitude [dB]");
  matplot::xlabel("Frequency [kHz]");

  fig->draw();

  while (!exit_flag) {
    sleep(1);
    stream_a_data->mutex_lock.lock();
    stream_b_data->mutex_lock.lock();
    fig->draw();
    stream_a_data->mutex_lock.unlock();
    stream_b_data->mutex_lock.unlock();
  }
}

void RadarData::process_ambiguity() {
  // WB ambiguity time scale factor
  double time_scale;

  // Temp value for integration
  std::complex<double> int_temp;

  // delay offset
  unsigned int sample_index;

  // Copy samples out of data streams
  stream_a_data->data_iq->mutex_lock.lock();
  stream_b_data->data_iq->mutex_lock.lock();
  for (int i = 0; i < stream_b_data->max_length; i++) {
    data_a_copy[i] = stream_a_data->data_iq->samples.at(i);
    data_b_copy[i] = stream_b_data->data_iq->samples.at(i);
  }
  stream_a_data->data_iq->mutex_lock.unlock();
  stream_b_data->data_iq->mutex_lock.unlock();

  // Calculate ambiguity surface
  for (int v_id = -n_speed / 2; v_id < n_speed / 2; v_id++) {
    time_scale = (PHASE_VELOCITY + v_id * speed_step) /
                 (PHASE_VELOCITY - v_id * speed_step);
    for (int r_id = -n_range / 2; r_id < n_range / 2; r_id++) {
      int_temp = 0.0;
      for (int i = 0; i < data_a_copy.size(); i++) {
        sample_index = static_cast<unsigned int>(time_scale * (i - r_id));
        int_temp =
            int_temp + data_a_copy[i] * std::conj(data_b_copy[sample_index]);
      }
      int_temp = std::sqrt(std::abs(time_scale)) * int_temp /
                 static_cast<double>(sample_frequency);
      ambiguity[v_id][r_id] = std::abs(int_temp);
    }
  }
}

void RadarData::plot_ambiguity(std::atomic<bool> *exit_flag) {
  // Initialise plot window
  FILE *plot_pipe = popen("gnuplot -persist", "w");

  while (!(exit_flag->load())) {
    sleep(1);

    // Calculate ambiguity surface
    process_ambiguity();
  }
}
