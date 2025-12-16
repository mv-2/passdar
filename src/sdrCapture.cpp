#include "sdrCapture.h"
#include <cstdint>
#include <iostream>
#include <sdrplay_api.h>
#include <sdrplay_api_callback.h>
#include <sdrplay_api_control.h>
#include <sdrplay_api_dev.h>
#include <sdrplay_api_rx_channel.h>

// GLobal Variables
sdrplay_api_ErrT sdrErr;
sdrplay_api_DeviceT *chosenDevice = NULL;
sdrplay_api_DeviceT devs[1]; // Assuming 1 device
sdrplay_api_DeviceParamsT *deviceParams = NULL;
sdrplay_api_CallbackFnsT cbFns;
sdrplay_api_RxChannelParamsT *chParams;

// One (1) device
const unsigned int MaxDevs = 1;

// TODO: Find if this is sane
Receiver::Receiver(uint32_t _fc, int _agc_bandwidth_nr, int _agc_set_point_nr,
                   int _gRdB_A, int _gRdB_B, int _lna_state, int _dec_factor,
                   sdrplay_api_If_kHzT _ifType, sdrplay_api_Bw_MHzT _bwType,
                   bool _rf_notch_enable, bool _dab_notch_enable) {
  fc = _fc;
  agc_bandwidth_nr = _agc_bandwidth_nr;
  agc_set_point_nr = _agc_set_point_nr;
  gRdB_A = _gRdB_A;
  gRdB_B = _gRdB_B;
  lna_state = _lna_state;
  dec_factor = _dec_factor;
  ifType = _ifType;
  bwType = _bwType;
  rf_notch_enable = _rf_notch_enable;
  dab_notch_enable = _dab_notch_enable;
}

void Receiver::start_api() {
  // open API
  if ((sdrErr = sdrplay_api_Open()) != sdrplay_api_Success) {
    std::cerr << "API open failed " << sdrplay_api_GetErrorString(sdrErr)
              << std::endl;
    exit(1);
  }

  float apiVer = 0.0;
  // Check API versions match
  if ((sdrErr = sdrplay_api_ApiVersion(&apiVer)) != sdrplay_api_Success) {
    std::cerr << "API version check failed "
              << sdrplay_api_GetErrorString(sdrErr) << std::endl;
    cleanup();
    exit(1);
  }

  if (apiVer != SDRPLAY_API_VERSION) {
    std::cerr << "API versions do not match" << std::endl;
    cleanup();
    exit(1);
  }

  // assign device handle and set tuner mode
  get_device();

  // Set all parameters following validation
  set_device_parameters();

  // TODO: PARAMETER VALIDATION
}

void Receiver::stop_api() {
  if ((sdrErr = sdrplay_api_Uninit(chosenDevice->dev)) != sdrplay_api_Success) {
    std::cerr << "sdrplay_apit_Uninit failed "
              << sdrplay_api_GetErrorString(sdrErr) << std::endl;
    cleanup();
    exit(1);
  }
  sdrplay_api_ReleaseDevice(chosenDevice);
  sdrplay_api_UnlockDeviceApi();
  sdrplay_api_Close();
}

void Receiver::initialise() {
  if ((sdrErr = sdrplay_api_Init(chosenDevice->dev, &cbFns, NULL)) !=
      sdrplay_api_Success) {
    std::cerr << "sdrplay_api_init failed "
              << sdrplay_api_GetErrorString(sdrErr) << std::endl;
    sdrplay_api_Close();
    exit(1);
  }
}

void Receiver::get_device() {
  unsigned int i;
  unsigned int ndev;
  unsigned int chosenIdx = 0;

  // Lock API during selection
  if ((sdrErr = sdrplay_api_LockDeviceApi()) != sdrplay_api_Success) {
    std::cerr << "API Lock failed " << sdrplay_api_GetErrorString(sdrErr)
              << std::endl;
    cleanup();
    exit(1);
  }

  // Find available device(s)
  // NOTE: Only one device will be available at any point in this project
  if ((sdrErr = sdrplay_api_GetDevices(devs, &ndev, MaxDevs))) {
    std::cerr << "Get devices failed " << sdrplay_api_GetErrorString(sdrErr)
              << std::endl;
    sdrplay_api_UnlockDeviceApi();
    cleanup();
    exit(1);
  }

  // Check if device found
  if (ndev == 0) {
    std::cerr << "No devices found" << std::endl;
    sdrplay_api_UnlockDeviceApi();
    cleanup();
    exit(1);
  }

  // Ensure device type is correct
  if (devs[0].hwVer != SDRPLAY_RSPduo_ID) {
    std::cerr << "Device is not RspDuo" << std::endl;
    sdrplay_api_UnlockDeviceApi();
    cleanup();
    exit(1);
  }

  // Assign device and mode
  chosenDevice = &devs[chosenIdx];
  chosenDevice->tuner = sdrplay_api_Tuner_Both;
  chosenDevice->rspDuoMode = sdrplay_api_RspDuoMode_Dual_Tuner;

  std::cout << "S/N:" << devs[0].SerNo << std::endl;
  std::cout << "Hardware Version:" << devs[0].hwVer << std::endl;
  std::cout << "Tuner:" << chosenDevice->tuner << std::endl;
  std::cout << "RspDuoMode:" << chosenDevice->rspDuoMode << std::endl;

  // Select device
  if ((sdrErr = sdrplay_api_SelectDevice(chosenDevice)) !=
      sdrplay_api_Success) {
    std::cerr << "Device Selection Failed "
              << sdrplay_api_GetErrorString(sdrErr) << std::endl;
    cleanup();
    exit(1);
  }

  // Enable debugging
  if ((sdrErr = sdrplay_api_DebugEnable(chosenDevice->dev,
                                        sdrplay_api_DbgLvl_Verbose)) !=
      sdrplay_api_Success) {
    std::cerr << "Debug eable failed " << sdrplay_api_GetErrorString(sdrErr)
              << std::endl;
    cleanup();
    exit(1);
  }

  return;
}

