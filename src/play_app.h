#ifndef __PLAY_PLAY

#include <stdlib.h>
#include <SDL2/SDL_ttf.h>
#include "play_math.h"
#include "play_platform.h"

struct MemoryArena
{
  memory_index size;
  uint8 *base;
  memory_index used;
  int32 tempCount;
};

struct TemporaryMemory
{
  memory_index used;
  MemoryArena* arena;
};

struct TaskWithMemory
{
  bool32 beingUsed;
  MemoryArena arena;
  TemporaryMemory memoryFlush;
};

struct TransientState;

struct TransientState
{
  bool32 isInitialized;
  MemoryArena arena;
};

static void
initializeArena(MemoryArena *arena, memory_index size, void *base)
{
  arena->size = size;
  arena->base = (uint8*)base;
  arena->used = 0;
  arena->tempCount = 0;
}

inline static size_t
getAlignmentOffset(MemoryArena *arena, size_t alignment)
{
  size_t resultPointer = (size_t) arena->base + arena->used;
  size_t alignmentOffset = 0;

  size_t alignmentMask = alignment -1;
  if (resultPointer & alignmentMask)
  {
    alignmentOffset = alignment - (resultPointer & alignmentMask);
  }

  return alignmentOffset;
}

inline memory_index
getArenaRemainingSize(MemoryArena* arena, memory_index alignment=4)
{
  size_t alignmentOffset = getAlignmentOffset(arena, alignment);
  return arena->size - arena->used - alignmentOffset;
}

#define pushStruct(arena, type, ...) (type *)pushSize_(arena, sizeof(type), ##__VA_ARGS__)
#define pushArray(arena, count, type, ...) (type *)pushSize_(arena, (count)*sizeof(type), ##__VA_ARGS__)
#define pushSize(arena, size, ...) pushSize_(arena, size, ##__VA_ARGS__)
void *
pushSize_(MemoryArena *arena, size_t size, size_t alignment=4)
{
  size_t originalSize = size;
  size_t alignmentOffset = getAlignmentOffset(arena, alignment);
  size += alignmentOffset;

  assert((arena->used + size) <= arena->size);
  void *result = arena->base + arena->used + alignmentOffset;
  arena->used += size;

  assert(size >= originalSize);

  return result;
}

inline char*
pushString(MemoryArena *arena, char* source)
{
  uint32 size = strlen(source) + 1;
  char* dest = (char*)pushSize(arena, size);
  for (uint32 charIndex = 0;
       charIndex < size;
       ++charIndex)
  {
    dest[charIndex] = source[charIndex];
  }
  return dest;
}

static void
subArena(MemoryArena *result, MemoryArena *arena, size_t size, size_t alignment=16)
{
  result->size = size;
  result->base = (uint8*)pushSize(arena, size, alignment);
  result->used = 0;
  result->tempCount = 0;
}

#define zeroStruct(instance) zeroSize(sizeof(instance), &(instance))
inline void
zeroSize(memory_index size, void *ptr)
{
  // TODO(casey): Check this guy for performance
  uint8 *byte = (uint8 *)ptr;
  while(size--)
  {
    *byte++ = 0;
  }
}

inline TemporaryMemory
beginTemporaryMemory(MemoryArena *arena)
{
  TemporaryMemory result;
  result.arena = arena;
  result.used = arena->used;
  ++arena->tempCount;
  return result;
}

inline void
endTemporaryMemory(TemporaryMemory temporaryMemory)
{
  MemoryArena* arena = temporaryMemory.arena;
  assert(arena->used >= temporaryMemory.used);
  arena->used = temporaryMemory.used;
  assert(arena->tempCount > 0);
  arena->tempCount--;
}

inline void
checkArena(MemoryArena* arena)
{
  assert(arena->tempCount == 0);
}

enum AppMode
{
  AppMode_normal,
  AppMode_ex,
  AppMode_insert
};

struct State
{
  bool32 isInitialized;
  AppMode mode;

  MemoryArena arena;

  char* text;
  char* exText;

  TTF_Font* font;
};

struct EntireFile
{
  uint32 contentsSize;
  void *contents;
};

#define __PLAY_PLAY
#endif
