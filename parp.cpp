#ifdef USE_CMAKE 
  #include "pa_ringbuffer.h"
#else
  #include "lib/portaudio/src/common/pa_ringbuffer.h"
#endif // DEBUG
#include <portaudio.h>
#include <pthread.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <cstring>

#include "parp.h"

#define MAX_FILE_NAME 2048
#define SAMPLE_RATE 44100
#define FRAMES_PER_BUFFER 512
#define NUM_WRITES_PER_BUFFER (4)
#define NUM_SECONDS (10)
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



void checkErr(PaError err) {
  if (err != paNoError) {
    printf("PortAudio error: %s\n", Pa_GetErrorText(err));
    exit(EXIT_FAILURE);
  }
}

unsigned NextPowerOf2(unsigned val) {
  val--;
  val = (val >> 1) | val;
  val = (val >> 2) | val;
  val = (val >> 4) | val;
  val = (val >> 8) | val;
  val = (val >> 16) | val;
  return ++val;
}


static void *threadFunctionWriteToRawFile(void *ptr) {
  paTestData *pData = (paTestData *)ptr;

  /* Mark thread started */
  pData->threadSyncFlag = 0;

  while (1) {
    ring_buffer_size_t elementsInBuffer =
        PaUtil_GetRingBufferReadAvailable(&pData->ringBuffer);
    if ((elementsInBuffer >=
         pData->ringBuffer.bufferSize / NUM_WRITES_PER_BUFFER) ||
        pData->threadSyncFlag) {
      void *ptr[2] = {0};
      ring_buffer_size_t sizes[2] = {0};
      /* By using PaUtil_GetRingBufferReadRegions,
       * we can read directly from the
       * ring buffer */
      ring_buffer_size_t elementsRead = PaUtil_GetRingBufferReadRegions(
          &pData->ringBuffer, elementsInBuffer, ptr + 0, sizes + 0, ptr + 1,
          sizes + 1);
      if (elementsRead > 0) {
        int i;
        for (i = 0; i < 2 && ptr[i] != NULL; ++i) {
          fwrite(ptr[i], pData->ringBuffer.elementSizeBytes, sizes[i],
                 pData->file);
        }
        PaUtil_AdvanceRingBufferReadIndex(&pData->ringBuffer, elementsRead);
      }

      if (pData->threadSyncFlag) {
        break;
      }
    }

    /* Sleep a little while... */
    Pa_Sleep(20);
  }

  pData->threadSyncFlag = 0;

  return 0;
}

static void *threadFunctionReadFromRawFile(void *ptr) {
  paTestData *pData = (paTestData *)ptr;

  while (1) {
    ring_buffer_size_t elementsInBuffer =
        PaUtil_GetRingBufferWriteAvailable(&pData->ringBuffer);
    if (elementsInBuffer >=
        pData->ringBuffer.bufferSize / NUM_WRITES_PER_BUFFER) {
      void *ptr[2] = {0};
      ring_buffer_size_t sizes[2] = {0};

      /* By using PaUtil_GetRingBufferWriteRegions,
      we can write directly into the ring buffer */
      PaUtil_GetRingBufferWriteRegions(&pData->ringBuffer, elementsInBuffer,
                                       ptr + 0, sizes + 0, ptr + 1, sizes + 1);

      if (!feof(pData->file)) {
        ring_buffer_size_t itemsReadFromFile = 0;
        int i;
        for (i = 0; i < 2 && ptr[i] != NULL; ++i) {
          itemsReadFromFile += (ring_buffer_size_t)fread(
              ptr[i], pData->ringBuffer.elementSizeBytes, sizes[i],
              pData->file);
        }
        PaUtil_AdvanceRingBufferWriteIndex(&pData->ringBuffer,
                                           itemsReadFromFile);

        /* Mark thread started here,
         * that way we "prime" the ring buffer before playback */
        pData->threadSyncFlag = 0;
      } else {
        /* No more data to read */
        pData->threadSyncFlag = 1;
        break;
      }
    }

    /* Sleep a little while... */
    Pa_Sleep(20);
  }

  return 0;
}

