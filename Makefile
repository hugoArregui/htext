OPTIMIZATIONS=-O3
DEBUG=-ggdb3
CC=clang

CFLAGS=-Wall -Wextra -std=c17 -pedantic -ggdb -Wno-gnu-empty-initializer `pkg-config --cflags sdl2`
LIBS=`pkg-config --libs sdl2 SDL2_ttf SDL2_image`

build: clean
	mkdir -p build/
	$(CC) -fPIC $(CFLAGS) -shared src/play_app.c src/play_sdl.c -o build/play.so $(LIBS)
	$(CC) $(CFLAGS) $(DEBUG) -o build/play src/play_platform.c src/play_sdl.c $(LIBS)

clean:
	rm -f build/*
