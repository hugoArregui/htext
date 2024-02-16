#include <string.h>
#include <sys/types.h>
#ifndef __H_TEXT_APP

#include "htext_platform.h"
#include <SDL2/SDL_render.h>
#include <SDL2/SDL_ttf.h>
#include <stdlib.h>

#define INDEX_SIZE 5000
#define KEY_PREFIX_MAX_SIZE 20
#define LINE_NUMBER_TEXTURE_CACHE_SIZE 5000

typedef struct {
  memory_index size;
  uint8_t *base;
  memory_index used;
  int32_t tempCount;
} MemoryArena;

typedef struct {
  memory_index used;
  MemoryArena *arena;
} TemporaryMemory;

typedef struct {
  bool is_initialized;
  MemoryArena arena;
} TransientState;

static void initializeArena(MemoryArena *arena, memory_index size, void *base) {
  arena->size = size;
  arena->base = (uint8_t *)base;
  arena->used = 0;
  arena->tempCount = 0;
}

inline static size_t getAlignmentOffset(MemoryArena *arena, size_t alignment) {
  size_t resultPointer = (size_t)arena->base + arena->used;
  size_t alignmentOffset = 0;

  size_t alignmentMask = alignment - 1;
  if (resultPointer & alignmentMask) {
    alignmentOffset = alignment - (resultPointer & alignmentMask);
  }

  return alignmentOffset;
}

inline static memory_index getArenaRemainingSize(MemoryArena *arena) {
  memory_index alignment = 4;
  size_t alignmentOffset = getAlignmentOffset(arena, alignment);
  return arena->size - arena->used - alignmentOffset;
}

#define pushStruct(arena, type, aligment)                                      \
  (type *)pushSize(arena, sizeof(type), aligment)
#define pushArray(arena, count, type, aligment)                                \
  (type *)pushSize(arena, (count) * sizeof(type), aligment)

#define DEFAULT_ALIGNMENT 4
void *pushSize(MemoryArena *arena, size_t size, size_t alignment) {
  size_t originalSize = size;
  size_t alignmentOffset = getAlignmentOffset(arena, alignment);
  size += alignmentOffset;

  assert((arena->used + size) <= arena->size);
  void *result = arena->base + arena->used + alignmentOffset;
  arena->used += size;

  assert(size >= originalSize);

  return result;
}

char *push_string(MemoryArena *arena, char *source) {
  uint32_t size = strlen(source) + 1;
  char *dest = (char *)pushSize(arena, size, DEFAULT_ALIGNMENT);
  for (uint32_t charIndex = 0; charIndex < size; ++charIndex) {
    dest[charIndex] = source[charIndex];
  }
  return dest;
}

#define ARENA_DEFAULT_ALIGNMENT 16
static void sub_arena(MemoryArena *result, MemoryArena *arena, size_t size,
                      size_t alignment) {
  result->size = size;
  result->base = (uint8_t *)pushSize(arena, size, alignment);
  result->used = 0;
  result->tempCount = 0;
}

/* inline TemporaryMemory beginTemporaryMemory(MemoryArena *arena) { */
/*   TemporaryMemory result; */
/*   result.arena = arena; */
/*   result.used = arena->used; */
/*   ++arena->tempCount; */
/*   return result; */
/* } */

/* inline void endTemporaryMemory(TemporaryMemory temporaryMemory) { */
/*   MemoryArena *arena = temporaryMemory.arena; */
/*   assert(arena->used >= temporaryMemory.used); */
/*   arena->used = temporaryMemory.used; */
/*   assert(arena->tempCount > 0); */
/*   arena->tempCount--; */
/* } */

/* inline void checkArena(MemoryArena *arena) { assert(arena->tempCount == 0); }
 */

enum AppMode { AppMode_normal, AppMode_ex, AppMode_insert, AppMode_count };

typedef struct {
  SDL_Texture *texture;
  int32_t w;
} CachedTexture;

struct Line;

typedef struct Line {
  char *text;
  int16_t len;
  int16_t max_len;
  struct Line *prev;
  struct Line *next;

  SDL_Texture *texture;
  int32_t texture_width;
} Line;

typedef struct {
  Line *line;
  int16_t line_num;
  int16_t column;
} Cursor;

typedef struct {
  int16_t start;
  int16_t size;
} Viewport;

typedef struct {
  Line *line;
  int16_t line_count;
  // NOTE: currently I use this only to optimize the rendering window, so it
  // could be much smaller, some cursor_line +/- amount_of_lines_to_render
  Line *index[INDEX_SIZE];

  Cursor cursor;

  Viewport viewport_v;
  Viewport viewport_h;

  MemoryArena arena;
  // IMPORTANT: this is not a double link list, only next pointers are valid
  Line *deleted_line;
} EditorFrame;

typedef struct {
  char *text;
  int16_t size;
  int16_t max_size;
  int16_t cursor_column;
  SDL_Texture *texture;
  int32_t texture_width;
} ExFrame;

#define ASCII_LOW 32
#define ASCII_HIGH 126

enum KeyStateMachineState {
  KeyStateMachine_Repetitions,
  KeyStateMachine_Operator,
  KeyStateMachine_Done,
};

struct KeyStateMachine;

typedef struct {
  SDL_Texture *texture;
  int32_t texture_width;

  enum KeyStateMachineState state;

  char keys[100];
  int8_t keys_size;

  int16_t repetitions;

  char operator[2];
  int8_t operator_size;
} KeyStateMachine;

typedef struct {
  int isInitialized;
  enum AppMode mode;

  MemoryArena arena;

  char status_message[200];
  char filename[200];

  EditorFrame editor_frame;
  ExFrame ex_frame;

  TTF_Font *font;
  int glyph_width[ASCII_HIGH - ASCII_LOW];
  int16_t font_h;

  CachedTexture appModeTextures[AppMode_count];

  int32_t line_number_texture_width;
  SDL_Texture *line_number_texture_cache[LINE_NUMBER_TEXTURE_CACHE_SIZE];

  SDL_Texture *filename_texture;
  int32_t filename_texture_width;

  SDL_Texture *ex_prefix_texture;
  int16_t ex_prefix_texture_width;

  KeyStateMachine normal_ksm;
} State;

typedef struct {
  State *state;
  SDL_Renderer *renderer;
  MemoryArena *transient_arena;
} RendererContext;

static inline void charcpy(char *dest, char *source, u_long size) {
  memcpy(dest, source, size);
}

#define __H_TEXT_APP
#endif
