# parp
## Port Audio Record and Play.
This was made using [portaudio](https://github.com/PortAudio/portaudio)
and basically reimplemented paex_record.c but for POSIX.
I also referenced [@chrisrouck](https://www.youtube.com/@chrisrouck)
on Youtube for the visualizer so go check him out.
You can also check out [parppui](https://github.com/enidrapp105/parpui) 
for the frontend im working on that uses this

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

    ./loaddevices

aswell as an unloading script

    ./unloaddevices
