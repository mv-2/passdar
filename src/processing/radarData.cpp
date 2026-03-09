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

// constant file location to store FFTW wisdom files
const std::string AMBIGUITY_WISDOM_FILE = "cfg/ambiguity.wisdom";

RadarData::RadarData(Config cfg, SpecData *_stream_a_data,
                     SpecData *_stream_b_data) {
  // not ready
  ready_flag.store(false);

  // assign data stream pointers
  stream_a_data = _stream_a_data;
  stream_b_data = _stream_b_data;
  sample_buffer_size = cfg.process_cfg.buffer_size;

  // Sample Frequency
  sample_frequency = cfg.receiver_cfg.fs;

  // Calculate range step
  range_step = PHASE_VELOCITY / sample_frequency;
  max_allowable_range = sample_buffer_size * range_step / 2;

  // range points
  speed_step = (stream_a_data->frequency[1] - stream_a_data->frequency[0]) *
               1e6 * PHASE_VELOCITY / (2 * cfg.receiver_cfg.fc);
  max_speed = cfg.process_cfg.max_speed;

  // calculate max_speed in line with possible speed steps
  n_speed = max_speed / speed_step;
  ambiguity_columns = 2 * n_speed - 1;
  max_speed = speed_step * n_speed;

  // Check if buffer length is divisble by n_speed
  if (sample_buffer_size % n_speed != 0) {
    std::cerr
        << "Sample buffer size is not divisible by number of speed points: "
        << sample_buffer_size % n_speed << "\n";
  }

  // range points
  n_range = static_cast<int>(cfg.process_cfg.max_range / range_step) + 1;

  // preallocate ambiguity
  ambiguity.resize(n_range * ambiguity_columns);

  // data copy preallocations
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
    fftw_plan p = fftw_plan_many_dft(
        1, &n_speed, sample_buffer_size / n_speed, delay_lag_product[i], NULL,
        sample_buffer_size / n_speed, 1, fftw_amb_out[i], NULL, 1, n_speed,
        FFTW_FORWARD, FFTW_PATIENT);
    fftw_amb_plans.push_back(p);
  }

  // Export wisdom
  if (fftw_export_wisdom_to_filename(AMBIGUITY_WISDOM_FILE.c_str()) == 0) {
    std::cout << "Failed to export " << AMBIGUITY_WISDOM_FILE << " file"
              << std::endl;
  }

  // Compute twiddles
  twiddle_factors =
      fftw_alloc_complex((n_speed - 1) * (sample_buffer_size / n_speed - 1));
  for (int j = 1; j < sample_buffer_size / n_speed; j++) {
    for (int i = 1; i < n_speed; i++) {
      twiddle_factors[(j - 1) * (n_speed - 1) + (i - 1)][0] =
          cos(static_cast<double>(i * j) * FFTW_FORWARD * 2 * M_PI /
              static_cast<double>(sample_buffer_size));
      twiddle_factors[(j - 1) * (n_speed - 1) + (i - 1)][1] =
          sin(static_cast<double>(i * j) * FFTW_FORWARD * 2 * M_PI /
              static_cast<double>(sample_buffer_size));
    }
  }

  // Set display scale for calculation
  ambiguity_scale = cfg.process_cfg.ambiguity_scale;
}

