#ifndef __PLAY_SDL
#define __PLAY_SDL

#include <SDL2/SDL_pixels.h>
#include <SDL2/SDL_render.h>
#include <SDL2/SDL_ttf.h>

// handling errors
void TTF_ccode(int code);
void *TTF_cpointer(void *p);
void SDL_ccode(int code);
void *SDL_cpointer(void *p);

// surface renderer
typedef struct SurfaceRenderer {
  SDL_Renderer *renderer;
  SDL_Surface *surface;
  SDL_Texture *texture;
} SurfaceRenderer;

SurfaceRenderer SR_create(SDL_Renderer *renderer, SDL_Surface *surface);
void SR_renderAndDestroy(SurfaceRenderer *surfaceRenderer, SDL_Rect *dest);
void SR_renderFullSizeAndDestroy(SurfaceRenderer *sr, int x, int y);

#endif
