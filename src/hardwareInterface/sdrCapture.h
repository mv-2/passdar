#ifndef SDRCAPTURE_H
#define SDRCAPTURE_H

#include <sdrplay_api.h>
#include <sdrplay_api_callback.h>
#include <sdrplay_api_tuner.h>
#include <stdint.h>

#include "../processing/spectrumData.h"
#include "cfgInterface.h"

/**
 * @brief Receiver class to run data capture process with SDRPlay RSPDuo
 */
class Receiver {
public:
  /* @brief Receiver class constructor
   *
   * @param receiver_cfg ReceiverConfig object as defined by user
   */
  Receiver(ReceiverConfig receiverCfg);

  /*
   * @brief Data capture looping function.
   *
   * @param stream_a_data Pointer to SpecData object for stream A.
   * @param stream_b_data Pointer to SpecData object for stream B.
   * @param exit_flag Pointer to atomic<bool> flag denoting required end of
   * process.
   */
  void run_capture(SpecData *stream_a_data, SpecData *stream_b_data,
                   std::atomic<bool> *exit_flag);

  void
  start_api(void); ///  Open API, validate version and set device parameters

  void initialise(void); /// Initialise receiver device

  void stop_api(void); /// Stop API and cleanup

  ReceiverConfig receiver_cfg; /// User defined config parameters

  std::atomic<bool>
      ready_flag; /// Flag denoting when processing data is ready to be accessed

private:
  /// @name  API control functions
  /// @{
  void get_device();            /// Get available device address
  void set_device_parameters(); /// Setup device with required parameters
  void cleanup();               /// Close API
  /// @}

  /// @name Callback functions
  /// @{
  /// Default callback signature to receiver A
  void stream_a_callback(short *xi, short *xq,
                         sdrplay_api_StreamCbParamsT *params,
                         unsigned int numSamples, unsigned int reset,
                         void *cbContext);

  /// Default callback signature to receiver B
  void stream_b_callback(short *xi, short *xq,
                         sdrplay_api_StreamCbParamsT *params,
                         unsigned int numSamples, unsigned int reset,
                         void *cbContext);

  /// Default callback for device event
  void event_callback(sdrplay_api_EventT eventId,
                      sdrplay_api_TunerSelectT tuner,
                      sdrplay_api_EventParamsT *params, void *cbContext);

  /// Static casting C-style default receiver A callback for C++ use
  static void stream_a_callback_static(short *xi, short *xq,
                                       sdrplay_api_StreamCbParamsT *params,
                                       unsigned int numSamples,
                                       unsigned int reset, void *cbContext) {
    static_cast<Receiver *>(cbContext)->stream_a_callback(
        xi, xq, params, numSamples, reset, cbContext);
  }

  /// Static casting C-style default receiver B callback for C++ use
  static void stream_b_callback_static(short *xi, short *xq,
                                       sdrplay_api_StreamCbParamsT *params,
                                       unsigned int numSamples,
                                       unsigned int reset, void *cbContext) {
    static_cast<Receiver *>(cbContext)->stream_b_callback(
        xi, xq, params, numSamples, reset, cbContext);
  }

  /// Static casting C-style default event callback for C++ use
  static void event_callback_static(sdrplay_api_EventT eventId,
                                    sdrplay_api_TunerSelectT tuner,
                                    sdrplay_api_EventParamsT *params,
                                    void *cbContext) {
    static_cast<Receiver *>(cbContext)->event_callback(eventId, tuner, params,
                                                       cbContext);
  }
  /// @}
};

#endif
