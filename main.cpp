#include "parp.h"
#include "lib/portaudio/src/common/pa_util.h"
#include <string.h>


//PULSE_SINK="combined" ./parp -p -f <file_name> 2>/dev/null for virtual mic output
//or
//./parp -r -f <file_name>

int main(int argc, char *argv[]) {
  int opt;
  int selected_device;
  bool record = false;
  bool play = false;
  bool list_devices = false;
  bool provided_file = false;
  bool spec_device_flag = false;
  char *file_name = (char *)calloc(MAX_FILE_NAME, sizeof(char));
  //parp -r <file_name>
  //parp -p <file_name>
  while ((opt = getopt(argc, argv, ":f:rphld:")) != -1) {
    switch (opt) {
    case 'h':
      printf("usage: parp [-prlh][-d <device_number>][-f <file_name>]\n"
             "-p\tplay file\n"
             "-r\trecord to file(default file_name is a.raw)\n"
             "-l\tdisplay list of devices\n"
             "-h\tprint usage info\n"
             "-d\tspecify device for both input and output\n"
             "-f\tflag to provide name of file to "
             "record to/play from\n");

      exit(0);
    case 'f':
      if (!provided_file) {
        strncpy(file_name, optarg, MAX_FILE_NAME);
        provided_file = true;
      } else {
        printf("Multiple files?????\n");
        exit(1);
      }
      break;
    case 'r':
      record = true;
      break;
    case 'p':
      play = true;
      break;
    case 'l':
      list_devices = true;
      break;
    case 'd':
      if (!spec_device_flag){
        selected_device = atoi(optarg);
        spec_device_flag = true;
      } else {
        printf("TODO: implement input and output select\n");
        exit(1);
      }
      if(selected_device < 0){
        printf("ERROR: negative device or invalid device\n");
      }
      break;
    case ':':
      printf("please provide a file (.raw) or a device number\n");
      exit(1);
    case '?':
      printf("unkown option: %c\n", optopt);
      exit(1);
    }
  }
  if (!provided_file) {
    strncpy(file_name, "a.raw", MAX_FILE_NAME);
  }
  if (!provided_file && !record && play) {
    printf("empty file can't play\n");
    exit(1);
  }
  if (provided_file && play) {
    if (FILE *file = fopen(file_name, "r")) {
      fclose(file);
    } else {
      printf("ERROR file not found\n");
      exit(1);
    }
  }
  PaStreamParameters inputParameters;
  PaStreamParameters outputParameters;
  PaError err;
  err = Pa_Initialize();
  checkErr(err);
  paTestData data = {0};
  memcpy(data.file_name, file_name, MAX_FILE_NAME);
  unsigned numSamples;
  unsigned numBytes;
  if (list_devices)
    printDevices();
  numSamples = NextPowerOf2((unsigned)(SAMPLE_RATE * 0.5 * NUM_CHANNELS));
  numBytes = numSamples * sizeof(SAMPLE);
  data.ringBufferData = (SAMPLE *)PaUtil_AllocateMemory(numBytes);
  if (data.ringBufferData == NULL) {
    printf("Could not allocate ring buffer data.\n");
    exit(1);
  }
  err = PaUtil_InitializeRingBuffer(&data.ringBuffer, sizeof(SAMPLE),
                                    numSamples, data.ringBufferData);
  checkErr(err);

  // record some audio
  memset(&inputParameters, 0, sizeof(inputParameters));
  inputParameters.channelCount = 2;
  if(spec_device_flag){
    inputParameters.device = selected_device;
  } else {
    inputParameters.device = Pa_GetDefaultInputDevice();
  }
  inputParameters.hostApiSpecificStreamInfo = NULL;
  inputParameters.sampleFormat = PA_SAMPLE_TYPE;
  inputParameters.suggestedLatency =
      Pa_GetDeviceInfo(inputParameters.device)->defaultLowInputLatency;
  if (record) {
    RecordSound(inputParameters, &data, err);
  }
  // playback
  memset(&outputParameters, 0, sizeof(outputParameters));
  outputParameters.channelCount = 2;
  if(spec_device_flag){
    outputParameters.device = selected_device;
  } else {
    outputParameters.device = Pa_GetDefaultOutputDevice();
  }  
  outputParameters.hostApiSpecificStreamInfo = NULL;
  outputParameters.sampleFormat = PA_SAMPLE_TYPE;
  outputParameters.suggestedLatency =
      Pa_GetDeviceInfo(outputParameters.device)->defaultLowOutputLatency;
  if (play) {
    PlaySound(outputParameters, &data, err);
  }
  err = Pa_Terminate();
  checkErr(err);
  free(file_name);
  if (data.ringBufferData)
    PaUtil_FreeMemory(data.ringBufferData);
  printf("\n");
  return EXIT_SUCCESS;
}
