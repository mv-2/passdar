#include <GL/gl.h>
#include <GLFW/glfw3.h>
#include <complex.h>
#include <thread>
#include <vector>

#include "cfgInterface.h"
#include "radarData.h"

// Number of available threads for ambiguity calculation
const int NUM_THREADS = 12;

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

  // FFTshift vel and invert range
  int v_id_swap;

  // Ambiguity calculation perform FFTshift at same time
  for (int v_id = first_col; v_id < last_col; v_id++) {
    v_id_swap = (v_id + n_speed / 2) % n_speed;
    for (int r_id = 0; r_id < n_range; r_id++) {
      int_temp = 0.0;
      for (unsigned int i = 0; i < (data_a_copy.size() - n_range); i++) {
        int_temp += data_a_copy[i + r_id] * std::conj(data_a_copy[i]) *
                    std::polar(1.0, static_cast<double>(-2 * (int)i * v_id) *
                                        M_PI / static_cast<double>(n_speed));
      }
      // NOTE: This is technically incorrect because absolute value of int_temp
      // should be divided by the pulse length but as only relative ambiguity
      // matters the offset can be applied as a linear subtraction to the db
      // value later
      ambiguity[(n_range - r_id - 1) * n_speed + v_id_swap] =
          20 * log10(std::abs(int_temp));
    }
  }
}

void RadarData::process_ambiguity(std::atomic<bool> *exit_flag) {
  // Initialise processing threads
  std::thread amb_threads[NUM_THREADS];
  int first_col, last_col;

  while (!exit_flag->load()) {
    // await next sample block
    sleep(1);

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
    for (int i = 0; i < NUM_THREADS; i++) {
      // ternary used here is ugly but effective
      first_col = i * (n_speed / NUM_THREADS);
      last_col = (i == (NUM_THREADS - 1)) ? n_speed + 1
                                          : (i + 1) * n_speed / NUM_THREADS;
      amb_threads[i] =
          std::thread([&] { ambiguity_thread_calc(first_col, last_col); });
    }
    // Join threads to close
    for (int i = 0; i < NUM_THREADS; i++) {
      amb_threads[i].join();
    }
    ambiguity_mutex.unlock();
  }
}
