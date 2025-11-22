#include "sdrplay_api.h"
#include <sdrplay_api_control.h>
#include <sdrplay_api_rspDuo.h>
#include <sdrplay_api_rx_channel.h>
#include <sdrplay_api_tuner.h>
#include <stdio.h>

int main(void) {
  // Open API
  sdrplay_api_ErrT err = sdrplay_api_Open();
  sdrplay_api_DeviceParamsT *device_params = NULL;
  sdrplay_api_RxChannelParamsT *ch_params;
  switch (err) {
  case sdrplay_api_Success:
    printf("Successfully Opened!!!\n");
    break;
  default:
    printf("Failure!!!\n");
  }

  // Test getting device IDs
  sdrplay_api_DeviceT devices[6];
  unsigned int numDevs;
  err = sdrplay_api_GetDevices(devices, &numDevs,
                               sizeof(devices) / sizeof(sdrplay_api_DeviceT));
  if (err == sdrplay_api_Success) {
    printf("Dev%d: SerNo=%s hwVer=%d tuner=0x%.2x rspDuoMode=0x%.2x\n", 0,
           devices[0].SerNo, devices[0].hwVer, devices[0].tuner,
           devices[0].rspDuoMode);
  } else {
    printf("Didn't Work!!!\n");
  }

  // set up tuner
  sdrplay_api_DeviceT *device = &devices[0];
  device->tuner = sdrplay_api_Tuner_A;
  device->rspDuoMode = sdrplay_api_RspDuoMode_Master;
  device->rspDuoSampleFreq = 6000000.0;

  if ((err = sdrplay_api_SelectDevice(device)) != sdrplay_api_Success) {
    printf("Select Device Failed\n");
    goto UnlockDeviceAndCloseApi;
  }

  sdrplay_api_UnlockDeviceApi();
  if ((err = sdrplay_api_GetDeviceParams(device->dev, &device_params)) !=
      sdrplay_api_Success) {
    printf("Get Device Params Failed\n");
    goto CloseApi;
  }

  if (device_params == NULL) {
    printf("Device Params NULL");
    goto CloseApi;
  }

  if (device_params->devParams != NULL) {
    device_params->devParams->fsFreq.fsHz = 8000000.0;
  }

  ch_params = (device->tuner == sdrplay_api_Tuner_B)
                  ? device_params->rxChannelB
                  : device_params->rxChannelA;
  if (ch_params != NULL) {
    ch_params->tunerParams.rfFreq.rfHz = 220000000.0;
    ch_params->tunerParams.bwType = sdrplay_api_BW_1_536;
    ch_params->tunerParams.gain.gRdB = 40;
    ch_params->tunerParams.gain.LNAstate = 5;
    ch_params->ctrlParams.agc.enable = sdrplay_api_AGC_DISABLE;
  }
  // if ((err = sdrplay_api_Init(device->dev, NULL, NULL)) !=
  //     sdrplay_api_Success) {
  //   printf("Not initialised");
  // }

  // Close API
UnlockDeviceAndCloseApi:
  sdrplay_api_UnlockDeviceApi();
CloseApi:
  err = sdrplay_api_Close();
  if (err == sdrplay_api_Success) {
    printf("Successfully Closed API\n");
  } else {
    printf("Error Closing API\n");
  }

  return 0;
}
