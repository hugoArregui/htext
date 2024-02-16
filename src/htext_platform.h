#ifndef __H_TEXT_MAIN
#define __H_TEXT_MAIN

#include <assert.h>
#include <dirent.h>
#include <fcntl.h>
#include <float.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/stat.h>
#include <unistd.h>

typedef size_t memory_index;

typedef float real32;
typedef double real64;

#include <SDL2/SDL.h>

#define ArrayCount(Array) (sizeof(Array) / sizeof((Array)[0]))

#define Kilobytes(Value) ((Value)*1024LL)
#define Megabytes(Value) (Kilobytes(Value) * 1024LL)
#define Gigabytes(Value) (Megabytes(Value) * 1024LL)
#define Terabytes(Value) (Gigabytes(Value) * 1024LL)

#define PLAYBACK_DISABLED 0
#define PLAYBACK_RECORDING 1
#define PLAYBACK_PLAYING 2

#define DEBUG_WINDOW 0
/* #define DEBUG_PLAYBACK PLAYBACK_PLAYING */

typedef struct {
  uint64_t total_size;
  void *memory_block;
} PlatformState;

typedef struct {
  SDL_Renderer *renderer;
#ifdef DEBUG_WINDOW
  SDL_Renderer *debug_renderer;
#endif

  int width;
  int height;
} SdlOffscreenBuffer;

typedef struct {
  uint64_t permanent_storage_size;
  void *permanent_storage; // NOTE(casey): REQUIRED to be cleared to zero at
                           // startup

  uint64_t transient_storage_size;
  void *transient_storage; // NOTE(casey): REQUIRED to be cleared to zero at
                           // startup
} Memory;

typedef struct {
  bool executableReloaded;
  int keypressed;

  char text[32];

#if DEBUG_RECORDING || DEBUG_PLAYBACK
  FILE *playbackFile;
#endif
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

  bool isValid;
} Code;

#endif
