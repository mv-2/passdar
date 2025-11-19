#include "sdrplay_api.h"
#include <stdio.h>

int main(void) {
  sdrplay_api_ErrT err = sdrplay_api_Open();
  switch (err) {
  case sdrplay_api_Success:
    printf("Successfully Opened!!!\n");
    break;
  default:
    printf("Failure!!!\n");
  }

  sdrplay_api_ErrT err_close = sdrplay_api_Close();
  if (err_close == sdrplay_api_Success) {
    printf("Successfully Closed!!!\n");
  } else {
    printf("UH OH!!!\n");
  }
}
