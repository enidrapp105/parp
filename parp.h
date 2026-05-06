#pragma once

#include <portaudio.h>
#ifdef USE_CMAKE
  #include "pa_ringbuffer.h"
#else
  #include "lib/portaudio/src/common/pa_ringbuffer.h"
#endif // USE_CMAKE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <atomic>


#define MAX_FILE_NAME 2048
#define SAMPLE_RATE 44100
#define NUM_CHANNELS (2)

#if 1
#define PA_SAMPLE_TYPE paFloat32
typedef float SAMPLE;
#define SAMPLE_SILENCE (0.0f)
#define PRINTF_S_FORMAT "%.8f"
#elif 1
#define PA_SAMPLE_TYPE paInt16
typedef short SAMPLE;
#define SAMPLE_SILENCE (0)
#define PRINTF_S_FORMAT "%d"
#elif 0
#define PA_SAMPLE_TYPE paInt8
typedef char SAMPLE;
#define SAMPLE_SILENCE (0)
#define PRINTF_S_FORMAT "%d"
#else
#define PA_SAMPLE_TYPE paUInt8
typedef unsigned char SAMPLE;
#define SAMPLE_SILENCE (128)
#define PRINTF_S_FORMAT "%d"
#endif

typedef struct {
  unsigned frameIndex;
  std::atomic<int> threadSyncFlag;
  SAMPLE *ringBufferData;
  PaUtilRingBuffer ringBuffer;
  FILE *file;
  char file_name[MAX_FILE_NAME];
  void *threadHandle;
} paTestData;

void checkErr(PaError err);
void printDevices();

PaError RecordSound(PaStreamParameters inputParameters,
                    paTestData *data,
                    PaError err);


PaError PlaySound(PaStreamParameters outputParameters,
                    paTestData *data,
                    PaError err);
 
 


