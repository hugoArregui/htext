#ifndef __PLAY_SDL
#define __PLAY_SDL

#include <SDL2/SDL_ttf.h>

void TTF_ccode(int code) {
  if (code < 0) {
    printf("Error : %s\n", TTF_GetError());
    exit(1);
  }
}

void *TTF_cpointer(void *p) {
  if (!p) {
    printf("Error : %s\n", TTF_GetError());
    exit(1);
  }
  return p;
}

void SDL_ccode(int code) {
  if (code < 0) {
    printf("Error : %s\n", SDL_GetError());
    exit(1);
  }
}

void *SDL_cpointer(void *p) {
  if (!p) {
    printf("Error : %s\n", SDL_GetError());
    exit(1);
  }
  return p;
}

#endif
