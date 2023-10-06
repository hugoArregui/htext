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

typedef struct GlyphRenderer {
  SDL_Renderer *renderer;
  SDL_Surface *surface;
  SDL_Texture *texture;
} GlyphRenderer;

void createGlyphRenderer(GlyphRenderer *glyphRenderer, SDL_Renderer *renderer,
                         TTF_Font *font, unsigned short ch, SDL_Color color) {

  SDL_Surface *surface = TTF_cpointer(TTF_RenderGlyph_Solid(font, ch, color));

  SDL_Texture *texture =
      SDL_cpointer(SDL_CreateTextureFromSurface(renderer, surface));

  glyphRenderer->renderer = renderer;
  glyphRenderer->surface = surface;
  glyphRenderer->texture = texture;
}

void renderAndDestroyGlyph(GlyphRenderer *glyph, SDL_Rect *dest) {
  SDL_ccode(SDL_RenderCopy(glyph->renderer, glyph->texture, NULL, dest));
  SDL_DestroyTexture(glyph->texture);
  SDL_FreeSurface(glyph->surface);
}

#endif
