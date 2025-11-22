#include <assert.h>
#include <cjson/cJSON.h>
#include <fcntl.h>
#include <sdrplay_api_control.h>
#include <sdrplay_api_rspDuo.h>
#include <sdrplay_api_tuner.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>

#include "fourier.h"
#include "sdrplay_api.h"

static struct termios old_termios, current_termios;

enum masterSlaveSelectT { Master = 1, Slave = 0 };

struct Config {
  sdrplay_api_TunerSelectT tuner;
  enum masterSlaveSelectT master_slave;
  double rspDuoSampleFreq;
  double fsFreqHz;
  double rfFreqHz;
  int gRdB;
  int LNAstate;
  int maxGain;
  int minGain;
  sdrplay_api_Bw_MHzT bandwidth;
  sdrplay_api_If_kHzT IF;
  sdrplay_api_AgcControlT AGC;
};

struct Config load_config(const char *path) {
  FILE *fp = fopen(path, "rb");

  fseek(fp, 0, SEEK_END);
  long length = ftell(fp);
  fseek(fp, 0, SEEK_SET);

  char *file_buffer = (char *)malloc(length + 1);

  fread(file_buffer, 1, length, fp);
  file_buffer[length] = '\0';
  fclose(fp);

  cJSON *root = cJSON_Parse(file_buffer);

  struct Config cfg;

  cJSON *tuner_item = cJSON_GetObjectItemCaseSensitive(root, "tuner");
  // assume the user (me) is not an idiot
  if (strcmp(tuner_item->valuestring, "A")) {
    cfg.tuner = sdrplay_api_Tuner_A;
  } else {
    cfg.tuner = sdrplay_api_Tuner_B;
  }

  cJSON *master_slave_item =
      cJSON_GetObjectItemCaseSensitive(root, "master_slave");
  // assume the user (me) is not an idiot
  if (strcmp(master_slave_item->valuestring, "Master")) {
    cfg.master_slave = Master;
  } else {
    cfg.master_slave = Slave;
  }

  cJSON *rspDuoSampleFreq_item =
      cJSON_GetObjectItemCaseSensitive(root, "rspDuoSampleFreq");
  cfg.rspDuoSampleFreq = rspDuoSampleFreq_item->valuedouble;
  cJSON *fsFreqHz_item = cJSON_GetObjectItemCaseSensitive(root, "fsFreqHz");
  cfg.rspDuoSampleFreq = rspDuoSampleFreq_item->valuedouble;
  cJSON *rfFreqHz_item = cJSON_GetObjectItemCaseSensitive(root, "rfFreqHz");
  cfg.rspDuoSampleFreq = rspDuoSampleFreq_item->valuedouble;
  cJSON *gRdB_item = cJSON_GetObjectItemCaseSensitive(root, "gRdB");
  cfg.rspDuoSampleFreq = rspDuoSampleFreq_item->valueint;
  cJSON *LNAstate_item = cJSON_GetObjectItemCaseSensitive(root, "LNAstate");
  cfg.LNAstate = LNAstate_item->valueint;
  cJSON *maxGain_item = cJSON_GetObjectItemCaseSensitive(root, "maxGain");
  cfg.maxGain = maxGain_item->valueint;
  cJSON *minGain_item = cJSON_GetObjectItemCaseSensitive(root, "minGain");
  cfg.minGain = minGain_item->valueint;
  cJSON *bandwidth_item = cJSON_GetObjectItemCaseSensitive(root, "bandwidth");

  // TODO: Change this to handle undefined as else case
  if (strcmp(bandwidth_item->valuestring, "sdrplay_api_BW_Undefined")) {
    cfg.bandwidth = sdrplay_api_BW_Undefined;
  } else if (strcmp(bandwidth_item->valuestring, "sdrplay_api_BW_0_200")) {
    cfg.bandwidth = sdrplay_api_BW_0_200;
  } else if (strcmp(bandwidth_item->valuestring, "sdrplay_api_BW_0_300")) {
    cfg.bandwidth = sdrplay_api_BW_0_300;
  } else if (strcmp(bandwidth_item->valuestring, "sdrplay_api_BW_0_600")) {
    cfg.bandwidth = sdrplay_api_BW_0_600;
  } else if (strcmp(bandwidth_item->valuestring, "sdrplay_api_BW_1_536")) {
    cfg.bandwidth = sdrplay_api_BW_1_536;
  } else if (strcmp(bandwidth_item->valuestring, "sdrplay_api_BW_5_000")) {
    cfg.bandwidth = sdrplay_api_BW_5_000;
  } else if (strcmp(bandwidth_item->valuestring, "sdrplay_api_BW_6_000")) {
    cfg.bandwidth = sdrplay_api_BW_6_000;
  } else if (strcmp(bandwidth_item->valuestring, "sdrplay_api_BW_7_000")) {
    cfg.bandwidth = sdrplay_api_BW_7_000;
  } else {
    cfg.bandwidth = sdrplay_api_BW_8_000;
  }

