#include "play_app.h"
#include <SDL2/SDL_keyboard.h>
#include <SDL2/SDL_keycode.h>
#include <cstring>

EntireFile
readEntireFile(char *path)
{
    EntireFile result = {};

    FILE *in = fopen(path, "rb");
    if (in)
    {
        fseek(in, 0, SEEK_END);
        result.contentsSize = ftell(in);
        fseek(in, 0, SEEK_SET);
        result.contents = malloc(result.contentsSize);
        fread(result.contents, result.contentsSize, 1, in);
        fclose(in);
    }
    else
    {
        printf("ERROR: Cannot open file %s\n", path);
    }

    return result;
}

extern "C" UPDATE_AND_RENDER(UpdateAndRender)
{
  assert(sizeof(State) <= memory->permanentStorageSize);

  State *state = (State *)memory->permanentStorage;

  if(!state->isInitialized)
  {
    state->mode = AppMode_normal;

    // NOTE: this is executed once
    state->font = TTF_OpenFont("/usr/share/fonts/TTF/Vera.ttf", 24);
    if (!state->font)
    {
      printf("Failed to load font: %s\n", TTF_GetError());
    }

    initializeArena(&state->arena,
                    memory->permanentStorageSize - sizeof(State),
                    (uint8 *)memory->permanentStorage + sizeof(State));

    // TODO: how are we going to handle this?
    state->text = (char*)pushSize(&state->arena, 1000);

    // TODO: how are we going to handle this?
    state->exText = (char*)pushSize(&state->arena, 1000);

    state->isInitialized = true;
  }

  //NOTE(casey): Transient initialization
  assert(sizeof(TransientState) <= memory->transientStorageSize);
  TransientState *transientState = (TransientState *)memory->transientStorage;
  if (!transientState->isInitialized)
  {
    initializeArena(&transientState->arena,
                    memory->transientStorageSize - sizeof(TransientState),
                    (uint8*) memory->transientStorage + sizeof(TransientState));

    transientState->isInitialized = true;
  }

  if (input->executableReloaded)
  {
    printf("RELOAD\n");
  }

  SDL_Color color = { 0, 0, 0 };

  SDL_StartTextInput();

  static const char* normalModeName = "NORMAL";
  static const char* exModeName = "EX";
  static const char* insertModeName = "INSERT";

  const char* modeName = NULL;
  if (state->mode == AppMode_normal)
  {
    modeName = normalModeName;
    if (strcmp(input->text, ":") == 0)
    {
      printf("switching to ex mode\n");
      state->mode = AppMode_ex;
      state->exText[0] = ':';
      state->exText[1] = '\0';
    }
  }
  else if (state->mode == AppMode_ex)
  {
    modeName = exModeName;
    strcat(state->exText, input->text);

    if (input->keypressed == SDLK_RETURN)
    {
      printf("switching to normal mode\n");
      state->mode = AppMode_normal;

      if (strcmp(state->exText + 1, "quit") == 0)
      {
        return 1;
      }
      else
      {
        printf("invalid command %s\n", state->exText + 1);
      }
    }

    if (strlen(state->exText) > 0)
    {
      SDL_Surface* text_surf = TTF_RenderText_Solid(state->font, state->exText, color);
      if (!text_surf)
      {
        printf("Failed to render text: %s\n", TTF_GetError());
      }

      SDL_Texture* text = SDL_CreateTextureFromSurface(buffer->renderer, text_surf);

      SDL_Rect dest;
      dest.x = 0.01 * buffer->width;
      dest.y = buffer->height - text_surf->h - 0.01 * buffer->height;
      dest.w = text_surf->w;
      dest.h = text_surf->h;
      SDL_RenderCopy(buffer->renderer, text, NULL, &dest);

      SDL_DestroyTexture(text);
      SDL_FreeSurface(text_surf);
    }
  }
  else if (state->mode == AppMode_insert)
  {
    modeName = insertModeName;
  }
  else
  {
    assert(false);
  }


  // render mode name
  {
    SDL_Color color = { 255, 0, 0 };
    SDL_Surface* text_surf = TTF_RenderText_Solid(state->font, modeName, color);
    if (!text_surf)
    {
      printf("Failed to render text: %s\n", TTF_GetError());
    }

    SDL_Texture* text = SDL_CreateTextureFromSurface(buffer->renderer, text_surf);

    SDL_Rect dest;
    dest.x = buffer->width - text_surf->w - 0.01 * buffer->width;
    dest.y = 0;
    dest.w = text_surf->w;
    dest.h = text_surf->h;
    SDL_RenderCopy(buffer->renderer, text, NULL, &dest);

    SDL_DestroyTexture(text);
    SDL_FreeSurface(text_surf);
  }


  return 0;
}
