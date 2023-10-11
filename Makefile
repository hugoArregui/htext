OPTIMIZATIONS=-O3
DEBUG=-ggdb3
CC=clang

CFLAGS=-Wall -Wextra -std=c17 -pedantic -ggdb -Wno-gnu-empty-initializer `pkg-config --cflags sdl2`
LIBS=`pkg-config --libs sdl2 SDL2_ttf SDL2_image`

build: clean
	mkdir -p build/
	$(CC) -fPIC $(CFLAGS) -shared src/htext_app.c src/htext_sdl.c -o build/htext.so $(LIBS)
	$(CC) $(CFLAGS) $(DEBUG) -o build/htext src/htext_platform.c src/htext_sdl.c $(LIBS)

test: clean
	mkdir -p build/
	$(CC) $(CFLAGS) $(DEBUG) -o build/tests src/tests.c src/htext_sdl.c $(LIBS)
	./build/tests

clean:
	rm -f build/*
