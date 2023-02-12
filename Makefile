OPTIMIZATIONS=-O3
DEBUG=-ggdb3
UNUSED=-Wno-unused-function
# -Wimplicit-float-conversion -Wimplicit-int-conversion disabled because of stb_truetype
FLAGS = $(DEBUG) $(OPTIMIZATIONS) -Wall -Wno-writable-strings -Wno-missing-braces $(UNUSED)
SDL2_LIBS = -lrt `pkg-config --libs sdl2`

build: clean
	mkdir -p build/
	clang++ -fPIC $(FLAGS) -shared src/play.cpp -o build/play.so $(SDL2_LIBS)
	clang++ $(FLAGS) $(DEBUG) -o build/play src/main.cpp -Isrc/ $(SDL2_LIBS)

clean:
	rm -f build/*
