#include <Eigen/Core>
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

// constant file location to store FFTW wisdom file
const std::string AMBIGUITY_WISDOM_FILE = "cfg/ambiguity.wisdom";

RadarData::RadarData(Config cfg, SpecData *_stream_a_data,
                     SpecData *_stream_b_data)
    : ambiguity(), detection() {

  // not ready
  ready_flag.store(false);

  // assign data stream pointers
  stream_a_data = _stream_a_data;
  stream_b_data = _stream_b_data;
  sample_block_size = cfg.process_cfg.sample_block_size;
  total_buffer_size =
      static_cast<int>(static_cast<double>(cfg.receiver_cfg.fs) /
                       cfg.process_cfg.frequency_step);

  // Sample Frequency
  sample_frequency = cfg.receiver_cfg.fs;

  // Calculate range step
  range_step = PHASE_VELOCITY / sample_frequency;

  // range points
  speed_step = (stream_a_data->frequency[1] - stream_a_data->frequency[0]) *
               1e6 * PHASE_VELOCITY / (2 * cfg.receiver_cfg.fc);
  max_speed = cfg.process_cfg.max_speed;

  // calculate max_speed in line with possible speed steps
  n_speed = max_speed / speed_step;
  ambiguity_columns = 2 * n_speed - 1;
  max_speed = speed_step * n_speed;

  // Check if buffer length is divisble by n_speed
  if (total_buffer_size % n_speed != 0) {
    std::cerr
        << "Sample buffer size is not divisible by number of speed points: "
        << total_buffer_size % n_speed << "\n";
  }

  // range points
  n_range = static_cast<int>(cfg.process_cfg.max_range / range_step) + 1;

  // Size clutter matrix
  clutter_basis.resize(cfg.process_cfg.filter_length, n_range);
  clutter_basis.setZero();

  // preallocate ambiguity
  ambiguity = LeftRight<double>(n_range * ambiguity_columns);

  // data copy preallocations
  reference_data.resize(sample_block_size);
  observation_data.resize(sample_block_size);
  delay_lag_length = sample_block_size - n_range;

  // Input and output vectors for fftw algorithm
  delay_lag_product.resize(n_range);
  fftw_amb_out.resize(n_range);
  for (int i = 0; i < n_range; i++) {
    delay_lag_product[i].resize(total_buffer_size);
    fftw_amb_out[i].resize(total_buffer_size);
  }

  // Compute twiddles
  twiddle_factors.resize((n_speed - 1) * (total_buffer_size / n_speed - 1));
  for (int j = 1; j < total_buffer_size / n_speed; j++) {
    for (int i = 1; i < n_speed; i++) {
      twiddle_factors[(j - 1) * (n_speed - 1) + (i - 1)] = std::complex<double>(
          cos(static_cast<double>(i * j) * FFTW_FORWARD * 2 * M_PI /
              static_cast<double>(total_buffer_size)),
          sin(static_cast<double>(i * j) * FFTW_FORWARD * 2 * M_PI /
              static_cast<double>(total_buffer_size)));
    }
  }

  // Set display scale for calculation
  ambiguity_scale = cfg.process_cfg.ambiguity_scale;

  // Calculate range & speed values
  // Calculate range values
  for (int i = 0; i < n_range; i++) {
    range_vals.push_back(static_cast<double>(i) * range_step - range_step / 2);
  }
  for (int i = -n_speed; i < n_speed; i++) {
    speed_vals.push_back(static_cast<double>(i) * speed_step - speed_step / 2);
  }

  // CFAR detection array preallocation
  detection = LeftRight<double>(n_range * ambiguity_columns);

  // Assign config for CFAR processing
  detection_config = cfg.detection_config;

  // Ambiguity spectrum type
  amb_type = cfg.process_cfg.amb_fft_type;

  // Initialise last time and ambiguity_rate
  last_time = std::chrono::high_resolution_clock::now();
  ambiguity_rate = 0.0f;
}

