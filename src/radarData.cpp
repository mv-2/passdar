#include <GL/gl.h>
#include <GLFW/glfw3.h>
#include <complex.h>
#include <vector>

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

  // preallocate ambiguity
  ambiguity.resize(n_range * n_speed);

  // data copy preallocations
  data_a_copy.resize(cfg.process_cfg.buffer_size);
  data_b_copy.resize(cfg.process_cfg.buffer_size);
}

void RadarData::process_ambiguity(std::atomic<bool> *exit_flag) {
  // Temp value for storing integral
  std::complex<double> int_temp;

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
    for (int v_id = 0; v_id < n_speed; v_id++) {
      for (int r_id = 0; r_id < n_range; r_id++) {
        int_temp = 0.0;
        for (unsigned int i = 0; i < (data_a_copy.size() - n_range); i++) {
          int_temp +=
              data_a_copy[i + r_id] * std::conj(data_b_copy[i]) *
              std::polar(1.0, -2 * r_id * M_PI * i / data_a_copy.size());
        }
        ambiguity[v_id * n_range + r_id] =
            std::abs(int_temp) / data_a_copy.size();
      }
    }
    ambiguity_mutex.unlock();
  }
}
