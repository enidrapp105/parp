# parp
## Port Audio Record and Play.
You can also check out [parpui](https://github.com/enidrapp105/parpui) 
for the frontend im working on that uses this

This currently records 32-bit encoded headerless raw audio with a sample rate of 44100Hz,
and plays mp3 files aswell as 32-bit encoded headerless raw audio files

This currently only works on ubuntu/debian systems

## To run:
clone the repository and install dependencies:

    sudo apt-get install curl cmake clang pulseaudio libasound-dev libjack-dev ffmpeg libavformat-dev libavcodec-dev libavutil-dev libswresample-dev

    make install-deps 
Then run:

    make
    
and for usage information:

    ./parp -h
    

## Virtual Mic Setup: 
If you want to use this as a soundboard,
I've included some scripts to set up a virtual mic so the audio
goes through the virtual mic

    ./loaddevices

as well as an unloading script

    ./unloaddevices
The virtual mic also need to be set up as an environment variable to be used:

    PULSE_SINK="combined" ./parp -h
## Acknowledgements:
This was made using [portaudio](https://github.com/PortAudio/portaudio)
and basically reimplemented paex_record.c but for POSIX.
I also referenced [@chrisrouck](https://www.youtube.com/@chrisrouck)
on Youtube for the visualizer so go check him out.
