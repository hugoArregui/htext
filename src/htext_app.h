#ifndef __H_TEXT_H_TEXT

#include <SDL2/SDL_render.h>
#include "htext_platform.h"
#include <SDL2/SDL_ttf.h>
#include <stdlib.h>

#define INDEX_SIZE 5000

typedef struct {
  memory_index size;
  uint8 *base;
  memory_index used;
  int32 tempCount;
} MemoryArena;

typedef struct {
  memory_index used;
  MemoryArena *arena;
} TemporaryMemory;

typedef struct {
  bool32 isInitialized;
  MemoryArena arena;
} TransientState;

static void initializeArena(MemoryArena *arena, memory_index size, void *base) {
  arena->size = size;
  arena->base = (uint8 *)base;
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

#define DEFAULT_ALIGMENT 4
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

char *pushString(MemoryArena *arena, char *source) {
  uint32 size = strlen(source) + 1;
  char *dest = (char *)pushSize(arena, size, DEFAULT_ALIGMENT);
  for (uint32 charIndex = 0; charIndex < size; ++charIndex) {
    dest[charIndex] = source[charIndex];
  }
  return dest;
}

#define ARENA_DEFAULT_ALIGMENT 16
/* static void */
/* subArena(MemoryArena *result, MemoryArena *arena, size_t size, size_t
 * alignment) */
/* { */
/*   result->size = size; */
/*   result->base = (uint8*)pushSize(arena, size, alignment); */
/*   result->used = 0; */
/*   result->tempCount = 0; */
/* } */

inline TemporaryMemory beginTemporaryMemory(MemoryArena *arena) {
  TemporaryMemory result;
  result.arena = arena;
  result.used = arena->used;
  ++arena->tempCount;
  return result;
}

inline void endTemporaryMemory(TemporaryMemory temporaryMemory) {
  MemoryArena *arena = temporaryMemory.arena;
  assert(arena->used >= temporaryMemory.used);
  arena->used = temporaryMemory.used;
  assert(arena->tempCount > 0);
  arena->tempCount--;
}

inline void checkArena(MemoryArena *arena) { assert(arena->tempCount == 0); }

enum AppMode { AppMode_normal, AppMode_ex, AppMode_insert, AppMode_count};

typedef struct {
  SDL_Texture* texture;
  int32 w;
} CachedTexture;

struct Line;

typedef struct Line {
  char *text;
  uint32 max_size;
  uint32 size;
  struct Line *prev;
  struct Line *next;

  SDL_Texture* texture;
  int32 texture_width;
} Line;

typedef struct {
  Line* line;
  uint32 line_num;
  uint32 column;
} Cursor;

typedef struct {
  Line* line;
  uint32 line_count;
  // NOTE: currently I use this only to optimize the rendering window, so it could be much smaller, some cursor_line +/- amount_of_lines_to_render
  Line* index[INDEX_SIZE];

  Cursor cursor;

  // IMPORTANT: this is not a double link list, only next pointers are valid
  Line* deleted_line;
} MainFrame;

typedef struct {
  Line* line;
  Cursor cursor;
} ExFrame;

#define ASCII_LOW 32
#define ASCII_HIGH 126

typedef struct {
  int isInitialized;
  enum AppMode mode;

  MemoryArena arena;

  MainFrame main_frame;
  ExFrame ex_frame;

  TTF_Font *font;
  int glyph_width[ASCII_HIGH-ASCII_LOW];
  int32 font_h;

  CachedTexture appModeTextures[AppMode_count];

  char *filename;
  SDL_Texture *filename_texture;
  int filename_texture_width;

  SDL_Texture* ex_prefix_texture;
  int ex_prefix_texture_width;
} State;

#define __H_TEXT_H_TEXT
#endif
