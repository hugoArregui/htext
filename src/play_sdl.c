#include "play_sdl.h"
#include <SDL2/SDL_rect.h>
#include <SDL2/SDL_surface.h>

void TTF_ccode(int code) {
  if (code < 0) {
    printf("Error : %s\n", TTF_GetError());
    SDL_Quit();
    exit(1);
  }
}

void *TTF_cpointer(void *p) {
  if (!p) {
    printf("Error : %s\n", TTF_GetError());
    SDL_Quit();
    exit(1);
  }
  return p;
}

void SDL_ccode(int code) {
  if (code < 0) {
    printf("Error : %s\n", SDL_GetError());
    SDL_Quit();
    exit(1);
  }
}

void *SDL_cpointer(void *p) {
  if (!p) {
    printf("Error : %s\n", SDL_GetError());
    SDL_Quit();
    exit(1);
  }
  return p;
}

SurfaceRenderer SR_create(SDL_Renderer *renderer, SDL_Surface *surface) {
  SDL_Texture *texture =
      SDL_cpointer(SDL_CreateTextureFromSurface(renderer, surface));

  return (SurfaceRenderer){
      .renderer = renderer, .surface = surface, .texture = texture};
}

void SR_render_fullsize_and_destroy(SurfaceRenderer *sr, int x, int y) {
  SDL_Rect dest;
  dest.x = x;
  dest.y = y;
  dest.w = sr->surface->w;
  dest.h = sr->surface->h;
  SR_render_and_destroy(sr, &dest);
}

void SR_render_and_destroy(SurfaceRenderer *sr, SDL_Rect *dest) {
  SDL_ccode(SDL_RenderCopy(sr->renderer, sr->texture, NULL, dest));
  SDL_DestroyTexture(sr->texture);
  SDL_FreeSurface(sr->surface);
}
