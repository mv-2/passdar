#include "sdrCapture.h"
#include <cstdint>
#include <iostream>
#include <sdrplay_api.h>
#include <sdrplay_api_dev.h>
#include <sdrplay_api_rx_channel.h>

// GLobal Variables
sdrplay_api_ErrT sdrErr;
sdrplay_api_DeviceT *chosenDevice = NULL;
sdrplay_api_DeviceT devs[1]; // Assuming 1 device
sdrplay_api_DeviceParamsT *deviceParams = NULL;
sdrplay_api_CallbackFnsT cbFns;
sdrplay_api_RxChannelParamsT *chParams;

const unsigned int MaxDevs = 1;

// TODO: Find if this is sane
Receiver::Receiver(uint32_t _fc) { fc = _fc; }

void Receiver::start_api() {
  // open API
  if ((sdrErr = sdrplay_api_Open()) != sdrplay_api_Success) {
    std::cerr << "API open failed " << sdrplay_api_GetErrorString(sdrErr)
              << std::endl;
    exit(1);
  }

  float apiVer = 0.0;
  // Check API versions match
  if ((sdrErr = sdrplay_api_ApiVersion(&apiVer))) {
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
}

// Cleanup function.
// NOTE: May not be necessary but leaving in in case of additional cleanup
// variables later
void Receiver::cleanup() {
  sdrErr = sdrplay_api_Close();
  if (sdrErr != sdrplay_api_Success) {
    std::cerr << "API Close Failed" << std::endl;
  }
}
