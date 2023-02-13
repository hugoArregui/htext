#include <sys/mman.h>
#include <stdio.h>
#include <unistd.h>
#include <dlfcn.h>
#include <errno.h>
#include <SDL.h>
// #include <SDL_keycode.h>
// #include <SDL_render.h>
#include <SDL_ttf.h>
#include "main.h"

#define DEBUG_FPS 1
#define DEBUG 1

int
getGameLastModificationDate(const char* path, struct stat* fileStat, time_t* modificationTime)
{
  if (stat(path, fileStat) == 0)
  {
    *modificationTime = fileStat->st_mtime;
    return 0;
  }
  else
  {
    printf("failed to get file last modification time %s", strerror(errno));
    return -1;
  }
}

void
loadGameCode(const char* sourcePath, Code* code)
{
  code->handler = dlopen(sourcePath, RTLD_NOW);
  code->updateAndRender = 0;

  if (code->handler)
  {
    code->updateAndRender = (update_and_render *) dlsym(code->handler, "UpdateAndRender");
  }
  else
  {
    char *errstr = dlerror();
    if (errstr != NULL)
    {
      printf("A dynamic linking error occurred: (%s)\n", errstr);
    }
  }

  code->isValid = code->updateAndRender != NULL;
}

void
unloadGameCode(Code* code)
{
  if (code->handler)
  {
    dlclose(code->handler);
    code->handler = 0;
  }
  code->isValid = false;
}

