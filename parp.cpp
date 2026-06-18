//********************************************************
// Author: Enid Rapp
// Purpose: The Definitions for the functions in parp.h
//

#include <cstdint>
#include <libavutil/mathematics.h>
#ifdef USE_CMAKE 
  #include "pa_ringbuffer.h"
#else
  #include "lib/portaudio/src/common/pa_ringbuffer.h"
#endif // DEBUG
#include <portaudio.h>
#include <pthread.h>

extern "C"{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
#include <libavutil/opt.h>
#include <libavutil/samplefmt.h>
#include <libavutil/channel_layout.h>
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <cstring>

#include "parp.h"

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
    if(pData->stopRequested) break;
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
  if(data->visualizer){
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
  }
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
  if(data->stopRequested) return paComplete;

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
  if(data->visualizer){
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
  }
  if(data->threadSyncFlag &&
    PaUtil_GetRingBufferReadAvailable(&data->ringBuffer) == 0) {
    return paComplete;
  }
  return paContinue;
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

char *convert_mp3_to_raw(char *in_file_name, char *raw_file_name, size_t out_size){
  AVFormatContext *fmt_ctx = NULL;
  AVCodecContext *codec_ctx = NULL;
  SwrContext * swr = NULL;
  AVPacket *pkt= av_packet_alloc();
  AVFrame *frame = av_frame_alloc();
  FILE *outfile = NULL;
  int audio_idx = -1;
  // OPEN INPUT
  do{
  if (avformat_open_input(&fmt_ctx, in_file_name, NULL, NULL) < 0) {
    fprintf(stderr, "Could not open input file\n");
    break;
  }
  avformat_find_stream_info(fmt_ctx, NULL);
  
  // FIND AUDIO STREAM
  for (unsigned int i = 0; i < fmt_ctx->nb_streams; i++) {
    if (fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
      audio_idx = i;
      break;
    }
  }
  if (audio_idx < 0) {
    fprintf(stderr, "No audio stream found\n");
    break;
  }
  
  // SET UP DECODER
  AVCodecParameters *codecpar = fmt_ctx->streams[audio_idx]->codecpar;
  const AVCodec *codec = avcodec_find_decoder(codecpar->codec_id);
  codec_ctx = avcodec_alloc_context3(codec);
  avcodec_parameters_to_context(codec_ctx, codecpar);
  avcodec_open2(codec_ctx, codec, NULL);


  // SET UP RESAMPLER
  const int out_sample_rate = 44100;
  const enum AVSampleFormat out_fmt = AV_SAMPLE_FMT_FLT;
  AVChannelLayout stereo = AV_CHANNEL_LAYOUT_STEREO;

  swr = swr_alloc();
  av_opt_set_chlayout  (swr, "in_chlayout",   &codec_ctx->ch_layout,  0);
  av_opt_set_chlayout  (swr, "out_chlayout",  &stereo,  0);
  av_opt_set_int       (swr, "in_sample_rate",  codec_ctx->sample_rate, 0);
  av_opt_set_int       (swr, "out_sample_rate", out_sample_rate, 0);
  av_opt_set_sample_fmt(swr, "in_sample_fmt",   codec_ctx->sample_fmt,  0);
  av_opt_set_sample_fmt(swr, "out_sample_fmt",  out_fmt,      0);
  swr_init(swr);

  // OPEN OUTPUT FILE
  size_t base_len;
  const char *dot = strrchr(in_file_name, '.');
  if(dot){
    base_len = (size_t)(dot - in_file_name);
  }else {
    base_len = strlen(in_file_name);
  }
  if (base_len + 5 > out_size) {   // 5 = strlen(".raw") + null terminator
    fprintf(stderr, "Output filename buffer too small (need %zu, have %zu)\n",
            base_len + 5, out_size);
    break;  
  }

  memcpy(raw_file_name, in_file_name, base_len);
  memcpy(raw_file_name + base_len, ".raw", 5);

  outfile = fopen(raw_file_name, "wb");
  if (!outfile) {
    fprintf(stderr, "Could not open output file\n");
    break;
  }
  // MAIN DECODE LOOP
  while (av_read_frame(fmt_ctx, pkt) >= 0) {
    if (pkt->stream_index != audio_idx) {
      av_packet_unref(pkt);
      continue;
    }

    avcodec_send_packet(codec_ctx, pkt);
    av_packet_unref(pkt);

    while (avcodec_receive_frame(codec_ctx, frame) == 0) {
      // Convert from planar float to interleaved S16
      uint8_t *out_buf = NULL;
      int64_t delay = swr_get_delay(swr, codec_ctx->sample_rate);
      int in_count = delay + frame->nb_samples;
      int out_samples = (int)((in_count * out_sample_rate + codec_ctx->sample_rate -1)
                              / codec_ctx->sample_rate);
      int out_linesize;
      av_samples_alloc(&out_buf,
                      &out_linesize,
                      stereo.nb_channels,
                      out_samples,
                      out_fmt,
                      0
                      );

      out_samples = swr_convert(swr,
                                &out_buf,
                                out_samples,
                                (const uint8_t **)frame->data,
                                frame->nb_samples
                                );

      int bytes_per_sample = av_get_bytes_per_sample(out_fmt);
      fwrite(out_buf, 
            bytes_per_sample * stereo.nb_channels,
            out_samples,
            outfile
            );
      av_freep(&out_buf);
      av_frame_unref(frame);
    }
  }

  avcodec_send_packet(codec_ctx, NULL);
  while (avcodec_receive_frame(codec_ctx, frame) == 0) {
    uint8_t *out_buf = NULL;
    int64_t delay = swr_get_delay(swr, codec_ctx->sample_rate);
    int in_count = delay + frame->nb_samples;
    int out_samples = (int)((in_count * out_sample_rate + codec_ctx->sample_rate -1)
                              / codec_ctx->sample_rate);
    int out_linesize = 0;
      (void)out_linesize;
    av_samples_alloc(&out_buf, &out_linesize,
                    stereo.nb_channels,
                    out_samples, out_fmt, 0
                    );
    out_samples = swr_convert(swr, 
                            &out_buf, 
                            out_samples,
                            (const uint8_t **)frame->data,
                            frame->nb_samples
                            );
    int bytes_per_sample = av_get_bytes_per_sample(out_fmt);
    fwrite(out_buf, 
          bytes_per_sample * stereo.nb_channels,
          out_samples, 
          outfile
          );
    av_freep(&out_buf);
    av_frame_unref(frame);
  }
  {
    int64_t delay = swr_get_delay(swr, codec_ctx->sample_rate);
    if (delay > 0) {
      uint8_t *out_buf = NULL;
      int out_linesize = 0;
      (void)out_linesize;
      int in_count = delay;
      int out_samples = (int)((in_count * out_sample_rate + codec_ctx->sample_rate -1)
                              / codec_ctx->sample_rate);
      av_samples_alloc(&out_buf,
                       &out_linesize,
                       stereo.nb_channels,
                       out_samples,
                       out_fmt,
                       0);
      out_samples = swr_convert(swr,
                                &out_buf,
                                out_samples,
                                NULL,
                                0);
      if (out_samples > 0) {
        int bytes_per_sample = av_get_bytes_per_sample(out_fmt);
        fwrite(out_buf,
               bytes_per_sample * stereo.nb_channels,
               out_samples,
               outfile);
      }
      av_freep(&out_buf);
    }
  }


  }while(0);
  //cleanup

  if (outfile)   fclose(outfile);
  swr_free(&swr);
  avcodec_free_context(&codec_ctx);
  avformat_close_input(&fmt_ctx);
  av_packet_free(&pkt);
  av_frame_free(&frame);
  return raw_file_name;
}

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
      while (true) {
        err = Pa_IsStreamActive(stream);
        if (err < 0) break;
        if (err == 0) break;
        Pa_Sleep(100);
      }
      checkErr(err);

      pthread_join((pthread_t)(data->threadHandle), NULL);
      data->threadHandle = 0;
    }

    err = Pa_CloseStream(stream);
    checkErr(err);
    fclose(data->file);
  }
  return err;
}


