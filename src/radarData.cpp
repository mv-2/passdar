#include <GL/gl.h>
#include <GLFW/glfw3.h>
#include <chrono>
#include <complex.h>
#include <iostream>
#include <thread>
#include <vector>

#include "cfgInterface.h"
#include "radarData.h"

// Number of available threads for ambiguity calculation
const int NUM_AMBIGUITY_THREADS = 12;

// Wave propagation velocity (I know we aren't in a vacuum)
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

  // range points
  double freq_step =
      (stream_a_data->frequency[stream_a_data->max_length / 2 + 3] -
       stream_a_data->frequency[stream_a_data->max_length / 2 + 2]) *
      1e6;
  speed_step = freq_step * PHASE_VELOCITY / (2 * cfg.receiver_cfg.fc);
  max_speed = cfg.process_cfg.max_speed;
  n_speed = 2 * max_speed / speed_step + 1;

  // range points
  n_range = static_cast<int>(cfg.process_cfg.max_range / range_step) + 1;

  // preallocate ambiguity
  ambiguity.resize(n_range * n_speed);

  // data copy preallocations
  data_a_copy.resize(cfg.process_cfg.buffer_size);
  data_b_copy.resize(cfg.process_cfg.buffer_size);
}

void RadarData::ambiguity_thread_calc(int first_col, int last_col) {
  // Temp value to accumulate integral
  std::complex<double> int_temp;

  // FFTshift vel
  int v_id_swap;

  // Ambiguity calculation perform FFTshift at same time
  for (int v_id = first_col; v_id < last_col; v_id++) {
    v_id_swap = (v_id + n_speed / 2) % n_speed;
    for (int r_id = 0; r_id < n_range; r_id++) {
      int_temp = 0.0;
      // TEST: See if FFTW3 can be used to speed up this process
      for (unsigned int j = 0; j < (data_a_copy.size() - n_range); j++) {
        int_temp += data_a_copy[j + r_id] * std::conj(data_b_copy[j]) *
                    std::polar(1.0, static_cast<double>(-2 * j * v_id) * M_PI /
                                        static_cast<double>(n_speed));
      }
      // NOTE: This is technically incorrect because absolute value of int_temp
      // should be divided by the pulse length but as only relative ambiguity
      // matters the offset of -20*log10(data_a_copy.size()) can be applied
      // later
      ambiguity[(n_range - r_id - 1) * n_speed + v_id_swap] =
          20.0 * log10(std::abs(int_temp));
    }
  }
  return;
}

void RadarData::process_ambiguity(std::atomic<bool> *exit_flag) {
  // Initialise processing threads
  std::deque<std::thread> amb_threads;
  std::vector<int> first_cols(NUM_AMBIGUITY_THREADS),
      last_cols(NUM_AMBIGUITY_THREADS);
  int col_step = n_speed % NUM_AMBIGUITY_THREADS == 0
                     ? n_speed / NUM_AMBIGUITY_THREADS
                     : n_speed / NUM_AMBIGUITY_THREADS + 1;

  // Set column limits for threads
  first_cols[0] = 0;
  for (int i = 0; i < NUM_AMBIGUITY_THREADS - 1; i++) {
    last_cols[i] = first_cols[i] + col_step;
    first_cols[i + 1] = last_cols[i];
  }
  last_cols[NUM_AMBIGUITY_THREADS - 1] = n_speed + 1;

  while (!exit_flag->load()) {
    // await next sample block
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // lock iqdata and copy samples
    stream_a_data->data_iq->mutex_lock.lock();
    stream_b_data->data_iq->mutex_lock.lock();
    for (unsigned int i = 0; i < data_a_copy.size(); i++) {
      data_a_copy[i] = stream_a_data->data_iq->samples[i];
      data_b_copy[i] = stream_b_data->data_iq->samples[i];
    }
    stream_a_data->data_iq->mutex_lock.unlock();
    stream_b_data->data_iq->mutex_lock.unlock();

    // Calculate ambiguity
    ambiguity_mutex.lock();
    for (int i = 0; i < NUM_AMBIGUITY_THREADS; i++) {
      amb_threads.emplace_back(&RadarData::ambiguity_thread_calc, this,
                               first_cols[i], last_cols[i]);
    }
    // Join threads to close
    for (int i = 0; i < NUM_AMBIGUITY_THREADS; i++) {
      amb_threads.front().join();
      amb_threads.pop_front();
    }
    ambiguity_mutex.unlock();
  }
}
