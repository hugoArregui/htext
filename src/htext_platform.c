#include "htext_platform.h"
#include "htext_sdl.h"
#include <SDL.h>
#include <SDL2/SDL_keycode.h>
#include <SDL2/SDL_video.h>
#include <SDL_keycode.h>
#include <SDL_ttf.h>
#include <dlfcn.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>

#define DEBUG_FPS 0
#define DEBUG 1

#if defined(MAP_ANON) && !defined(MAP_ANONYMOUS)
#define MAP_ANONYMOUS MAP_ANON
#endif

int getGameLastModificationDate(const char *path, struct stat *fileStat,
                                time_t *modificationTime) {
  if (stat(path, fileStat) == 0) {
    *modificationTime = fileStat->st_mtime;
    return 0;
  } else {
    return -1;
  }
}

void loadGameCode(const char *sourcePath, Code *code) {
  code->handler = dlopen(sourcePath, RTLD_NOW);
  code->updateAndRender = 0;

  if (code->handler) {
    code->updateAndRender =
        (update_and_render *)dlsym(code->handler, "UpdateAndRender");
  } else {
    char *errstr = dlerror();
    if (errstr != NULL) {
      printf("A dynamic linking error occurred: (%s)\n", errstr);
    }
  }

  code->isValid = code->updateAndRender != NULL;
}

void unloadGameCode(Code *code) {
  if (code->handler) {
    dlclose(code->handler);
    code->handler = 0;
  }
  code->isValid = false;
}

int main(void) {
  const char *libSourcePath = "build/htext.so";

  uint64 permanentStorageSize = Megabytes(256);
  uint64 transientStorageSize = Gigabytes(3);
  PlatformState platformState = {};
  platformState.totalSize = permanentStorageSize + transientStorageSize;

  void *baseAddress = (void *)(0);
  // NOTE: MAP_ANONYMOUS content initialized to zero
  platformState.gameMemoryBlock =
      mmap(baseAddress, platformState.totalSize, PROT_READ | PROT_WRITE,
           MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);

  if (platformState.gameMemoryBlock == MAP_FAILED) {
    printf("failed to reserve memory %s\n", strerror(errno));
    return -1;
  }

  Memory memory = {};
  memory.permanentStorageSize = permanentStorageSize;
  memory.transientStorageSize = transientStorageSize;
  memory.permanentStorage = platformState.gameMemoryBlock;
  memory.transientStorage =
      ((uint8 *)memory.permanentStorage + memory.permanentStorageSize);

  int width = 1920;
  int height = 1080;

  SDL_ccode(SDL_Init(SDL_INIT_VIDEO));

  TTF_ccode(TTF_Init());

  SDL_Window *window = SDL_cpointer(
      SDL_CreateWindow("Play", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                       width, height, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE));
  SDL_Renderer *renderer =
      SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC);

#if DEBUG_WINDOW
  SDL_Window *debugWindow = SDL_cpointer(
      SDL_CreateWindow("Play debug", width - width * 0.3, height - height * 0.3,
                       100, 100, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE));

  SDL_Renderer *debugRenderer = SDL_cpointer(
      SDL_CreateRenderer(debugWindow, -1, SDL_RENDERER_PRESENTVSYNC));
#endif

  int monitorRefreshHz = 60;
  SDL_DisplayMode mode;
  if (SDL_GetWindowDisplayMode(window, &mode) == 0) {
    if (mode.refresh_rate > 0) {
      monitorRefreshHz = mode.refresh_rate;
    }
  }

  real32 gameUpdateHz = monitorRefreshHz;
  real32 targetSecondsPerFrame = 1.0f / (real32)gameUpdateHz;

#if DEBUG
  struct stat fileStat;
  time_t lastModificationTime = 0;
#endif

  Input input = {};
  input.dtForFrame = targetSecondsPerFrame;

#if DEBUG_PLAYBACK == PLAYBACK_RECORDING
  input.playbackFile = fopen("playback", "w");
  assert(input.playbackFile != NULL);
#elif DEBUG_PLAYBACK == PLAYBACK_PLAYING
  input.playbackFile = fopen("playback", "r");
  assert(input.playbackFile != NULL);
#endif
  Code code = {};
  loadGameCode(libSourcePath, &code);

  if (!code.isValid) {
    return -1;
  }

  uint32 targetFrameDurationMs = targetSecondsPerFrame * 1000;
  int running = 1;

  SDL_StartTextInput();
  while (running) {
    uint32 frameStartMs = SDL_GetTicks();

#if DEBUG
    time_t modificationTime;
    input.executableReloaded = false;
    if (getGameLastModificationDate(libSourcePath, &fileStat,
                                    &modificationTime) == 0) {
      if (lastModificationTime == 0) {
        lastModificationTime = modificationTime;
      } else if (lastModificationTime != modificationTime || !code.isValid) {
        input.executableReloaded = true;
        unloadGameCode(&code);
        loadGameCode(libSourcePath, &code);
        lastModificationTime = modificationTime;
      }
    }
#endif

    if (code.isValid) {
      SdlOffscreenBuffer buffer = {};
      buffer.renderer = renderer;

#if DEBUG_WINDOW
      buffer.debugRenderer = debugRenderer;
#endif

      SDL_GetWindowSize(window, &buffer.width, &buffer.height);

      if (code.updateAndRender(&memory, &input, &buffer) == 1) {
        running = 0;
      }

      input.keypressed = 0;
      input.text[0] = '\0';

      SDL_RenderPresent(renderer);

#if DEBUG_WINDOW
      SDL_RenderPresent(debugRenderer);
#endif
    }

    uint32 frameDurationMs = SDL_GetTicks() - frameStartMs;

    if (frameDurationMs < targetFrameDurationMs) {
      uint32 sleepMs = targetFrameDurationMs - frameDurationMs;
      if (sleepMs > 0) {
        SDL_Delay(sleepMs);
      }

      frameDurationMs = SDL_GetTicks() - frameStartMs;
#if DEBUG_FPS
      if (frameDurationMs > targetFrameDurationMs) {
        printf("frame missed due to sleep delay\n");
      }
#endif
    } else {
#if DEBUG_FPS
      printf("frame missed\n");
#endif
    }
  }

  unloadGameCode(&code);
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
#if DEBUG_WINDOW
  SDL_DestroyRenderer(debugRenderer);
  SDL_DestroyWindow(debugWindow);
#endif

#if DEBUG_PLAYBACK == PLAYBACK_PLAYING || DEBUG_PLAYBACK == PLAYBACK_RECORDING
  fclose(input.playbackFile);
#endif

  SDL_Quit();

  munmap(platformState.gameMemoryBlock, platformState.totalSize);
  return 0;
}
