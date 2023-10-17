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

build: clean
	mkdir -p build/
	$(CC) -fPIC $(CFLAGS) -shared src/htext_app.c -o build/htext.so $(LIBS)
	$(CC) $(CFLAGS) $(DEBUG) -o build/htext src/htext_platform.c $(LIBS)

test: clean
	mkdir -p build/
	$(CC) $(CFLAGS) $(DEBUG) -o build/tests src/tests.c src/htext_sdl.c $(LIBS)
	./build/tests

format:
	clang-format -i src/*.c src/*.h

clean:
	rm -f build/*

.DEFAULT_GOAL := build
