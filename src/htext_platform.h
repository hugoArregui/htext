#include <SDL2/SDL_render.h>
#ifndef __H_TEXT_MAIN
#define __H_TEXT_MAIN

#include <assert.h>
#include <dirent.h>
#include <fcntl.h>
#include <float.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/stat.h>
#include <unistd.h>

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;
typedef int32 bool32;

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef uintptr_t uintptr;

typedef size_t memory_index;

typedef float real32;
typedef double real64;

#include <SDL2/SDL.h>

#define ArrayCount(Array) (sizeof(Array) / sizeof((Array)[0]))

#define Kilobytes(Value) ((Value)*1024LL)
#define Megabytes(Value) (Kilobytes(Value) * 1024LL)
#define Gigabytes(Value) (Megabytes(Value) * 1024LL)
#define Terabytes(Value) (Gigabytes(Value) * 1024LL)

#define DEBUG_WINDOW 0

typedef struct {
  uint64 totalSize;
  void *gameMemoryBlock;
} PlatformState;

typedef struct {
  SDL_Renderer *renderer;
#ifdef DEBUG_WINDOW
  SDL_Renderer *debugRenderer;
#endif

  int width;
  int height;
} SdlOffscreenBuffer;

typedef struct {
  uint64 permanentStorageSize;
  void *permanentStorage; // NOTE(casey): REQUIRED to be cleared to zero at
                          // startup

  uint64 transientStorageSize;
  void *transientStorage; // NOTE(casey): REQUIRED to be cleared to zero at
                          // startup
} Memory;

typedef struct {
  real32 dtForFrame;
  bool32 executableReloaded;
  int keypressed;

  char text[32];
} Input;

#define UPDATE_AND_RENDER(name)                                                \
  int name(Memory *memory, Input *input, SdlOffscreenBuffer *buffer)
typedef UPDATE_AND_RENDER(update_and_render);

// clang-format off
#define UNHEX(color) \
  ((color) >> (8 * 3)) & 0xFF, \
  ((color) >> (8 * 2)) & 0xFF, \
  ((color) >> (8 * 1)) & 0xFF, \
  ((color) >> (8 * 0)) & 0xFF
// clang-format on

typedef struct {
  void *handler;
  // IMPORTANT(casey): Either of the callbacks can be 0!  You must
  // check before calling.
  update_and_render *updateAndRender;

  bool32 isValid;
} Code;

#endif