void RadarData::initialise_fftw_plans(void) {
  // Load wisdom file if available
  if (fftw_import_wisdom_from_filename(AMBIGUITY_WISDOM_FILE.c_str()) == 0) {
    std::cout << "Failed to load " << AMBIGUITY_WISDOM_FILE << " file"
              << std::endl;
  }

  fftw_amb_plans.reserve(n_range);
  switch (amb_type) {
  case AmbiguityType::Full:
    for (int i = 0; i < n_range; i++) {
      fftw_plan p = fftw_plan_dft_1d(
          total_buffer_size,
          reinterpret_cast<fftw_complex *>(delay_lag_product[i].data()),
          reinterpret_cast<fftw_complex *>(fftw_amb_out[i].data()),
          FFTW_FORWARD, FFTW_EXHAUSTIVE);
      fftw_amb_plans.push_back(p);
    }

    break;
  case AmbiguityType::Pruned:
    for (int i = 0; i < n_range; i++) {
      fftw_plan p = fftw_plan_many_dft(
          1, &n_speed, total_buffer_size / n_speed,
          reinterpret_cast<fftw_complex *>(delay_lag_product[i].data()), NULL,
          total_buffer_size / n_speed, 1,
          reinterpret_cast<fftw_complex *>(fftw_amb_out[i].data()), NULL, 1,
          n_speed, FFTW_FORWARD, FFTW_EXHAUSTIVE);
      fftw_amb_plans.push_back(p);
    }
  }

  // Export wisdom
  if (fftw_export_wisdom_to_filename(AMBIGUITY_WISDOM_FILE.c_str()) == 0) {
    std::cout << "Failed to export " << AMBIGUITY_WISDOM_FILE << " file"
              << std::endl;
  }
}

void RadarData::ambiguity_row_calc(int row) {
  // FFTshift vel id
  int v_id_swap;

  // Range swap
  int range_row_id = n_range - row - 1;

  // execute the FFTW business
  fftw_execute(fftw_amb_plans[row]);

  switch (amb_type) {
  case AmbiguityType::Full:
    switch (ambiguity_scale) {
    case DisplayScale::Linear:
      // Positive frequency assignment
      for (int v_id = 0; v_id < n_speed; v_id++) {
        v_id_swap = n_speed + v_id - 1;
        ambiguity.write(std::abs(fftw_amb_out[row][v_id]),
                        range_row_id * ambiguity_columns + v_id_swap);
      }

      // Negative frequency assignment
      v_id_swap = n_speed - 1;
      for (int v_id = total_buffer_size - n_speed - 1; v_id < total_buffer_size;
           v_id++) {
        ambiguity.write(std::abs(fftw_amb_out[row][v_id]),
                        range_row_id * ambiguity_columns + v_id_swap);
        v_id_swap--;
      }
      break;
    case DisplayScale::dB:
      // Positive frequency assignment
      for (int v_id = 0; v_id < n_speed; v_id++) {
        v_id_swap = n_speed + v_id - 1;
        ambiguity.write(20 * log10(std::abs(fftw_amb_out[row][v_id])),
                        range_row_id * ambiguity_columns + v_id_swap);
      }

      // Negative frequency assignment
      v_id_swap = 0;
      for (int v_id = total_buffer_size - n_speed - 1; v_id < total_buffer_size;
           v_id++) {
        ambiguity.write(20 * log10(std::abs(fftw_amb_out[row][v_id])),
                        range_row_id * ambiguity_columns + v_id_swap);
        v_id_swap++;
      }
    }
    break;

  case AmbiguityType::Pruned:

    // Apply twiddles and adjustments
    std::complex<double> amb_adj0, amb_adjN;

    // For FFT mapping N = sample_buffer_size, K = n_speed
    fftw_amb_out[row][0] += fftw_amb_out[row][total_buffer_size - n_speed];

    // Apply twiddle factors and adjustments for pruned FFT
    for (int i = 1; i < n_speed; i++) {
      // Adjustment values
      amb_adj0 = fftw_amb_out[row][i];
      amb_adjN = fftw_amb_out[row][(total_buffer_size - n_speed) + i];

      fftw_amb_out[row][i] =
          amb_adj0 +
          amb_adjN * twiddle_factors[(total_buffer_size / n_speed - 2) *
                                         (n_speed - 1) +
                                     (i - 1)];

      // NOTE: change of sign for amb_adjN term is intentional for complex
      // conjugate multiplication
      fftw_amb_out[row][i] =
          amb_adj0 + std::conj(amb_adjN) *
                         twiddle_factors[(total_buffer_size / n_speed - 2) *
                                             (n_speed - 1) +
                                         (n_speed - i - 1)];
    }

    // Sum FFTs
    for (int j = 1; j < (total_buffer_size / n_speed - 1); j++) {
      fftw_amb_out[row][0] + fftw_amb_out[row][j * n_speed];

      for (int i = 1; i < n_speed; i++) {
        fftw_amb_out[row][i] +=
            fftw_amb_out[row][i + j * n_speed] *
            twiddle_factors[(j - 1) * (n_speed - 1) + (i - 1)];
      }
      for (int i = 1; i < n_speed; i++) {
        fftw_amb_out[row][(total_buffer_size - n_speed) + i] += std::conj(
            fftw_amb_out[row][i + j * n_speed] *
            twiddle_factors[(j - 1) * (n_speed - 1) + (n_speed + i - 1)]);
      }
    }

    switch (ambiguity_scale) {
    case DisplayScale::Linear:
      // Positive frequency assignment
      for (int v_id = 0; v_id < n_speed; v_id++) {
        v_id_swap = n_speed + v_id - 1;
        ambiguity.write(std::abs(fftw_amb_out[row][v_id]),
                        range_row_id * ambiguity_columns + v_id_swap);
      }

      // Negative frequency assignment
      v_id_swap = 0;
      for (int v_id = total_buffer_size - 1; v_id > total_buffer_size - n_speed;
           v_id--) {
        ambiguity.write(std::abs(fftw_amb_out[row][v_id]),
                        range_row_id * ambiguity_columns + v_id_swap);
        v_id_swap++;
      }
      break;
    case DisplayScale::dB:
      // Positive frequency assignment
      for (int v_id = 0; v_id < n_speed; v_id++) {
        v_id_swap = n_speed + v_id - 1;
        ambiguity.write(20 * log10(std::abs(fftw_amb_out[row][v_id])),
                        range_row_id * ambiguity_columns + v_id_swap);
      }

      // Negative frequency assignment
      v_id_swap = 0;
      for (int v_id = total_buffer_size - 1; v_id > total_buffer_size - n_speed;
           v_id--) {
        ambiguity.write(20 * log10(std::abs(fftw_amb_out[row][v_id])),
                        range_row_id * ambiguity_columns + v_id_swap);
        v_id_swap++;
      }
    }
  }

  return;
}

