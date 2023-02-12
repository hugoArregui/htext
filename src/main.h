#ifndef __PLAY_MAIN

#include <fcntl.h>
#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include <float.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

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
#define align4(value) ((value + 3) & ~3)
#define align8(value) ((value + 7) & ~7)
#define align16(value) ((value + 15) & ~15)
#define BITMAP_BYTES_PER_PIXEL 4

#define Kilobytes(Value) ((Value)*1024LL)
#define Megabytes(Value) (Kilobytes(Value)*1024LL)
#define Gigabytes(Value) (Megabytes(Value)*1024LL)
#define Terabytes(Value) (Gigabytes(Value)*1024LL)

struct PlatformState
{
    uint64 totalSize;
    void *gameMemoryBlock;

    bool32 isRecording;
    FILE* recordingHandle;

    FILE* playbackHandle;
    bool32 isPlaying;
};

struct SdlOffscreenBuffer
{
    // NOTE(casey): Pixels are alwasy 32-bits wide, Memory Order BB GG RR XX
    void *memory;
    int width;
    int height;
    int pitch;
    int bytesPerPixel;
};

typedef struct Memory
{
    uint64 permanentStorageSize;
    void *permanentStorage; // NOTE(casey): REQUIRED to be cleared to zero at startup

    uint64 transientStorageSize;
    void *transientStorage; // NOTE(casey): REQUIRED to be cleared to zero at startup
} Memory;

/* typedef struct GameButtonState */
/*
{ */
/*     bool32 endedDown; */
/*     bool32 pressedNow; */
/* } GameButtonState; */

/* int buttonKeyCodes[] =
{ */
/*     SDL_SCANCODE_W, */
/*     SDL_SCANCODE_S, */
/*     SDL_SCANCODE_A, */
/*     SDL_SCANCODE_D, */
/*     SDL_SCANCODE_UP, */
/*     SDL_SCANCODE_DOWN, */
/*     SDL_SCANCODE_LEFT, */
/*     SDL_SCANCODE_RIGHT, */
/*     SDL_SCANCODE_Q, */
/*     SDL_SCANCODE_E, */
/*     SDL_SCANCODE_ESCAPE, */
/*     SDL_SCANCODE_SPACE, */
/*     SDL_SCANCODE_P, */
/*     SDL_SCANCODE_L, */
/*     SDL_SCANCODE_F */
/* }; */

struct Input
{
    real32 dtForFrame;
    bool32 executableReloaded;
/*     union */
/*
{ */
/*         GameButtonState buttons[15]; */
/*         struct */
/*
{ */
/*             GameButtonState moveUp; */
/*             GameButtonState moveDown; */
/*             GameButtonState moveLeft; */
/*             GameButtonState moveRight; */

/*             GameButtonState actionUp; */
/*             GameButtonState actionDown; */
/*             GameButtonState actionLeft; */
/*             GameButtonState actionRight; */

/*             GameButtonState leftShoulder; */
/*             GameButtonState rightShoulder; */

/*             GameButtonState back; */
/*             GameButtonState start; */

/*             GameButtonState pause; */
/*             GameButtonState loop; */

/*             GameButtonState fullscreen; */

/*             // NOTE(casey): all buttons must be added above this line */

/*             GameButtonState terminator; */
/*         }; */
/*     }; */
};

#define UPDATE_AND_RENDER(name) void name(Memory *memory, Input* input, SdlOffscreenBuffer* backBuffer)
typedef UPDATE_AND_RENDER(update_and_render);

struct Code
{
    void* handler;
    // IMPORTANT(casey): Either of the callbacks can be 0!  You must
    // check before calling.
    update_and_render *updateAndRender;

    bool32 isValid;
};

inline uint16
safeTruncateToUint16(int32 value)
{
    assert(value <= 65535);
    assert(value >= 0);

    uint16 result = (uint16) value;
    return result;
}

inline int16
safeTruncateToInt16(int32 value)
{
    // TODO(casey): Defines for maximum values
    assert(value < 32767);
    assert(value >= -32768);
    int16 result = (int16)value;
    return result;
}

#define __PLAY_MAIN
#endif
