#ifndef SDRCAPTURE_H
#define SDRCAPTURE_H

#include <cstdint>
#include <sdrplay_api.h>
#include <sdrplay_api_callback.h>
#include <sdrplay_api_tuner.h>
#include <stdint.h>

#include "spectrumData.h"

class Receiver {
public:
  /* Receiver class constructor
   *
   * @param fc centre frequency for tuner
   * @param agc_bandwidth_nr AGC bandwidth in Hz. Must line up with SDRPlay API
   * allowed values. 0 to disable.
   * @param agc_set_point_nr AGC set point. Set to max(agc_set_point_nr, 0)
   * @param gRdB_A Channel A gain reduction.
   * @param gRdB_B Channel B gain reduction.
   * @param lna_state LNA state as per SDRplay API specification
   * @param dec_factor Decimation/downsampling factor
   * @param ifType IF type as per sdrplay_api_If_kHzT allowed values
   * @param bwType BW type as per sdrplay_api_Bw_MHzT allowed values
   * @param rf_notch_enable RF notch filter flag
   * @param dab_notch_enable DAB notch filter flag
   */
  Receiver(uint32_t fc, int agc_bandwidth_nr, int agc_set_point_nr, int gRdB_A,
           int gRdB_B, int lna_state, int dec_factor,
           sdrplay_api_If_kHzT ifType, sdrplay_api_Bw_MHzT bwType,
           bool rf_notch_enable, bool dab_notch_enable);

  /*
   * Data capture looping function.
   *
   * @param stream_a_data Pointer to SpecData object for stream A.
   * @param stream_b_data Pointer to SpecData object for stream B.
   * @param loop_exit Function with no arguments which returns true on condition
   * to break looping.
   */
  void run_capture(SpecData *stream_a_data, SpecData *stream_b_data,
                   bool (*break_loop)(void));

  /*
   * Open API, validate version and set device parameters
   */
  void start_api();

  /*
   * Initialise receiver device
   */
  void initialise();

  /*
   * Stop API and cleanup
   */
  void stop_api();

  // Required params
  uint32_t fc;
  int agc_bandwidth_nr;
  int agc_set_point_nr;
  int _gain_reduction_nr_a;
  int gRdB_A;
  int gRdB_B;
  int lna_state;
  int dec_factor;
  sdrplay_api_If_kHzT ifType;
  sdrplay_api_Bw_MHzT bwType;
  bool rf_notch_enable;
  bool dab_notch_enable;

private:
  // API control functions
  void get_device();
  void set_device_parameters();
  void cleanup();

  // Callback functions
  void stream_a_callback(short *xi, short *xq,
                         sdrplay_api_StreamCbParamsT *params,
                         unsigned int numSamples, unsigned int reset,
                         void *cbContext);

  void stream_b_callback(short *xi, short *xq,
                         sdrplay_api_StreamCbParamsT *params,
                         unsigned int numSamples, unsigned int reset,
                         void *cbContext);

  void event_callback(sdrplay_api_EventT eventId,
                      sdrplay_api_TunerSelectT tuner,
                      sdrplay_api_EventParamsT *params, void *cbContext);

  // Static casting of functions
  static void stream_a_callback_static(short *xi, short *xq,
                                       sdrplay_api_StreamCbParamsT *params,
                                       unsigned int numSamples,
                                       unsigned int reset, void *cbContext) {
    static_cast<Receiver *>(cbContext)->stream_a_callback(
        xi, xq, params, numSamples, reset, cbContext);
  }

  static void stream_b_callback_static(short *xi, short *xq,
                                       sdrplay_api_StreamCbParamsT *params,
                                       unsigned int numSamples,
                                       unsigned int reset, void *cbContext) {
    static_cast<Receiver *>(cbContext)->stream_b_callback(
        xi, xq, params, numSamples, reset, cbContext);
  }

  static void event_callback_static(sdrplay_api_EventT eventId,
                                    sdrplay_api_TunerSelectT tuner,
                                    sdrplay_api_EventParamsT *params,
                                    void *cbContext) {
    static_cast<Receiver *>(cbContext)->event_callback(eventId, tuner, params,
                                                       cbContext);
  }
};

#endif