void RadarData::radar_process(std::atomic<bool> *exit_flag,
                              std::mutex *fftw_plan_mutex) {
  // Initialise fftw plans
  fftw_plan_mutex->lock();
  initialise_fftw_plans();
  fftw_plan_mutex->unlock();

  // Initialise processing threads
  std::deque<std::thread> amb_threads;

  // Time point and duration for ambiguity_rate calc
  std::chrono::time_point<std::chrono::high_resolution_clock> time_now;
  std::chrono::duration<float, std::milli> duration;

  // Loop ambiguity calculation
  while (!exit_flag->load()) {
    // Determine update rate
    time_now = std::chrono::high_resolution_clock::now();
    duration = time_now - last_time;
    ambiguity_rate =
        1e3 / std::chrono::duration<float, std::milli>(duration).count();
    last_time = std::chrono::high_resolution_clock::now();

    // await next sample block
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // lock iqdata and copy samples
    stream_a_data->ambiguity_iq->mutex_lock.lock();
    stream_b_data->ambiguity_iq->mutex_lock.lock();
    for (int i = 0; i < sample_block_size; i++) {
      reference_data[i] =
          std::complex<double>(stream_a_data->ambiguity_iq->samples[i][0],
                               stream_a_data->ambiguity_iq->samples[i][1]);
      observation_data[i] =
          std::complex<double>(stream_b_data->ambiguity_iq->samples[i][0],
                               stream_b_data->ambiguity_iq->samples[i][1]);
    }
    stream_a_data->ambiguity_iq->mutex_lock.unlock();
    stream_b_data->ambiguity_iq->mutex_lock.unlock();

    // Filtering step
    // Create clutter basis
    for (int i = 0; i < n_range; i++) {
      clutter_basis.col(i) = Eigen::Map<Eigen::VectorXcd>(
          &reference_data[n_range - i - 1], clutter_basis.rows());
    }

    // Cast observation_data to Eigen compatible format
    Eigen::VectorXcd observation_vector = Eigen::Map<Eigen::VectorXcd>(
        observation_data.data(), clutter_basis.rows());

    // Compute projection residual
    Eigen::VectorXcd projection_residual =
        clutter_basis * (clutter_basis.adjoint() * clutter_basis).inverse() *
        clutter_basis.adjoint() * observation_vector;

    for (int i = 0; i < projection_residual.size(); i++) {
      observation_data[i] = observation_data[i] - projection_residual[i];
    }

    // Pre calculate delay signal
    // TODO: Make this a std::vector of eigen::vectorxcd objects
    for (int r_id = 0; r_id < n_range; r_id++) {
      for (int j = 0; j < delay_lag_length; j++) {
        delay_lag_product[r_id][j] =
            reference_data[j] * std::conj(observation_data[j + r_id]);
      }
    }

    // Calculate ambiguity
    for (int i = 0; i < n_range; i++) {
      amb_threads.emplace_back(&RadarData::ambiguity_row_calc, this, i);
    }

    // Join threads to close
    for (int i = 0; i < n_range; i++) {
      amb_threads.front().join();
      amb_threads.pop_front();
    }

    // Finish writing to ambiguity
    ambiguity.swap_lr();

    // CFAR processing
    CA_CFAR();

    // Ready after first loop
    ready_flag.store(true);
  }

  // free FFTW arrays
  for (int i = 0; i < n_range; i++) {
    fftw_destroy_plan(fftw_amb_plans[i]);
  }

  fftw_cleanup();
}