  // TODO: Change this to handle undefined as else case
  cJSON *IF_item = cJSON_GetObjectItemCaseSensitive(root, "IF");
  if (strcmp(IF_item->valuestring, "sdrplay_api_IF_Undefined")) {
    cfg.IF = sdrplay_api_IF_Undefined;
  } else if (strcmp(IF_item->valuestring, "sdrplay_api_IF_Zero")) {
    cfg.IF = sdrplay_api_IF_Zero;
  } else if (strcmp(IF_item->valuestring, "sdrplay_api_IF_0_450")) {
    cfg.IF = sdrplay_api_IF_0_450;
  } else if (strcmp(IF_item->valuestring, "sdrplay_api_IF_1_620")) {
    cfg.IF = sdrplay_api_IF_1_620;
  } else {
    cfg.IF = sdrplay_api_IF_2_048;
  }

  // TODO: Change this to handle undefined as else case
  cJSON *AGC_item = cJSON_GetObjectItemCaseSensitive(root, "AGC");
  if (strcmp(AGC_item->valuestring, "sdrplay_api_AGC_DISABLE")) {
    cfg.AGC = sdrplay_api_AGC_DISABLE;
  } else if (strcmp(AGC_item->valuestring, "sdrplay_api_AGC_100HZ")) {
    cfg.AGC = sdrplay_api_AGC_100HZ;
  } else if (strcmp(AGC_item->valuestring, "sdrplay_api_AGC_50HZ")) {
    cfg.AGC = sdrplay_api_AGC_50HZ;
  } else if (strcmp(AGC_item->valuestring, "sdrplay_api_AGC_5HZ")) {
    cfg.AGC = sdrplay_api_AGC_5HZ;
  } else {
    cfg.AGC = sdrplay_api_AGC_CTRL_EN;
  }

  cfg.minGain = minGain_item->valueint;

  cJSON_Delete(root);
  free(file_buffer);

  return cfg;
}

void init_keyboard() {
  tcgetattr(STDIN_FILENO, &old_termios);
  current_termios = old_termios;
  current_termios.c_lflag &= ~(ICANON | ECHO);
  tcsetattr(STDIN_FILENO, TCSANOW, &current_termios);
  fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);
}

void close_keyboard() { tcsetattr(STDIN_FILENO, TCSANOW, &old_termios); }

int kbhit() {
  fd_set fds;
  struct timeval tv;
  tv.tv_sec = 0;
  tv.tv_usec = 0;

  FD_ZERO(&fds);
  FD_SET(STDIN_FILENO, &fds);

  return select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) > 0;
}

int getch() {
  char ch;
  if (read(STDIN_FILENO, &ch, 1) > 0) {
    return ch;
  }
  return -1;
}

int masterInitialised = 0;
int slaveUninitialised = 0;
float result_bufferA[2000];
float result_bufferB;

sdrplay_api_DeviceT *chosenDevice = NULL;

void StreamACallback(short *xi, short *xq, sdrplay_api_StreamCbParamsT *params,
                     unsigned int numSamples, unsigned int reset,
                     void *cbContext) {
  if (reset) {
    printf("sdrplay_api_StreamACallback: numSamples=%d\n", numSamples);
  }

  dft_real(xi, xq, result_bufferA, numSamples);
  // for (int i = 0; i < numSamples; i++) {
  //   printf("%d: %f\n", i, result_bufferA[i]);
  // }

  return;
}

void StreamBCallback(short *xi, short *xq, sdrplay_api_StreamCbParamsT *params,
                     unsigned int numSamples, unsigned int reset,
                     void *cbContext) {
  if (reset) {
    printf("sdrplay_api_StreamBCallback: numSamples=%d\n", numSamples);
  }

  return;
}

