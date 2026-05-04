EXEC = parp
CLIB = -I./lib/portaudio/include ./lib/portaudio/lib/.libs/libportaudio.a -lrt -lasound -ljack -pthread
$(EXEC): ./lib/portaudio/src/common/pa_ringbuffer.c parp.cpp main.cpp
	g++ -g -Wall -Wno-unused-function -lpthread -o $@ $^ $(CLIB)

install-deps:
	mkdir -p lib && curl -L http://files.portaudio.com/archives/pa_stable_v190700_20210406.tgz | tar -xz -C lib
	cd lib/portaudio && ./configure && $(MAKE) -j
.PHONY: install-deps
unistall-deps:
	cd lib/portaudio && $(MAKE) uninstall
	rm -rf lib/portaudio
.PHONY: uninstall-deps
clean:
	rm -f $(EXEC)
.PHONY: clean