void RadarData::CA_CFAR() {
  // Vector for averaging
  double ave_vals;
  int n_vals;

  for (int r_id = 0; r_id < n_range; r_id++) {
    for (int v_id = 0; v_id < ambiguity_columns; v_id++) {
      // reset ave_vals and n_vals
      ave_vals = 0.0;
      n_vals = 0;

      // Set search limits
      int r_min = std::max(0, r_id - detection_config.range_window);
      int r_max = std::min(n_range, r_id + detection_config.range_window + 1);
      int v_min = std::max(0, v_id - detection_config.speed_window);
      int v_max =
          std::min(ambiguity_columns, v_id + detection_config.speed_window + 1);

      // Add first cells outside of guard region
      for (int r_ave_id = r_min;
           r_ave_id < r_id - detection_config.range_guard + 1; r_ave_id++) {
        for (int v_ave_id = v_min;
             v_ave_id < v_id - detection_config.speed_guard + 1; v_ave_id++) {
          ave_vals += ambiguity.read(r_ave_id * ambiguity_columns + v_ave_id);
          n_vals++;
        }
        for (int v_ave_id = v_id + detection_config.speed_guard;
             v_ave_id < v_max; v_ave_id++) {
          ave_vals += ambiguity.read(r_ave_id * ambiguity_columns + v_ave_id);
          n_vals++;
        }
      }

      // Add cells in guard region
      for (int r_ave_id = r_min; r_ave_id < r_max; r_ave_id++) {
        for (int v_ave_id = v_min; v_ave_id < v_max; v_ave_id++) {
          ave_vals += ambiguity.read(r_ave_id * ambiguity_columns + v_ave_id);
          n_vals++;
        }
      }

      // Add cells following guard region
      for (int r_ave_id = r_id + detection_config.range_guard; r_ave_id < r_max;
           r_ave_id++) {
        for (int v_ave_id = v_min;
             v_ave_id < v_id - detection_config.speed_guard + 1; v_ave_id++) {
          ave_vals += ambiguity.read(r_ave_id * ambiguity_columns + v_ave_id);
          n_vals++;
        }
        for (int v_ave_id = v_id + detection_config.speed_guard;
             v_ave_id < v_max; v_ave_id++) {
          ave_vals += ambiguity.read(r_ave_id * ambiguity_columns + v_ave_id);
          n_vals++;
        }
      }

      // Determine if ambiguity breaks threshold
      detection.write(
          ambiguity.read(r_id * ambiguity_columns + v_id) >=
              (detection_config.cfar_multiplier * ave_vals / n_vals),
          r_id * ambiguity_columns + v_id);
    }
  }
}