void RadarData::ambiguity_thread_calc(int row) {
  // FFTshift vel id
  int v_id_swap;

  // Range swap
  int range_row_id = row;

  // execute the FFTW business
  fftw_execute(fftw_amb_plans[row]);

  // Apply twiddles and adjustments
  fftw_complex amb_adj0, amb_adjN;

  // TODO: Use actual complex operations where possible

  // For FFT mapping N = sample_buffer_size, K = n_speed
  fftw_amb_out[row][0][0] += fftw_amb_out[row][sample_buffer_size - n_speed][0];
  fftw_amb_out[row][0][1] += fftw_amb_out[row][sample_buffer_size - n_speed][1];

  // TODO: try cascade summation to fix accuracy issues
  for (int i = 1; i < n_speed; i++) {
    // Adjustment values
    amb_adj0[0] = fftw_amb_out[row][i][0];
    amb_adj0[1] = fftw_amb_out[row][i][1];
    amb_adjN[0] = fftw_amb_out[row][(sample_buffer_size - n_speed) + i][0];
    amb_adjN[1] = fftw_amb_out[row][(sample_buffer_size - n_speed) + i][1];

    fftw_amb_out[row][i][0] =
        amb_adj0[0] +
        amb_adjN[0] *
            twiddle_factors[(sample_buffer_size / n_speed - 2) * (n_speed - 1) +
                            (i - 1)][0];
    fftw_amb_out[row][i][1] =
        amb_adj0[1] +
        amb_adjN[1] *
            twiddle_factors[(sample_buffer_size / n_speed - 2) * (n_speed - 1) +
                            (i - 1)][1];

    // NOTE: change of sign for amb_adjN term is intentional for complex
    // conjugate multiplication
    fftw_amb_out[row][sample_buffer_size - n_speed + i][0] =
        amb_adj0[0] +
        amb_adjN[0] *
            twiddle_factors[(sample_buffer_size / n_speed - 2) * (n_speed - 1) +
                            (n_speed - i - 1)][0];
    fftw_amb_out[row][sample_buffer_size - n_speed + i][1] =
        amb_adj0[1] -
        amb_adjN[1] *
            twiddle_factors[(sample_buffer_size / n_speed - 2) * (n_speed - 1) +
                            (n_speed - i - 1)][1];
  }
  for (int j = 1; j < (sample_buffer_size / n_speed - 1); j++) {
    fftw_amb_out[row][0][0] += fftw_amb_out[row][j * n_speed][0];
    fftw_amb_out[row][0][1] += fftw_amb_out[row][j * n_speed][1];

    for (int i = 1; i < n_speed; i++) {
      fftw_amb_out[row][i][0] +=
          fftw_amb_out[row][i + j * n_speed][0] *
          twiddle_factors[(j - 1) * (n_speed - 1) + (i - 1)][0];
      fftw_amb_out[row][i][1] +=
          fftw_amb_out[row][i + j * n_speed][1] *
          twiddle_factors[(j - 1) * (n_speed - 1) + (i - 1)][1];
    }
    for (int i = 1; i < n_speed; i++) {
      // NOTE: change of sign is intentional for complex conjugate
      // multiplication
      fftw_amb_out[row][(sample_buffer_size - n_speed) + i][0] +=
          fftw_amb_out[row][i + j * n_speed][0] *
          twiddle_factors[(j - 1) * (n_speed - 1) + (n_speed + i - 1)][0];
      fftw_amb_out[row][(sample_buffer_size - n_speed) + i][1] -=
          fftw_amb_out[row][i + j * n_speed][1] *
          twiddle_factors[(j - 1) * (n_speed - 1) + (n_speed + i - 1)][1];
    }
  }

  // Positive frequency assignment
  switch (ambiguity_scale) {
  case DisplayScale::Linear:
    for (int v_id = 0; v_id < n_speed; v_id++) {
      v_id_swap = n_speed + v_id - 1;
      ambiguity[range_row_id * ambiguity_columns + v_id_swap] =
          sqrt(fftw_amb_out[row][v_id][0] * fftw_amb_out[row][v_id][0] +
               fftw_amb_out[row][v_id][1] * fftw_amb_out[row][v_id][1]);
    }

    // Negative frequency assignment
    v_id_swap = 0;
    for (int v_id = sample_buffer_size - 1; v_id > sample_buffer_size - n_speed;
         v_id--) {
      ambiguity[range_row_id * ambiguity_columns + v_id_swap] =
          sqrt(fftw_amb_out[row][v_id][0] * fftw_amb_out[row][v_id][0] +
               fftw_amb_out[row][v_id][1] * fftw_amb_out[row][v_id][1]);
      v_id_swap++;
    }
    break;
  case DisplayScale::dB:
    for (int v_id = 0; v_id < n_speed; v_id++) {
      v_id_swap = n_speed + v_id - 1;
      ambiguity[range_row_id * ambiguity_columns + v_id_swap] =
          10 * log10(fftw_amb_out[row][v_id][0] * fftw_amb_out[row][v_id][0] +
                     fftw_amb_out[row][v_id][1] * fftw_amb_out[row][v_id][1]);
    }

    // Negative frequency assignment
    v_id_swap = 0;
    for (int v_id = sample_buffer_size - 1; v_id > sample_buffer_size - n_speed;
         v_id--) {
      ambiguity[range_row_id * ambiguity_columns + v_id_swap] =
          10 * log10(fftw_amb_out[row][v_id][0] * fftw_amb_out[row][v_id][0] +
                     fftw_amb_out[row][v_id][1] * fftw_amb_out[row][v_id][1]);
      v_id_swap++;
    }
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
    stream_a_data->ambiguity_iq->mutex_lock.lock();
    stream_b_data->ambiguity_iq->mutex_lock.lock();
    for (int i = 0; i < sample_buffer_size; i++) {
      // buffer a
      data_a_copy[i][0] = stream_a_data->ambiguity_iq->samples[i][0];
      data_a_copy[i][1] = stream_a_data->ambiguity_iq->samples[i][1];
      // buffer b
      data_b_copy[i][0] = stream_b_data->ambiguity_iq->samples[i][0];
      data_b_copy[i][1] = stream_b_data->ambiguity_iq->samples[i][1];
    }
    stream_a_data->ambiguity_iq->mutex_lock.unlock();
    stream_b_data->ambiguity_iq->mutex_lock.unlock();

    // Pre calculate delay signal
    for (int r_id = 0; r_id < n_range; r_id++) {
      for (int j = 0; j < delay_lag_length; j++) {
        // Complex conjugate multiplication for delay lag (a+bi)*(c-d*i) =
        // (a*c
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
    // Ready after first loop
    ready_flag.store(true);
  }

  // free FFTW arrays
  for (int i = 0; i < n_range; i++) {
    fftw_free(delay_lag_product[i]);
    fftw_free(fftw_amb_out[i]);
    fftw_destroy_plan(fftw_amb_plans[i]);
  }
  fftw_free(data_a_copy);
  fftw_free(data_b_copy);
  fftw_free(twiddle_factors);

  fftw_cleanup();
}
