#ifndef __PLAY_PLAY

#include "play_platform.h"
#include <SDL2/SDL_ttf.h>
#include <stdlib.h>

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

inline char *pushString(MemoryArena *arena, char *source) {
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

enum AppMode { AppMode_normal, AppMode_ex, AppMode_insert };

struct Line;

typedef struct Line {
  char *text;
  uint32 max_size;
  uint32 size;
  struct Line *prev;
  struct Line *next;
} Line;

typedef struct {
  Line* line;
  Line* deleted_line;
  Line* cursor_line;
  uint32 cursor_pos;
} EditorBuffer;

typedef struct {
  int isInitialized;
  enum AppMode mode;

  MemoryArena arena;

  EditorBuffer mainBuffer;
  EditorBuffer exBuffer;

  TTF_Font *font;
} State;

#define __PLAY_PLAY
#endif