void Receiver::set_device_parameters() {
  // Get existing parameters
  if ((sdrErr =
           sdrplay_api_GetDeviceParams(chosenDevice->dev, &deviceParams))) {
    std::cerr << "Get devioce params failed "
              << sdrplay_api_GetErrorString(sdrErr) << std::endl;
    cleanup();
    exit(1);
  }

  // Ensure deviceParams has been assigned correctly
  if (deviceParams == NULL) {
    std::cerr << "deviceParams is NULL" << std::endl;
    cleanup();
    exit(1);
  }

  // set USB mode to bulk
  // TODO: Check if this is sane
  deviceParams->devParams->mode = sdrplay_api_BULK;

  // Configure receiver settings for both channels (if parameters were
  // previously set correctly)
  chParams = deviceParams->rxChannelA;

  // Ensure not NULL
  if (chParams == NULL) {
    std::cerr << "chParams is NULL" << std::endl;
    cleanup();
    exit(1);
  }

  // Tuner frequency (??)
  chParams->tunerParams.rfFreq.rfHz = fc;

  // AGC
  // TODO: INVESTIGATE
  chParams->ctrlParams.agc.enable = sdrplay_api_AGC_DISABLE;
  if (agc_bandwidth_nr == 5) {
    chParams->ctrlParams.agc.enable = sdrplay_api_AGC_5HZ;
  } else if (agc_bandwidth_nr == 50) {
    chParams->ctrlParams.agc.enable = sdrplay_api_AGC_50HZ;
  } else if (agc_bandwidth_nr == 100) {
    chParams->ctrlParams.agc.enable = sdrplay_api_AGC_100HZ;
  }
  if (chParams->ctrlParams.agc.enable != sdrplay_api_AGC_DISABLE) {
    chParams->ctrlParams.agc.setPoint_dBfs =
        (0 < agc_set_point_nr) ? 0 : agc_set_point_nr;
  }

  // Set gain reduction and LNA state
  deviceParams->rxChannelA->tunerParams.gain.gRdB = gRdB_A;
  deviceParams->rxChannelB->tunerParams.gain.gRdB = gRdB_B;
  deviceParams->rxChannelA->tunerParams.gain.LNAstate = lna_state;
  deviceParams->rxChannelB->tunerParams.gain.LNAstate = lna_state;

  // set decimation parameters
  chParams->ctrlParams.decimation.enable = 1;
  chParams->ctrlParams.decimation.decimationFactor = dec_factor;
  chParams->tunerParams.ifType = ifType;
  chParams->tunerParams.bwType = bwType;

  // Notch filter flags
  chParams->rspDuoTunerParams.rfNotchEnable = rf_notch_enable;
  chParams->rspDuoTunerParams.rfDabNotchEnable = dab_notch_enable;

  // Callback function assignment
  cbFns.StreamACbFn = stream_a_callback_static;
  cbFns.StreamBCbFn = stream_b_callback_static;
  cbFns.EventCbFn = event_callback_static;
}

// Receiver A data callback
void Receiver::stream_a_callback(short *xi, short *xq,
                                 sdrplay_api_StreamCbParamsT *params,
                                 unsigned int numSamples, unsigned int reset,
                                 void *cbContext) {
  // Allocate memory for buffer w/ IQ data
  short int *buffer_A = (short int *)malloc(2 * numSamples * sizeof(short));

  // if (buffer_A == NULL) {
  //   std::cerr << "Stream A malloc error" << std::endl;
  //   exit(1);
  // }
  //
  // // Fill buffer
  // for (unsigned int i = 0; i < numSamples; i++) {
  //   buffer_A[2 * i] = xi[i];
  //   buffer_A[2 * i + 1] = xq[i];
  // }

  return;
}

// Receiver B data callback
void Receiver::stream_b_callback(short *xi, short *xq,
                                 sdrplay_api_StreamCbParamsT *params,
                                 unsigned int numSamples, unsigned int reset,
                                 void *cbContext) {
  // Allocate memory for buffer w/ IQ data
  short int *buffer_B = (short int *)malloc(2 * numSamples * sizeof(short));

  // if (buffer_B == NULL) {
  //   std::cerr << "Stream B malloc error" << std::endl;
  //   exit(1);
  // }
  //
  // // Fill buffer
  // for (unsigned int i = 0; i < numSamples; i++) {
  //   buffer_B[2 * i] = xi[i];
  //   buffer_B[2 * i + 1] = xq[i];
  // }

  return;
}

// Event callback (funnily enough)
void Receiver::event_callback(sdrplay_api_EventT eventId,
                              sdrplay_api_TunerSelectT tuner,
                              sdrplay_api_EventParamsT *params,
                              void *cbContext) {}

// Cleanup function.
// NOTE: May not be necessary but leaving in in case of additional cleanup
// variables later
void Receiver::cleanup() {
  sdrErr = sdrplay_api_Close();
  if (sdrErr != sdrplay_api_Success) {
    std::cerr << "API Close Failed" << std::endl;
  }
}
