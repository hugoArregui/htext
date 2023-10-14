#include "htext_sdl.h"
#include <SDL2/SDL_rect.h>
#include <SDL2/SDL_surface.h>

void _TTF_ccode(int code, char* file, int line) {
  if (code < 0) {
    printf("Error (%s:%d): %s\n", file, line, TTF_GetError());
    SDL_Quit();
    exit(1);
  }
}

void *_TTF_cpointer(void *p, char* file, int line) {
  if (!p) {
    printf("Error (%s:%d): %s\n", file, line, TTF_GetError());
    SDL_Quit();
    exit(1);
  }
  return p;
}

void _SDL_ccode(int code, char* file, int line) {
  if (code < 0) {
    printf("Error (%s:%d): %s\n", file, line, SDL_GetError());
    SDL_Quit();
    exit(1);
  }
}

void *_SDL_cpointer(void *p, char* file, int line) {
  if (!p) {
    printf("Error (%s:%d): %s\n", file, line, SDL_GetError());
    SDL_Quit();
    exit(1);
  }
  return p;
}
