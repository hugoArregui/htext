#include "play_app.h"
#include <SDL2/SDL_keyboard.h>
#include <SDL2/SDL_keycode.h>
#include <SDL2/SDL_scancode.h>
#include <stdio.h>
#include <string.h>

const char* normalModeName = "NORMAL";
const char* exModeName = "EX";
const char* insertModeName = "INSERT";

void sttfcc(int code)
{
  if (code < 0)
  {
    printf("Error : %s\n", TTF_GetError());
    exit(1);
  }
}

extern UPDATE_AND_RENDER(UpdateAndRender)
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
    state->text = (char*)pushSize(&state->arena, 1000, DEFAULT_ALIGMENT);

    // TODO: how are we going to handle this?
    state->exText = (char*)pushSize(&state->arena, 1000, DEFAULT_ALIGMENT);

    state->isInitialized = TRUE;
  }

  //NOTE(casey): Transient initialization
  assert(sizeof(TransientState) <= memory->transientStorageSize);
  TransientState *transientState = (TransientState *)memory->transientStorage;
  if (!transientState->isInitialized)
  {
    initializeArena(&transientState->arena,
                    memory->transientStorageSize - sizeof(TransientState),
                    (uint8*) memory->transientStorage + sizeof(TransientState));

    transientState->isInitialized = TRUE;
  }

  if (input->executableReloaded)
  {
    printf("RELOAD\n");
  }

  SDL_Event event;
  while (SDL_PollEvent(&event))
  {
    switch (event.type)
    {
    case SDL_KEYDOWN:
      switch (state->mode)
      {
      case AppMode_normal:
        break;
      case AppMode_ex:
        if (event.key.keysym.scancode == SDL_SCANCODE_RETURN)
        {
          state->mode = AppMode_normal;

          if (strcmp(state->exText, "quit") == 0 || strcmp(state->exText, "q") == 0)
          {
            return 1;
          }
          else
          {
            printf("invalid command %s\n", state->exText);
          }
        }
        else if (event.key.keysym.scancode == SDL_SCANCODE_BACKSPACE)
        {
          size_t len = strlen(state->exText);
          if (len > 1)
          {
            state->exText[len - 1] = '\0';
          }
        }
        break;
      case AppMode_insert:
        if (event.key.keysym.scancode == SDL_SCANCODE_RETURN)
        {
          size_t len = strlen(state->text);
          if (len == 0)
          {
            state->text[0] = '\n';
            state->text[1] = '\0';
          }
          else
          {
            state->text[len-1] = '\n';
            state->text[len] = '\0';
          }
        }
        else if (event.key.keysym.scancode == SDL_SCANCODE_BACKSPACE)
        {
          size_t len = strlen(state->text);
          if (len > 1)
          {
            state->text[len - 1] = '\0';
          }
        }
        else if (event.key.keysym.scancode == SDL_SCANCODE_ESCAPE)
        {
          state->mode = AppMode_normal;
        }
        break;
      default:
        assert(FALSE);
        break;
      }
      break;
    case SDL_TEXTINPUT:
      switch (state->mode)
      {
      case AppMode_normal:
        for (size_t x = 0; x < strlen(event.text.text); ++x)
        {
          if (event.text.text[x] == ':')
          {
            state->mode = AppMode_ex;
            state->exText[0] = '\0';
          }
          else if (event.text.text[x] == 'i')
          {
            state->mode = AppMode_insert;
          }
        }
        break;
      case AppMode_ex:
        strcat(state->exText, event.text.text);
        break;
      case AppMode_insert:
        strcat(state->text, event.text.text);
        break;
      default:
        assert(FALSE);
        break;
      }
      break;
    case SDL_QUIT: /* if mouse click to close window */
      return 1;
    }
  }

  static int count = -1;
  count++;
  static char blink = ' ';

  // render
  const char* modeName = NULL;
  switch (state->mode)
  {
  case AppMode_normal: {
    modeName = normalModeName;
    break;
  }
  case AppMode_ex: {
    modeName = exModeName;
    SDL_Color color = { 0, 0, 0, 0 };

    char* fText = (char*)pushSize(&transientState->arena, strlen(state->exText) + 3, DEFAULT_ALIGMENT);
    if (count >= 80)
    {
      count = 0;
      blink = blink == ' ' ? '*' : ' ';
    }

    sprintf(fText, ":%s%c", state->exText, blink);
    SDL_Surface* text_surf = TTF_RenderText_Solid(state->font, fText, color);
    if (!text_surf)
    {
      printf("Failed to render ex command text: %s\n", TTF_GetError());
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
    break;
  }
  case AppMode_insert: {
    modeName = insertModeName;
    break;
  }
  default:
    assert(FALSE);
    break;
  }

  // render text
  if (strlen(state->text) > 0)
  {
    SDL_Color color = { 0, 0, 0, 0 };

    SDL_Surface* text_surf = TTF_RenderText_Solid(state->font, state->text, color);
    if (!text_surf)
    {
      printf("Failed to render ex command text: %s\n", TTF_GetError());
    }

    SDL_Texture* text = SDL_CreateTextureFromSurface(buffer->renderer, text_surf);

    SDL_Rect dest;
    dest.x = buffer->width * 0.01;
    dest.y = buffer->height * 0.01;
    dest.w = text_surf->w;
    dest.h = text_surf->h;
    SDL_RenderCopy(buffer->renderer, text, NULL, &dest);

    SDL_DestroyTexture(text);
    SDL_FreeSurface(text_surf);
  }

  // Render status separator
  {
    SDL_SetRenderDrawColor(buffer->renderer, 0, 0, 0, 0);
    int h;
    int w;
    sttfcc(TTF_SizeUTF8(state->font, "test", &w, &h));

    int y = buffer->height - h * 2;
    SDL_RenderDrawLine(buffer->renderer, 0, y, buffer->width, y);
  }


  // render mode name
  {
    SDL_Color color = { 255, 0, 0, 0 };
    SDL_Surface* text_surf = TTF_RenderText_Solid(state->font, modeName, color);
    if (!text_surf)
    {
      printf("Failed to render mode name: %s\n", TTF_GetError());
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
