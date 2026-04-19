# parp
## Port Audio Record and Play.
this was made using https://github.com/PortAudio/portaudio
and basically reimplemented paex_record.c but for POSIX.
I also referenced @chrisrouck on Youtube for the visualizer 
so go check him out at https://www.youtube.com/@chrisrouck

this currently only plays/records 32-bit encoded raw audio with a sample rate of 44100Hz

## To run:
clone the repository and run
Install dependencies:

    sudo apt-get install curl cmake clang pulseaudio libasound-dev libjack-dev
    
    
    
    make install-deps``

Then run:

    make
    
and

    ./parp -h
    
for usage information
