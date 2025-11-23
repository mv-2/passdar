#include "fourier.h"
#include <assert.h>
#include <cjson/cJSON.h>
#include <fcntl.h>
#include <inttypes.h>
#include <sdrplay_api.h>
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
  if (strcmp(tuner_item->valuestring, "A") == 0) {
    cfg.tuner = sdrplay_api_Tuner_A;
  } else if (strcmp(tuner_item->valuestring, "B") == 0) {
    cfg.tuner = sdrplay_api_Tuner_B;
  } else if (strcmp(tuner_item->valuestring, "Both") == 0) {
    cfg.tuner = sdrplay_api_Tuner_Both;
  }

  cJSON *master_slave_item =
      cJSON_GetObjectItemCaseSensitive(root, "master_slave");
  // assume the user (me) is not an idiot
  if (strcmp(master_slave_item->valuestring, "Master") == 0) {
    cfg.master_slave = Master;
  } else {
    cfg.master_slave = Slave;
  }

  cJSON *rspDuoSampleFreq_item =
      cJSON_GetObjectItemCaseSensitive(root, "rspDuoSampleFreq");
  cfg.rspDuoSampleFreq = rspDuoSampleFreq_item->valuedouble;
  cJSON *fsFreqHz_item = cJSON_GetObjectItemCaseSensitive(root, "fsFreqHz");
  cfg.fsFreqHz = fsFreqHz_item->valuedouble;
  cJSON *rfFreqHz_item = cJSON_GetObjectItemCaseSensitive(root, "rfFreqHz");
  cfg.rfFreqHz = rfFreqHz_item->valuedouble;
  cJSON *gRdB_item = cJSON_GetObjectItemCaseSensitive(root, "gRdB");
  cfg.gRdB = gRdB_item->valueint;
  cJSON *LNAstate_item = cJSON_GetObjectItemCaseSensitive(root, "LNAstate");
  cfg.LNAstate = LNAstate_item->valueint;
  cJSON *maxGain_item = cJSON_GetObjectItemCaseSensitive(root, "maxGain");
  cfg.maxGain = maxGain_item->valueint;
  cJSON *minGain_item = cJSON_GetObjectItemCaseSensitive(root, "minGain");
  cfg.minGain = minGain_item->valueint;
  cJSON *bandwidth_item = cJSON_GetObjectItemCaseSensitive(root, "bandwidth");

  // TODO: Change this to handle undefined as else case
  if (strcmp(bandwidth_item->valuestring, "sdrplay_api_BW_Undefined") == 0) {
    cfg.bandwidth = sdrplay_api_BW_Undefined;
  } else if (strcmp(bandwidth_item->valuestring, "sdrplay_api_BW_0_200") == 0) {
    cfg.bandwidth = sdrplay_api_BW_0_200;
  } else if (strcmp(bandwidth_item->valuestring, "sdrplay_api_BW_0_300") == 0) {
    cfg.bandwidth = sdrplay_api_BW_0_300;
  } else if (strcmp(bandwidth_item->valuestring, "sdrplay_api_BW_0_600") == 0) {
    cfg.bandwidth = sdrplay_api_BW_0_600;
  } else if (strcmp(bandwidth_item->valuestring, "sdrplay_api_BW_1_536") == 0) {
    cfg.bandwidth = sdrplay_api_BW_1_536;
  } else if (strcmp(bandwidth_item->valuestring, "sdrplay_api_BW_5_000") == 0) {
    cfg.bandwidth = sdrplay_api_BW_5_000;
  } else if (strcmp(bandwidth_item->valuestring, "sdrplay_api_BW_6_000") == 0) {
    cfg.bandwidth = sdrplay_api_BW_6_000;
  } else if (strcmp(bandwidth_item->valuestring, "sdrplay_api_BW_7_000") == 0) {
    cfg.bandwidth = sdrplay_api_BW_7_000;
  } else {
    cfg.bandwidth = sdrplay_api_BW_8_000;
  }

  // TODO: Change this to handle undefined as else case
  cJSON *IF_item = cJSON_GetObjectItemCaseSensitive(root, "IF");
  if (strcmp(IF_item->valuestring, "sdrplay_api_IF_Undefined") == 0) {
    cfg.IF = sdrplay_api_IF_Undefined;
  } else if (strcmp(IF_item->valuestring, "sdrplay_api_IF_Zero") == 0) {
    cfg.IF = sdrplay_api_IF_Zero;
  } else if (strcmp(IF_item->valuestring, "sdrplay_api_IF_0_450") == 0) {
    cfg.IF = sdrplay_api_IF_0_450;
  } else if (strcmp(IF_item->valuestring, "sdrplay_api_IF_1_620") == 0) {
    cfg.IF = sdrplay_api_IF_1_620;
  } else {
    cfg.IF = sdrplay_api_IF_2_048;
  }

  // TODO: Change this to handle undefined as else case
  cJSON *AGC_item = cJSON_GetObjectItemCaseSensitive(root, "AGC");
  if (strcmp(AGC_item->valuestring, "sdrplay_api_AGC_DISABLE") == 0) {
    cfg.AGC = sdrplay_api_AGC_DISABLE;
  } else if (strcmp(AGC_item->valuestring, "sdrplay_api_AGC_100HZ") == 0) {
    cfg.AGC = sdrplay_api_AGC_100HZ;
  } else if (strcmp(AGC_item->valuestring, "sdrplay_api_AGC_50HZ") == 0) {
    cfg.AGC = sdrplay_api_AGC_50HZ;
  } else if (strcmp(AGC_item->valuestring, "sdrplay_api_AGC_5HZ") == 0) {
    cfg.AGC = sdrplay_api_AGC_5HZ;
  } else {
    cfg.AGC = sdrplay_api_AGC_CTRL_EN;
  }

  cfg.minGain = minGain_item->valueint;

  cJSON_Delete(root);
  free(file_buffer);

  return cfg;
}