typedef void *(*ThreadFunctionType)(void *);

static PaError startThread(paTestData *pData, ThreadFunctionType fn) {
  pthread_t thread;
  int err = pthread_create(&thread, NULL, fn, pData);
  checkErr(err);
  pData->threadHandle = (void *)thread;
  pData->threadSyncFlag = 1;
  return paNoError;
}

static int stopThread(paTestData *pData) {
  pData->threadSyncFlag = 1;

  while (pData->threadSyncFlag) {
    Pa_Sleep(10);
  }
  pthread_join((pthread_t)(pData->threadHandle), NULL);
  pData->threadHandle = 0;
  return paNoError;
}

static ring_buffer_size_t rbs_min(ring_buffer_size_t a, ring_buffer_size_t b) {
  return (a < b) ? a : b;
}

static inline float max(float a, float b) { return a > b ? a : b; }


static int recordCallback(const void *inputBuffer,
                          void *outputBuffer,
                          unsigned long framesPerBuffer,
                          const PaStreamCallbackTimeInfo *timeInfo,
                          PaStreamCallbackFlags statusFlags,
                          void *userData
                          ){
  (void)outputBuffer;
  paTestData *data = (paTestData *)userData;

  ring_buffer_size_t elementsWriteable =
      PaUtil_GetRingBufferWriteAvailable(&data->ringBuffer);

  ring_buffer_size_t elementsToWrite = rbs_min(
      elementsWriteable, (ring_buffer_size_t)(framesPerBuffer * NUM_CHANNELS));

  const SAMPLE *rptr = (const SAMPLE *)inputBuffer;
  float *in = (float *)inputBuffer;
  int dispSize = 50;
  printf("\r");
  float vol_l = 0;
  float vol_r = 0;
  for (unsigned long i = 0; i < framesPerBuffer * 2; i += 2) {
    vol_l = max(vol_l, std::abs(in[i]));
    vol_r = max(vol_r, std::abs(in[i + 1]));
  }

  for (int i = 0; i < dispSize; i++) {
    float barProportion = i / (float)dispSize;
    if (barProportion <= vol_l && barProportion <= vol_r) {
      printf("█");
    } else if (barProportion <= vol_l) {
      printf("▀");
    } else if (barProportion <= vol_r) {
      printf("▄");
    } else {
      printf(" ");
    }
  }
  data->frameIndex +=
      PaUtil_WriteRingBuffer(&data->ringBuffer, rptr, elementsToWrite);
  fflush(stdout);

  return paContinue;
}

static int playCallback(const void *inputBuffer,
                        void *outputBuffer,
                        unsigned long framesPerBuffer,
                        const PaStreamCallbackTimeInfo *timeInfo,
                        PaStreamCallbackFlags statusFlags,
                        void *userData
                        ){

  paTestData *data = (paTestData *)userData;

  ring_buffer_size_t elementsToPlay =
      PaUtil_GetRingBufferReadAvailable(&data->ringBuffer);
  ring_buffer_size_t elementsToRead = rbs_min(
      elementsToPlay, (ring_buffer_size_t)(framesPerBuffer * NUM_CHANNELS));

  SAMPLE *wptr = (SAMPLE *)outputBuffer;
  data->frameIndex +=
      PaUtil_ReadRingBuffer(&data->ringBuffer, wptr, elementsToRead);

  if (elementsToRead < (ring_buffer_size_t)(framesPerBuffer * NUM_CHANNELS)) {
    memset(wptr + elementsToRead, 0,
           (framesPerBuffer * NUM_CHANNELS - elementsToRead) * sizeof(SAMPLE));
  }

  (void)inputBuffer; /* Prevent unused variable warnings. */
  (void)timeInfo;
  (void)statusFlags;
  (void)userData;
  float *out = (float *)outputBuffer;
  int dispSize = 50;
  printf("\r");
  float vol_l = 0;
  float vol_r = 0;
  for (unsigned long i = 0; i < framesPerBuffer * 2; i += 2) {
    vol_l = max(vol_l, std::abs(out[i]));
    vol_r = max(vol_r, std::abs(out[i + 1]));
  }

  for (int i = 0; i < dispSize; i++) {
    float barProportion = i / (float)dispSize;
    if (barProportion <= vol_l && barProportion <= vol_r) {
      printf("█");
    } else if (barProportion <= vol_l) {
      printf("▀");
    } else if (barProportion <= vol_r) {
      printf("▄");
    } else {
      printf(" ");
    }
  }
  fflush(stdout);
  return data->threadSyncFlag ? paComplete : paContinue;
}