int
main()
{
  const char* libSourcePath = "/home/hugo/workspace/play/build/play.so";

  uint64 permanentStorageSize = Megabytes(256);
  uint64 transientStorageSize = Gigabytes(3);
  PlatformState platformState = {};
  platformState.totalSize = permanentStorageSize + transientStorageSize;

  void *baseAddress = (void *)(0);
  // NOTE: MAP_ANONYMOUS content initialized to zero
  platformState.gameMemoryBlock = mmap(baseAddress, platformState.totalSize,
                                       PROT_READ | PROT_WRITE,
                                       MAP_ANONYMOUS | MAP_PRIVATE,
                                       -1, 0);

  if (platformState.gameMemoryBlock == MAP_FAILED)
  {
    printf("failed to reserve memory %s\n", strerror(errno));
    return -1;
  }

  Memory memory = {};
  memory.permanentStorageSize = permanentStorageSize;
  memory.transientStorageSize = transientStorageSize;
  memory.permanentStorage = platformState.gameMemoryBlock;
  memory.transientStorage = ((uint8 *)memory.permanentStorage + memory.permanentStorageSize);

  int width = 1920;
  int height = 1080;

  if (SDL_Init(SDL_INIT_VIDEO) < 0)
  {
    printf("SDL could not initialize: %s\n", SDL_GetError());
    return -1;
  }

  if (TTF_Init() < 0)
  {
    printf("Error initializing SDL_ttf: %s\n", TTF_GetError());
  }

  SDL_Window* window = SDL_CreateWindow("Play",
                                        SDL_WINDOWPOS_UNDEFINED,
                                        SDL_WINDOWPOS_UNDEFINED,
                                        width,
                                        height,
                                        SDL_WINDOW_SHOWN|SDL_WINDOW_RESIZABLE);
  if (window == NULL)
  {
    printf("Window could not be created! SDL_Error: %s\n", SDL_GetError());
    return -1;
  }
  SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC);

  SDL_Texture* texture = SDL_CreateTexture(renderer,
                                           SDL_PIXELFORMAT_ARGB8888,
                                           SDL_TEXTUREACCESS_STREAMING,
                                           width,
                                           height);
  if (texture == NULL)
  {
    printf("cannot create texture: %s\n ", SDL_GetError());
    return -1;
  }

  // SdlOffscreenBuffer globalBackBuffer;
  // globalBackBuffer.bytesPerPixel = BITMAP_BYTES_PER_PIXEL;
  // globalBackBuffer.pitch = align16(width * globalBackBuffer.bytesPerPixel);
  // globalBackBuffer.width = width;
  // globalBackBuffer.height = height;

  // int bitmapMemorySize = globalBackBuffer.pitch*globalBackBuffer.height;
  // globalBackBuffer.memory = mmap(0, bitmapMemorySize, PROT_READ | PROT_WRITE,
  //                                MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

  // if (globalBackBuffer.memory == MAP_FAILED)
  // {
  //   printf("cannot allocate GlobalBackbuffer memory\n");
  //   return -1;
  // }


  int monitorRefreshHz = 60;
  SDL_DisplayMode mode;
  if (SDL_GetWindowDisplayMode(window, &mode) == 0)
  {
    if (mode.refresh_rate > 0)
    {
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

  if (!code.isValid)
  {
    return -1;
  }

  uint32 targetFrameDurationMs = targetSecondsPerFrame * 1000;
  bool running = 1;
  SDL_Event event;
  int keypressed;

  while (running)
  {
    uint32 frameStartMs = SDL_GetTicks();

#if DEBUG
    time_t modificationTime;
    input.executableReloaded = false;
    if (getGameLastModificationDate(libSourcePath,
                                    &fileStat,
                                    &modificationTime) == 0)
    {
      if (lastModificationTime == 0)
      {
        lastModificationTime = modificationTime;
      } else if (lastModificationTime != modificationTime || !code.isValid)
      {
        input.executableReloaded = true;
        unloadGameCode(&code);
        loadGameCode(libSourcePath, &code);
        lastModificationTime = modificationTime;
      }
    }
#endif


    while (SDL_PollEvent(&event))
    {
      switch (event.type)
      {
      case SDL_KEYDOWN:
        keypressed = event.key.keysym.sym;
        if (keypressed == SDLK_ESCAPE)
        {
          running = 0;
          break;
        }

        break;
      case SDL_QUIT: /* if mouse click to close window */
      {
        running = 0;
        break;
      }
      case SDL_KEYUP: {
        break;
      }
      }

    }

    if (code.isValid)
    {



  TTF_Font* font = TTF_OpenFont("font.ttf", 24);
  if ( !font ) {
    printf("Failed to load font: %s\n", TTF_GetError());
  }
// Set color to black
  SDL_Color color = { 0, 0, 0 };

  SDL_Surface* text_surf = TTF_RenderText_Solid( font, "Hello World!", color );
  if ( !text_surf ) {
    printf("Failed to render text: %s\n", TTF_GetError());
  }

	SDL_Texture*text = SDL_CreateTextureFromSurface(renderer, text_surf);

	SDL_Rect dest;
  dest.x = 320 - (text_surf->w / 2.0f);
  dest.y = 240;
  dest.w = text_surf->w;
  dest.h = text_surf->h;
  SDL_RenderCopy(renderer, text, NULL, &dest);


      // SdlOffscreenBuffer buffer = {};
      // buffer.memory = ((uint8*)globalBackBuffer.memory) + (globalBackBuffer.height - 1) * globalBackBuffer.pitch;
      // buffer.width = globalBackBuffer.width;
      // buffer.height = globalBackBuffer.height;
      // buffer.pitch = -globalBackBuffer.pitch;
      // code.updateAndRender(&memory, &input, &buffer);

      // SDL_UpdateTexture(texture, 0, globalBackBuffer.memory, globalBackBuffer.pitch);

      // int offsetX = 0;
      // int offsetY = 0;
      // SDL_Rect srcRect = {0, 0, buffer.width, buffer.height};
      // SDL_Rect destRect = {offsetX, offsetY, buffer.width, buffer.height};
      // SDL_RenderCopy(renderer, texture, &srcRect, &destRect);


      // code.updateAndRender(&memory, &input, &buffer);

      SDL_RenderPresent(renderer);
    }

    uint32 frameDurationMs = SDL_GetTicks() - frameStartMs;

    if (frameDurationMs < targetFrameDurationMs)
    {
      uint32 sleepMs = targetFrameDurationMs - frameDurationMs;
      if (sleepMs > 0)
      {
        SDL_Delay(sleepMs);
      }

      frameDurationMs = SDL_GetTicks() - frameStartMs;
#if DEBUG_FPS
      if (frameDurationMs > targetFrameDurationMs)
      {
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