void EventCallback(sdrplay_api_EventT eventId, sdrplay_api_TunerSelectT tuner,
                   sdrplay_api_EventParamsT *params, void *cbContext) {
  switch (eventId) {
  case sdrplay_api_GainChange:
    printf("sdrplay_api_EventCb: %s, tuner=%s gRdB=%d lnaGRdB=%d "
           "systemGain=%.2f\n",
           "sdrplay_api_GainChange",
           (tuner == sdrplay_api_Tuner_A) ? "sdrplay_api_Tuner_A"
                                          : "sdrplay_api_Tuner_B",
           params->gainParams.gRdB, params->gainParams.lnaGRdB,
           params->gainParams.currGain);
    break;

  case sdrplay_api_PowerOverloadChange:
    printf("sdrplay_api_PowerOverloadChange: tuner=%s "
           "powerOverloadChangeType=%s\n",
           (tuner == sdrplay_api_Tuner_A) ? "sdrplay_api_Tuner_A"
                                          : "sdrplay_api_Tuner_B",
           (params->powerOverloadParams.powerOverloadChangeType ==
            sdrplay_api_Overload_Detected)
               ? "sdrplay_api_Overload_Detected"
               : "sdrplay_api_Overload_Corrected");
    // Send update message to acknowledge power overload message received
    sdrplay_api_Update(chosenDevice->dev, tuner,
                       sdrplay_api_Update_Ctrl_OverloadMsgAck,
                       sdrplay_api_Update_Ext1_None);
    break;

  case sdrplay_api_RspDuoModeChange:
    printf(
        "sdrplay_api_EventCb: %s, tuner=%s modeChangeType=%s\n",
        "sdrplay_api_RspDuoModeChange",
        (tuner == sdrplay_api_Tuner_A) ? "sdrplay_api_Tuner_A"
                                       : "sdrplay_api_Tuner_B",
        (params->rspDuoModeParams.modeChangeType ==
         sdrplay_api_MasterInitialised)
            ? "sdrplay_api_MasterInitialised"
        : (params->rspDuoModeParams.modeChangeType == sdrplay_api_SlaveAttached)
            ? "sdrplay_api_SlaveAttached"
        : (params->rspDuoModeParams.modeChangeType == sdrplay_api_SlaveDetached)
            ? "sdrplay_api_SlaveDetached"
        : (params->rspDuoModeParams.modeChangeType ==
           sdrplay_api_SlaveInitialised)
            ? "sdrplay_api_SlaveInitialised"
        : (params->rspDuoModeParams.modeChangeType ==
           sdrplay_api_SlaveUninitialised)
            ? "sdrplay_api_SlaveUninitialised"
            : "unknown type");
    if (params->rspDuoModeParams.modeChangeType ==
        sdrplay_api_MasterInitialised) {
      masterInitialised = 1;
    }
    if (params->rspDuoModeParams.modeChangeType ==
        sdrplay_api_SlaveUninitialised) {
      slaveUninitialised = 1;
    }
    break;

  case sdrplay_api_DeviceRemoved:
    printf("sdrplay_api_EventCb: %s\n", "sdrplay_api_DeviceRemoved");
    break;

  default:
    printf("sdrplay_api_EventCb: %d, unknown event\n", eventId);
    break;
  }
}

void usage(void) {
  printf("Usage: <compilename>.out <config path>\n");
  exit(1);
}