void printDevices() {
  const PaDeviceInfo *deviceInfo;
  int numDevices = Pa_GetDeviceCount();
  printf("Number of devices: %d\n", numDevices);
  if (numDevices < 0) {
    printf("Error getting device count.\n");
    exit(EXIT_FAILURE);
  } else if (numDevices == 0) {
    printf("There are no devices on this machine.\n");
  }
  for (PaDeviceIndex i = 0; i < numDevices; i++) {
    deviceInfo = Pa_GetDeviceInfo(i);
    printf("Device %d:\n"
           "\tname: %s\n"
           "\tmaxInputChannels: %d\n"
           "\tmaxOutputChannels: %d\n"
           "\tdefaultSampleRate: %f\n"
           "\tdefaultLowInputLatency: %f\n",
           i,
           deviceInfo->name,
           deviceInfo->maxInputChannels,
           deviceInfo->maxOutputChannels,
           deviceInfo->defaultSampleRate,
           deviceInfo->defaultLowInputLatency);
  }
}

PaError RecordSound(PaStreamParameters inputParameters,
                    paTestData *data,
                    PaError err
                    ){
  PaStream *stream;
  err = Pa_OpenStream(&stream,
                      &inputParameters,
                      NULL, 
                      SAMPLE_RATE,
                      FRAMES_PER_BUFFER,
                      paClipOff, 
                      recordCallback, 
                      data);

  unsigned delayCntr;
  checkErr(err);

  data->file = fopen(data->file_name, "wb");
  if (data->file == 0)
    exit(1);

  err = startThread(data, threadFunctionWriteToRawFile);
  checkErr(err);

 

  err = Pa_StartStream(stream);
  checkErr(err);
  delayCntr = 0;
  while (delayCntr++ < NUM_SECONDS) {
    Pa_Sleep(1000);
  }
  checkErr(err);

  err = Pa_CloseStream(stream);
  checkErr(err);

  err = stopThread(data);
  checkErr(err);

  fclose(data->file);
  data->file = 0;
  return err;
}

void convert(char * in_file_name, char *raw_file_name);

PaError PlaySound(PaStreamParameters outputParameters,
                  paTestData *data,
                  PaError err
                  ){
  PaStream *stream;
  data->frameIndex = 0;
  err = Pa_OpenStream(&stream,
                      NULL, /* no input */
                      &outputParameters,
                      SAMPLE_RATE, 
                      FRAMES_PER_BUFFER,
                      paClipOff, 
                      playCallback, 
                      data);
  checkErr(err);
  if (stream) {
    /* Open file again for reading */
    data->file = fopen(data->file_name, "rb");
    if (data->file != 0) {
      /* Start the file reading thread */
      err = startThread(data, threadFunctionReadFromRawFile);
      checkErr(err);

      while(data->threadSyncFlag) {
        Pa_Sleep(10);
      }
      Pa_Sleep(50);

      err = Pa_StartStream(stream);
      checkErr(err);

      /* The playback will end when EOF is reached */
      while ((err = Pa_IsStreamActive(stream)) == 1) {
        Pa_Sleep(1000);
      }
      checkErr(err);
    }

    err = Pa_CloseStream(stream);
    checkErr(err);
    fclose(data->file);
  }
  return err;
}


