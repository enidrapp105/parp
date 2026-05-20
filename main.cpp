#include "parp.h"
#include "lib/portaudio/src/common/pa_util.h"
#include <getopt.h>
#include <string.h>
#include <regex.h>

//PULSE_SINK="combined" ./parp -p -f <file_name> 2>/dev/null for virtual mic output
//or
//./parp -r -f <file_name>
static void print_help_message(){
  printf("usage: parp [-lh][-d <device_number>][-r | -r <record_file_name>][-p <play_file_name>]\n"
        "-l\tdisplay list of devices\n"
        "-h\tprint usage info\n"
        "-d\tspecify device for both input and output\n"
        "-r\trecord to file(default record_file_name is a.raw)\n"
        "-p\tplay file (default file is not provided)\n"
        );
  exit(1);
}

static bool file_exists(char* file_name){
  bool exists = false;
  if (FILE *file = fopen(file_name, "r")) {
    exists = true;
    fclose(file);
  }
  return exists;
}

static int valid_file(regex_t *regex, char* file_name){
  int ret;
  ret = regexec(regex, file_name, 0, NULL, 0);
  if(!ret)
    return ret;
  else if(ret == REG_NOMATCH){}
  else{
    fprintf(stderr, "REGEX ERROR OCCURRED\n");
    exit(1);
  }
  return ret;
}

int main(int argc, char *argv[]) {
  int opt;
  int selected_device;
  bool list_devices = false;
  bool file_play = false;
  bool file_record = false;
  bool spec_device_flag = false;
  char *file_name_play = (char *)calloc(MAX_FILE_NAME, sizeof(char));
  char *file_name_record = (char *)calloc(MAX_FILE_NAME, sizeof(char));
  

  while ((opt = getopt(argc, argv, ":r:p:hld:")) != -1) {
    switch (opt) {
      case 'h':
        print_help_message();
      case 'r':
        if (!file_record) {
          strncpy(file_name_record, optarg, MAX_FILE_NAME);
          file_record = true;
        } else {
          fprintf(stderr, "Multiple files?????\n");
          exit(1);
        }

        break;
      case 'p':
        if (!file_play) {
          strncpy(file_name_play, optarg, MAX_FILE_NAME);
          file_play = true;
        } else {
          fprintf(stderr, "Multiple files?????\n");
          exit(1);
        }

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
        switch (optopt) {
          case 'r':
            printf("please provide a file (.raw)\n");
          case 'p':
            printf("please provide a file (.raw)\n");
            exit(1);
          case 'd':
            printf("please provide a device number\n");
            exit(1);
        }
        break;
      case '?':
        printf("unkown option: %c\n", optopt);
        exit(1);
    }
  }
  if(!file_exists(file_name_record) && !file_exists(file_name_play)){
    printf("ERROR file doesn't exist");
    exit(1);
  }
  if(!file_play && !file_record && !list_devices && !spec_device_flag){
    print_help_message();
  }
  regex_t mp3regex;
  regex_t rawregex;
  regcomp(&mp3regex, "^.+\\.(mp3)$", REG_EXTENDED);
  regcomp(&rawregex, "^.+\\.(raw)$", REG_EXTENDED);

  if(valid_file(&mp3regex, file_name_play) == 0){
    char raw_name[MAX_FILE_NAME];
    convert_mp3_to_raw(file_name_play, raw_name, sizeof(raw_name));
    snprintf(file_name_play, sizeof(raw_name), "%s", raw_name);
  }
  if(valid_file(&rawregex, file_name_play) == 0){}



  
 
  PaStreamParameters inputParameters;
  PaStreamParameters outputParameters;
  PaError err;
  err = Pa_Initialize();
  checkErr(err);
  paTestData data = {0};
  memcpy(data.file_name, file_name_record, MAX_FILE_NAME);
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
  if (file_record) {
    RecordSound(inputParameters, &data, err);
  }
  // playback
  memset(&outputParameters, 0, sizeof(outputParameters));
  memcpy(data.file_name, file_name_play, MAX_FILE_NAME);
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
  if (file_play) {
    PlaySound(outputParameters, &data, err);
  }
  err = Pa_Terminate();
  checkErr(err);
  free(file_name_play);
  free(file_name_record);

  if (data.ringBufferData)
    PaUtil_FreeMemory(data.ringBufferData);
  printf("\n");
  return EXIT_SUCCESS;
}