void print_config(struct Config cfg) {
  char *tuner_str;
  char *ms_str;
  char *BW_str;
  char *IF_str;
  char *AGC_str;

  switch (cfg.tuner) {
  case sdrplay_api_Tuner_A:
    tuner_str = "sdrplay_api_Tuner_A";
    break;
  case sdrplay_api_Tuner_B:
    tuner_str = "sdrplay_api_Tuner_B";
    break;
  case sdrplay_api_Tuner_Both:
    tuner_str = "sdrplay_api_Tuner_Both";
    break;
  case sdrplay_api_Tuner_Neither:
    tuner_str = "sdrplay_api_Tuner_Neither";
  }

  switch (cfg.master_slave) {
  case Master:
    ms_str = "Master";
    break;
  case Slave:
    ms_str = "Slave";
  }

  switch (cfg.bandwidth) {
  case sdrplay_api_BW_0_200:
    BW_str = "sdrplay_BW_0_200";
    break;
  case sdrplay_api_BW_0_300:
    BW_str = "sdrplay_BW_0_300";
    break;
  case sdrplay_api_BW_0_600:
    BW_str = "sdrplay_BW_0_600";
    break;
  case sdrplay_api_BW_1_536:
    BW_str = "sdrplay_BW_1_536";
    break;
  case sdrplay_api_BW_5_000:
    BW_str = "sdrplay_BW_5_000";
    break;
  case sdrplay_api_BW_6_000:
    BW_str = "sdrplay_BW_6_000";
    break;
  case sdrplay_api_BW_7_000:
    BW_str = "sdrplay_BW_7_000";
    break;
  case sdrplay_api_BW_8_000:
    BW_str = "sdrplay_BW_8_000";
    break;
  case sdrplay_api_BW_Undefined:
    BW_str = "sdrplay_BW_Undefined";
    break;
  }

  switch (cfg.IF) {
  case sdrplay_api_IF_Zero:
    IF_str = "sdrplay_api_IF_Zero";
    break;
  case sdrplay_api_IF_0_450:
    IF_str = "sdrplay_api_IF_0_450";
    break;
  case sdrplay_api_IF_1_620:
    IF_str = "sdrplay_api_IF_1_620";
    break;
  case sdrplay_api_IF_2_048:
    IF_str = "sdrplay_api_IF_2_048";
    break;
  case sdrplay_api_IF_Undefined:
    IF_str = "sdrplay_api_IF_Undefined";
    break;
  }

  switch (cfg.AGC) {
  case sdrplay_api_AGC_DISABLE:
    AGC_str = "sdrplay_api_AGC_DISABLE";
    break;
  case sdrplay_api_AGC_100HZ:
    AGC_str = "sdrplay_api_AGC_100Hz";
    break;
  case sdrplay_api_AGC_50HZ:
    AGC_str = "sdrplay_api_AGC_50HZ";
    break;
  case sdrplay_api_AGC_5HZ:
    AGC_str = "sdrplay_api_AGC_5HZ";
    break;
  case sdrplay_api_AGC_CTRL_EN:
    AGC_str = "sdrplay_api_AGC_CTRL_EN";
    break;
  }

  printf("Config {\n  tuner: %s,\n   master_slave: %s,\n   rspDuoSampleFreq: "
         "%f,\n   fsFreqHz: %f,\n   "
         "rfFreqHz: %f,\n   gRdB: %d,\n   LNAstate: %d,\n   "
         "maxGain: %d,\n   minGain: %d,\n   bandwidth: %s,\n   IF: %s,\n   "
         "AGC: %s,\n }\n",
         tuner_str, ms_str, cfg.rspDuoSampleFreq, cfg.fsFreqHz, cfg.rfFreqHz,
         cfg.gRdB, cfg.LNAstate, cfg.maxGain, cfg.minGain, BW_str, IF_str,
         AGC_str);
}

double result_bufferA[2000];

void StreamACallback(short *xi, short *xq, sdrplay_api_StreamCbParamsT *params,
                     unsigned int numSamples, unsigned int reset,
                     void *cbContext) {
  if (reset) {
    printf("sdrplay_api_StreamACallback: numSamples=%d\n", numSamples);
  }
  // printf("A\n");

  dft_real(xi, xq, result_bufferA, numSamples);
  for (int i = 0; i < numSamples; i++) {
    printf("%d: %f\n", i, result_bufferA[i]);
  }

  return;
}

void StreamBCallback(short *xi, short *xq, sdrplay_api_StreamCbParamsT *params,
                     unsigned int numSamples, unsigned int reset,
                     void *cbContext) {
  if (reset) {
    printf("sdrplay_api_StreamBCallback: numSamples=%d\n", numSamples);
  }
  printf("B\n");

  return;
}

void usage(void) {
  printf("Usage: <compilename>.out <config path>\n");
  exit(1);
}
