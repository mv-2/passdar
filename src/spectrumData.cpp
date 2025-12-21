#include "spectrumData.h"
#include <mutex>
#include <unistd.h>

const std::complex<double> I = std::complex<double>(0, 1.0);
const std::complex<double> COMPLEX_ZERO = std::complex<double>(0.0, 0.0);

// TODO: Figure out what I'm doing here. Maybe NFFT as arg?
SpecData::SpecData(int _max_length) {
  max_length = _max_length;
  data_iq = new ReceiverRawIQ(max_length);
  std::vector<std::complex<double>> _spectrum(max_length);
  spectrum = _spectrum;
}

void SpecData::update_data(short *xi, short *xq, unsigned int numSamples) {
  data_iq->mutex_lock.lock();
  for (unsigned int i = 0; i < numSamples; i++) {
    data_iq->samples.pop_front();
    data_iq->samples.push_back(
        std::complex<double>((double)xi[i], (double)xq[i]));
  }
  data_iq->mutex_lock.unlock();
}

void SpecData::calc_dft() {
  data_iq->mutex_lock.lock();
  for (int k = 0; k < max_length; k++) {
    spectrum[k] = COMPLEX_ZERO;
    for (int n = 0; n < max_length; n++) {
      spectrum[k] += data_iq->samples.at(n) *
                     std::polar(1.0, -2.0 * M_PI * (double)k * (double)n /
                                         (double)max_length);
    }
  }
  data_iq->mutex_lock.unlock();
}

void SpecData::process_data(bool(loop_exit)(void)) {
  while (!loop_exit()) {
    mutex_lock.lock();
    calc_dft();
    mutex_lock.unlock();
    sleep(1);
  }
}

void SpecData::set_plot_datablock(FILE *plot_pipe, std::string id) {
  mutex_lock.lock();
  fprintf(plot_pipe, "$data_%s << EOD\n", id.c_str());
  for (unsigned int i = 0; i < max_length; i++) {
    fprintf(plot_pipe, "%d %f\n", i, std::abs(spectrum[i]));
  }
  mutex_lock.unlock();
  fprintf(plot_pipe, "EOD\n");
}

ReceiverRawIQ::ReceiverRawIQ(unsigned int _max_length) {
  max_length = _max_length;
  samples = std::deque<std::complex<double>>(max_length);
}
