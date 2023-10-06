#include "play_platform.h"
#include <SDL.h>
#include <SDL2/SDL_keycode.h>
#include <SDL_keycode.h>
#include <SDL_ttf.h>
#include <dlfcn.h>
#include <errno.h>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdbool.h>

#define DEBUG_FPS 0
#define DEBUG 1
#define DEBUG_LOAD 0

int getGameLastModificationDate(const char *path, struct stat *fileStat,
                                time_t *modificationTime) {
  if (stat(path, fileStat) == 0) {
    *modificationTime = fileStat->st_mtime;
    return 0;
  } else {
#if DEBUG_LOAD
    printf("failed to get file last modification time %s\n", strerror(errno));
#endif
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
  const char *libSourcePath = "/home/hugo/workspace/play/build/play.so";

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

  if (SDL_Init(SDL_INIT_VIDEO) < 0) {
    printf("SDL could not initialize: %s\n", SDL_GetError());
    return -1;
  }

  if (TTF_Init() < 0) {
    printf("Error initializing SDL_ttf: %s\n", TTF_GetError());
  }

  SDL_Window *window =
      SDL_CreateWindow("Play", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                       width, height, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
  if (window == NULL) {
    printf("Window could not be created! SDL_Error: %s\n", SDL_GetError());
    return -1;
  }
  SDL_Renderer *renderer =
      SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC);

  SDL_Texture *texture =
      SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                        SDL_TEXTUREACCESS_STREAMING, width, height);
  if (texture == NULL) {
    printf("cannot create texture: %s\n ", SDL_GetError());
    return -1;
  }

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
      SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
      SDL_RenderClear(renderer);

      SdlOffscreenBuffer buffer = {};
      buffer.renderer = renderer;
      SDL_GetWindowSize(window, &buffer.width, &buffer.height);

      if (code.updateAndRender(&memory, &input, &buffer) == 1) {
        running = 0;
      }

      input.keypressed = 0;
      input.text[0] = '\0';

      SDL_RenderPresent(renderer);
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
  SDL_Quit();

  munmap(platformState.gameMemoryBlock, platformState.totalSize);
  return 0;
}
