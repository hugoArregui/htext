OPTIMIZATIONS=-O3
DEBUG=-ggdb3
CC=clang

# -D_DEFAULT_SOURCE so ubuntu allows MAP_ANONYMOUS (this doesn't happen in arch)
# see https://man7.org/linux/man-pages/man2/mmap.2.html
CFLAGS=-Wall -Wextra -std=c17 -pedantic -ggdb -Wno-gnu-empty-initializer `pkg-config --cflags sdl2` -D_DEFAULT_SOURCE
LIBS=`pkg-config --libs sdl2 SDL2_ttf SDL2_image`

report:
	clang --version
	uname -r

_build: clean
	mkdir -p build/
	$(CC) -DDEBUG_PLAYBACK=$(DEBUG_PLAYBACK) -fPIC $(CFLAGS) -shared src/htext_app.c -o build/htext.so $(LIBS)
	$(CC) -DDEBUG_PLAYBACK=$(DEBUG_PLAYBACK) $(CFLAGS) $(DEBUG) -o build/htext src/htext_platform.c $(LIBS)

build:
	DEBUG_PLAYBACK=0 make _build

build-for-recording:
	DEBUG_PLAYBACK=1 make _build

build-for-playback:
	DEBUG_PLAYBACK=2 make _build

test: clean
	mkdir -p build/
	$(CC) $(CFLAGS) $(DEBUG) -o build/tests src/tests.c $(LIBS)
	./build/tests

format:
	clang-format -i src/*.c src/*.h

clean:
	rm -f build/*

.DEFAULT_GOAL := build
.PHONY: build
