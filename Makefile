EXEC = parp
CFLAGS = -g -Wall -Wno-unused-function -I./lib/portaudio/include $(shell pkg-config --cflags libavformat libavcodec libavutil libswresample)
LIBS = ./lib/portaudio/lib/.libs/libportaudio.a -lrt -lasound -ljack -pthread -lavformat -lavcodec -lavutil -lswresample -lavutil 
$(EXEC): ./lib/portaudio/src/common/pa_ringbuffer.c parp.cpp main.cpp
	g++ $(CFLAGS) -o $@ $^ $(LIBS)

install-deps:
	mkdir -p lib && curl -L http://files.portaudio.com/archives/pa_stable_v190700_20210406.tgz | tar -xz -C lib
	cd lib/portaudio && ./configure && $(MAKE) -j
.PHONY: install-deps
uninstall-deps:
	cd lib/portaudio && $(MAKE) uninstall
	rm -rf lib/portaudio
.PHONY: uninstall-deps
clean:
	rm -f $(EXEC)
.PHONY: clean
