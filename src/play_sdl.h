#ifndef __PLAY_SDL
#define __PLAY_SDL

#include <SDL2/SDL_pixels.h>
#include <SDL2/SDL_render.h>
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

void renderText(SDL_Renderer *renderer, TTF_Font *font, char *text,
                SDL_Color color, int x, int y) {

  SDL_Surface *text_surface =
      TTF_cpointer(TTF_RenderText_Solid(font, text, color));

  SDL_Texture *text_texture =
      SDL_cpointer(SDL_CreateTextureFromSurface(renderer, text_surface));

  SDL_Rect dest;
  dest.x = x;
  dest.y = y;
  dest.w = text_surface->w;
  dest.h = text_surface->h;
  SDL_ccode(SDL_RenderCopy(renderer, text_texture, NULL, &dest));

  SDL_DestroyTexture(text_texture);
  SDL_FreeSurface(text_surface);
}

#endif
