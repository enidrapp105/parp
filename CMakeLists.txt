cmake_minimum_required(VERSION 3.16)
project(parp)
include(FetchContent)
FetchContent_Declare(
  portaudio
  URL http://files.portaudio.com/archives/pa_stable_v190700_20210406.tgz
)
FetchContent_MakeAvailable(portaudio)

# CLIB = -I./lib/portaudio/include ./lib/portaudio/lib/.libs/libportaudio.a -lrt -lasound -ljack -pthread
# $(EXEC): ./lib/portaudio/src/common/pa_ringbuffer.c parp.cpp main.cpp
# 	g++ -g -Wall -Wno-unused-function -lpthread -o $@ $^ $(CLIB)

add_library(parp
  parp.cpp
  ${portaudio_SOURCE_DIR}/src/common/pa_ringbuffer.c
)

target_compile_definitions(parp PRIVATE USE_CMAKE)

target_include_directories(parp PUBLIC
  ./
  ${portaudio_SOURCE_DIR}/src/common/
  ${portaudio_SOURCE_DIR}/include
)

target_link_libraries(parp PRIVATE
  rt
  asound
  jack
  pthread
  portaudio
)
