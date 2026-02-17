#include <GL/gl.h>
#include <GLFW/glfw3.h>
#include <chrono>
#include <cmath>
#include <fftw3.h>
#include <iostream>
#include <thread>
#include <vector>

#include "../hardwareInterface/cfgInterface.h"
#include "radarData.h"

// Wave propagation velocity (I know we aren't in a vacuum)
const double PHASE_VELOCITY = 3e8;

const std::string AMBIGUITY_WISDOM_FILE = "cfg/ambiguity.wisdom";

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
  speed_step = (stream_a_data->frequency[1] - stream_a_data->frequency[0]) *
               1e6 * PHASE_VELOCITY / (2 * cfg.receiver_cfg.fc);
  max_speed = cfg.process_cfg.max_speed;
  n_speed = 2 * max_speed / speed_step + 1;
  max_speed = speed_step * (n_speed - 1) / 2;

  // range points
  n_range = static_cast<int>(cfg.process_cfg.max_range / range_step) + 1;

  // preallocate ambiguity
  ambiguity.resize(n_range * n_speed);

  // data copy preallocations
  sample_buffer_size = cfg.process_cfg.buffer_size;
  data_a_copy = fftw_alloc_complex(sample_buffer_size);
  data_b_copy = fftw_alloc_complex(sample_buffer_size);
  delay_lag_length = sample_buffer_size - n_range;

  // Input and output vectors for fftw algorithm
  delay_lag_product.resize(n_range);
  fftw_amb_out.resize(n_range);
  for (int i = 0; i < n_range; i++) {
    delay_lag_product[i] = fftw_alloc_complex(sample_buffer_size);
    fftw_amb_out[i] = fftw_alloc_complex(sample_buffer_size);
  }

  // Load wisdom file if available
  if (fftw_import_wisdom_from_filename(AMBIGUITY_WISDOM_FILE.c_str()) == 0) {
    std::cout << "Failed to load " << AMBIGUITY_WISDOM_FILE << " file"
              << std::endl;
  }

  // Make FFTW3 plans
  fftw_amb_plans.reserve(n_range);
  for (int i = 0; i < n_range; i++) {
    fftw_plan p = fftw_plan_dft_1d(sample_buffer_size, delay_lag_product[i],
                                   fftw_amb_out[i], FFTW_FORWARD, FFTW_PATIENT);
    fftw_amb_plans.push_back(p);
  }

  // Export wisdom
  if (fftw_export_wisdom_to_filename(AMBIGUITY_WISDOM_FILE.c_str()) == 0) {
    std::cout << "Failed to export " << AMBIGUITY_WISDOM_FILE << " file"
              << std::endl;
  }
}

void RadarData::ambiguity_thread_calc(int row) {
  // FFTshift vel id
  int v_id_swap;

  // execute the FFTW business
  fftw_execute(fftw_amb_plans[row]);

  // Positive frequencies
  for (int v_id = 0; v_id < n_speed / 2 + 1; v_id++) {
    v_id_swap = (v_id > static_cast<int>(sample_buffer_size / 2))
                    ? (v_id + n_speed / 2) % sample_buffer_size
                    : v_id + n_speed / 2;
    ambiguity[row * n_speed + v_id_swap] =
        20 * log10(std::sqrt(
                 fftw_amb_out[row][v_id][0] * fftw_amb_out[row][v_id][0] +
                 fftw_amb_out[row][v_id][1] * fftw_amb_out[row][v_id][1]));
  }

  // Negative frequencies
  for (int v_id = sample_buffer_size - n_speed / 2; v_id < sample_buffer_size;
       v_id++) {
    v_id_swap = (v_id > static_cast<int>(sample_buffer_size / 2))
                    ? (v_id + n_speed / 2) % sample_buffer_size
                    : v_id + n_speed / 2;
    ambiguity[row * n_speed + v_id_swap] =
        20 * log10(std::sqrt(
                 fftw_amb_out[row][v_id][0] * fftw_amb_out[row][v_id][0] +
                 fftw_amb_out[row][v_id][1] * fftw_amb_out[row][v_id][1]));
  }

  return;
}

void RadarData::process_ambiguity(std::atomic<bool> *exit_flag) {
  // Initialise processing threads
  std::deque<std::thread> amb_threads;

  // Loop ambiguity calculation
  while (!exit_flag->load()) {
    // await next sample block
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // lock iqdata and copy samples
    stream_a_data->data_iq->mutex_lock.lock();
    stream_b_data->data_iq->mutex_lock.lock();
    for (int i = 0; i < sample_buffer_size; i++) {
      // buffer a
      data_a_copy[i][0] = stream_a_data->data_iq->samples[i][0];
      data_a_copy[i][1] = stream_a_data->data_iq->samples[i][1];
      // buffer b
      data_b_copy[i][0] = stream_b_data->data_iq->samples[i][0];
      data_b_copy[i][1] = stream_b_data->data_iq->samples[i][1];
    }
    stream_a_data->data_iq->mutex_lock.unlock();
    stream_b_data->data_iq->mutex_lock.unlock();

    // Pre calculate delay signal
    for (int r_id = 0; r_id < n_range; r_id++) {
      for (int j = 0; j < delay_lag_length; j++) {
        // Complex conjugate multiplication for delay lag (a+bi)*(c-d*i) = (a*c
        // + b*d) + (b*c - a*d)*i

        // real
        delay_lag_product[r_id][j][0] =
            data_a_copy[j + r_id][0] * data_b_copy[j][0] +
            data_a_copy[j + r_id][1] * data_b_copy[j][1];
        // imag
        delay_lag_product[r_id][j][1] =
            data_a_copy[j + r_id][1] * data_b_copy[j][0] -
            data_a_copy[j + r_id][0] * data_b_copy[j][1];
      }
    }

    // Calculate ambiguity
    ambiguity_mutex.lock();
    for (int i = 0; i < n_range; i++) {
      amb_threads.emplace_back(&RadarData::ambiguity_thread_calc, this, i);
    }
    // Join threads to close
    for (int i = 0; i < n_range; i++) {
      amb_threads.front().join();
      amb_threads.pop_front();
    }
    ambiguity_mutex.unlock();
  }

  // free FFTW arrays
  for (int i = 0; i < n_range; i++) {
    fftw_free(delay_lag_product[i]);
    fftw_free(fftw_amb_out[i]);
    fftw_destroy_plan(fftw_amb_plans[i]);
  }
  fftw_free(data_a_copy);
  fftw_free(data_b_copy);

  fftw_cleanup();
}