int main(int argc, char *argv[]) {
  sdrplay_api_DeviceT devs[1];
  unsigned int ndev;
  int i;
  float ver = 0.0;
  sdrplay_api_ErrT err;
  sdrplay_api_DeviceParamsT *deviceParams = NULL;
  sdrplay_api_CallbackFnsT cbFns;
  sdrplay_api_RxChannelParamsT *chParams;

  struct Config cfg;
  char c;

  unsigned int chosenIdx = 0;

  // init keyboard for control
  init_keyboard();

  // load config
  if (argc == 2) {
    cfg = load_config(argv[1]);
  } else {
    usage();
  }

  // Open API
  if ((err = sdrplay_api_Open()) != sdrplay_api_Success) {
    printf("sdrplay_api_Open failed %s\n", sdrplay_api_GetErrorString(err));
  } else {
    // Enable debug logging output
    if ((err = sdrplay_api_DebugEnable(NULL, 1)) != sdrplay_api_Success) {
      printf("sdrplay_api_DebugEnable failed %s\n",
             sdrplay_api_GetErrorString(err));
    }

    // Check API versions match
    if ((err = sdrplay_api_ApiVersion(&ver)) != sdrplay_api_Success) {
      printf("sdrplay_api_ApiVersion failed %s\n",
             sdrplay_api_GetErrorString(err));
    }
    if (ver != SDRPLAY_API_VERSION) {
      printf("API version don't match (local=%.2f dll=%.2f)\n",
             SDRPLAY_API_VERSION, ver);
      goto CloseApi;
    }

    // Lock API while device selection is performed
    sdrplay_api_LockDeviceApi();

    // Fetch list of available devices
    if ((err = sdrplay_api_GetDevices(
             devs, &ndev, sizeof(devs) / sizeof(sdrplay_api_DeviceT))) !=
        sdrplay_api_Success) {
      printf("sdrplay_api_GetDevices failed %s\n",
             sdrplay_api_GetErrorString(err));
      goto UnlockDeviceAndCloseApi;
    }

    printf("MaxDevs=%lu NumDevs=%d\n",
           sizeof(devs) / sizeof(sdrplay_api_DeviceT), ndev);

    if (ndev == 0) {
      printf("No Devices Found\n");
      goto CloseApi;
    }
    // Don't need to select device in the same manner as this will always be
    // an RSPDuo
    chosenIdx = 0;
    chosenDevice = &devs[chosenIdx];

    // Only rspDuo should be available
    assert(chosenDevice->hwVer == SDRPLAY_RSPduo_ID);
    if (chosenDevice->rspDuoMode & sdrplay_api_RspDuoMode_Master)
      ;
    chosenDevice->tuner = cfg.tuner;
    switch (cfg.master_slave) {
    case Master:
      chosenDevice->rspDuoMode = sdrplay_api_RspDuoMode_Master;
      chosenDevice->rspDuoSampleFreq = cfg.rspDuoSampleFreq;
      break;
    case Slave:
      chosenDevice->rspDuoMode = sdrplay_api_RspDuoMode_Single_Tuner;
    }

    // Select chosen device
    if ((err = sdrplay_api_SelectDevice(chosenDevice)) != sdrplay_api_Success) {
      printf("sdrplay_api_SelectDevice failed %s\n",
             sdrplay_api_GetErrorString(err));
      goto UnlockDeviceAndCloseApi;
    }

    // Unlock API now that device is selected
    sdrplay_api_UnlockDeviceApi();

    // Retrieve device parameters so they can be changed if wanted
    if ((err = sdrplay_api_GetDeviceParams(chosenDevice->dev, &deviceParams)) !=
        sdrplay_api_Success) {
      printf("sdrplay_api_GetDeviceParams failed %s\n",
             sdrplay_api_GetErrorString(err));
      goto CloseApi;
    }

    // Check for NULL pointers before changing settings
    if (deviceParams == NULL) {
      printf("sdrplay_api_GetDeviceParams returned NULL deviceParams "
             "pointer\n");
      goto CloseApi;
    }

    // Configure dev parameters
    if (deviceParams->devParams !=
        NULL) // This will be NULL for slave devices as only the master can
              // change these parameters
    {
      // Only need to update non-default settings
      if (cfg.master_slave == Slave) {
        // Change from default Fs  to 8MHz
        deviceParams->devParams->fsFreq.fsHz = cfg.fsFreqHz;
      } else {
        // Can't change Fs in master/slave mode
      }
    }

    // Configure tuner parameters (depends on selected Tuner which set of
    // parameters to use)
    chParams = (chosenDevice->tuner == sdrplay_api_Tuner_B)
                   ? deviceParams->rxChannelB
                   : deviceParams->rxChannelA;
    if (chParams != NULL) {
      chParams->tunerParams.rfFreq.rfHz = cfg.rfFreqHz;
      chParams->tunerParams.bwType = cfg.bandwidth;
      if (cfg.master_slave == Slave) // Change single tuner mode to ZIF
      {
        chParams->tunerParams.ifType = cfg.IF;
      }
      chParams->tunerParams.gain.gRdB = cfg.gRdB;
      chParams->tunerParams.gain.LNAstate = cfg.LNAstate;

      // Disable AGC
      chParams->ctrlParams.agc.enable = cfg.AGC;
    } else {
      printf("sdrplay_api_GetDeviceParams returned NULL chParams pointer\n");
      goto CloseApi;
    }

    // Assign callback functions to be passed to sdrplay_api_Init()
    cbFns.StreamACbFn = StreamACallback;
    cbFns.StreamBCbFn = StreamBCallback;
    cbFns.EventCbFn = EventCallback;

    // Now we're ready to start by calling the initialisation function
    // This will configure the device and start streaming
    if ((err = sdrplay_api_Init(chosenDevice->dev, &cbFns, NULL)) !=
        sdrplay_api_Success) {
      printf("sdrplay_api_Init failed %s\n", sdrplay_api_GetErrorString(err));
      if (err == sdrplay_api_StartPending) // This can happen if we're starting
                                           // in master/slave mode as a slave
                                           // and the master is not yet running
      {
        while (1) {
          usleep(1000);
          if (masterInitialised) // Keep polling flag set in event callback
                                 // until the master is initialised
          {
            // Redo call - should succeed this time
            if ((err = sdrplay_api_Init(chosenDevice->dev, &cbFns, NULL)) !=
                sdrplay_api_Success) {
              printf("sdrplay_api_Init failed %s\n",
                     sdrplay_api_GetErrorString(err));
            }
            goto CloseApi;
          }
          printf("Waiting for master to initialise\n");
        }
      } else {
        goto CloseApi;
      }
    }

    while (1) // Small loop allowing user to control gain reduction in
              // +/-1dB steps using keyboard keys
    {
      if (kbhit()) {
        c = getch();
        if (c == 'q') {
          break;
        } else if (c == 'u') {
          chParams->tunerParams.gain.gRdB += 1;
          // Limit it to a maximum of 59dB
          if (chParams->tunerParams.gain.gRdB > cfg.maxGain)
            chParams->tunerParams.gain.gRdB = cfg.minGain;
          if ((err = sdrplay_api_Update(chosenDevice->dev, chosenDevice->tuner,
                                        sdrplay_api_Update_Tuner_Gr,
                                        sdrplay_api_Update_Ext1_None)) !=
              sdrplay_api_Success) {
            printf("sdrplay_api_Update sdrplay_api_Update_Tuner_Gr failed "
                   "%s\n",
                   sdrplay_api_GetErrorString(err));
            break;
          }
        } else if (c == 'd') {
          chParams->tunerParams.gain.gRdB -= 1;
          // Limit it to a minimum of 20dB
          if (chParams->tunerParams.gain.gRdB < cfg.minGain)
            chParams->tunerParams.gain.gRdB = cfg.maxGain;
          if ((err = sdrplay_api_Update(chosenDevice->dev, chosenDevice->tuner,
                                        sdrplay_api_Update_Tuner_Gr,
                                        sdrplay_api_Update_Ext1_None)) !=
              sdrplay_api_Success) {
            printf("sdrplay_api_Update sdrplay_api_Update_Tuner_Gr failed "
                   "%s\n",
                   sdrplay_api_GetErrorString(err));
            break;
          }
        }
      }
      usleep(100);
    }

    // Finished with device so uninitialise it
    if ((err = sdrplay_api_Uninit(chosenDevice->dev)) != sdrplay_api_Success) {
      printf("sdrplay_api_Uninit failed %s\n", sdrplay_api_GetErrorString(err));
      if (err == sdrplay_api_StopPending) // This can happen if we're stopping
                                          // in master/slave mode as a master
                                          // and the slave is still running
      {
        while (1) {
          usleep(1000);
          if (slaveUninitialised) // Keep polling flag set in event callback
                                  // until the slave is uninitialised
          {
            // Repeat call - should succeed this time
            if ((err = sdrplay_api_Uninit(chosenDevice->dev)) !=
                sdrplay_api_Success) {
              printf("sdrplay_api_Uninit failed %s\n",
                     sdrplay_api_GetErrorString(err));
            }
            slaveUninitialised = 0;
            goto CloseApi;
          }
          printf("Waiting for slave to uninitialise\n");
        }
      }
      goto CloseApi;
    }

    // Release device (make it available to other applications)
    sdrplay_api_ReleaseDevice(chosenDevice);

  UnlockDeviceAndCloseApi:
    // Unlock API
    sdrplay_api_UnlockDeviceApi();

  CloseApi:
    // Close API
    sdrplay_api_Close();
  }

  close_keyboard();

  return 0;
}
