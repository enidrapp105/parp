# parp
## Port Audio Record and Play.
This was made using https://github.com/PortAudio/portaudio
and basically reimplemented paex_record.c but for POSIX.
I also referenced @chrisrouck on Youtube for the visualizer 
so go check him out at https://www.youtube.com/@chrisrouck

This currently only plays/records 32-bit encoded raw audio with a sample rate of 44100Hz

## To run:
clone the repository install dependencies:

    sudo apt-get install curl cmake clang pulseaudio libasound-dev libjack-dev
    make install-deps

Then run:

    make
    
and

    ./parp -h
    
for usage information

## Additionally: 
Because this is going to be developed into a soundboard
Ive included some scripts to set up a virtual mic so the audio
goes through the virtual mic

    ./setupdevice

aswell as an unloading script

    ./unloaddevices
