#ifndef __H_TEXT_SDL
#define __H_TEXT_SDL

#include <SDL2/SDL_pixels.h>
#include <SDL2/SDL_render.h>
#include <SDL2/SDL_ttf.h>

#define TTF_cpointer(p) _TTF_cpointer(p, __FILE__, __LINE__)
#define TTF_ccode(c) _TTF_ccode(c, __FILE__, __LINE__)

#define SDL_cpointer(p) _SDL_cpointer(p, __FILE__, __LINE__)
#define SDL_ccode(c) _SDL_ccode(c, __FILE__, __LINE__)

// handling errors
void _TTF_ccode(int code, char*file, int linenum);
void *_TTF_cpointer(void *p, char*file, int linenum);
void _SDL_ccode(int code, char*file, int linenum);
void *_SDL_cpointer(void *p, char*file, int linenum);

#endif
