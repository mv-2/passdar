#include "sdrplay_api.h"
#include <sdrplay_api_rspDuo.h>
#include <sdrplay_api_tuner.h>
#include <stdio.h>

int main(void) {
  // Open API
  sdrplay_api_ErrT err = sdrplay_api_Open();
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
  sdrplay_api_ErrT err_get = sdrplay_api_GetDevices(
      devices, &numDevs, sizeof(devices) / sizeof(sdrplay_api_DeviceT));
  if (err_get == sdrplay_api_Success) {
    printf("Dev%d: SerNo=%s hwVer=%d tuner=0x%.2x rspDuoMode=0x%.2x\n", 0,
           devices[0].SerNo, devices[0].hwVer, devices[0].tuner,
           devices[0].rspDuoMode);
  } else {
    printf("Didn't Work!!!\n");
  }

  // set up tuner
  sdrplay_api_DeviceT *device = &devices[0];

  // Close API
  sdrplay_api_ErrT err_close = sdrplay_api_Close();
  if (err_close == sdrplay_api_Success) {
    printf("Successfully Closed API\n");
  } else {
    printf("Error Closing API\n");
  }

  return 0;
}
