#ifndef SDR_UTILS_H
#define SDR_UTILS_H
#include "sdrplay_api.h"

struct Config load_config(const char *path);
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

void print_config(struct Config cfg);

void StreamACallback(short *xi, short *xq, sdrplay_api_StreamCbParamsT *params,
                     unsigned int numSamples, unsigned int reset,
                     void *cbContext);

void StreamBCallback(short *xi, short *xq, sdrplay_api_StreamCbParamsT *params,
                     unsigned int numSamples, unsigned int reset,
                     void *cbContext);

void usage(void);

#endif
