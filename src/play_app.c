#include "play_app.h"
#include "play_la.h"
#include "play_sdl.h"
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_keyboard.h>
#include <SDL2/SDL_keycode.h>
#include <SDL2/SDL_rect.h>
#include <SDL2/SDL_render.h>
#include <SDL2/SDL_scancode.h>
#include <SDL2/SDL_ttf.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>

const char *normalModeName = "NORMAL";
const char *exModeName = "EX";
const char *insertModeName = "INSERT";

extern UPDATE_AND_RENDER(UpdateAndRender) {
  assert(sizeof(State) <= memory->permanentStorageSize);

  uint32 backgroundColor = 0x00000000;
  uint32 fontColor = 0xFFFFFF00;
  uint32 cursorColor = fontColor | 0x9F;
  SDL_Color sdlModeColor = {UNHEX(0xFF000000)};
  SDL_Color sdlFontColor = {UNHEX(fontColor)};

#if DEBUG_WINDOW
  uint32 debugBackgroundColor = 0xFFFFFFFF;
  uint32 debugFontColor = 0x00000000;
  SDL_SetRenderDrawColor(buffer->debugRenderer, UNHEX(debugBackgroundColor));
  SDL_RenderClear(buffer->debugRenderer);
#endif

  SDL_ccode(SDL_SetRenderDrawColor(buffer->renderer, UNHEX(backgroundColor)));
  SDL_ccode(SDL_RenderClear(buffer->renderer));

  State *state = (State *)memory->permanentStorage;

  if (!state->isInitialized) {
    state->mode = AppMode_normal;

    // NOTE: this is executed once
    state->font = TTF_cpointer(
        TTF_OpenFont("/usr/share/fonts/TTF/IosevkaNerdFont-Regular.ttf", 20));

    initializeArena(&state->arena, memory->permanentStorageSize - sizeof(State),
                    (uint8 *)memory->permanentStorage + sizeof(State));

    // TODO: how are we going to handle this?
    state->text = (char *)pushSize(&state->arena, 1000, DEFAULT_ALIGMENT);
    state->text[0] = '\0';

    // TODO: how are we going to handle this?
    state->exText = (char *)pushSize(&state->arena, 1000, DEFAULT_ALIGMENT);
    state->text[0] = '\0';

    state->isInitialized = true;
  }

  // NOTE(casey): Transient initialization
  assert(sizeof(TransientState) <= memory->transientStorageSize);
  TransientState *transientState = (TransientState *)memory->transientStorage;
  if (!transientState->isInitialized) {
    initializeArena(&transientState->arena,
                    memory->transientStorageSize - sizeof(TransientState),
                    (uint8 *)memory->transientStorage + sizeof(TransientState));

    transientState->isInitialized = true;
  }

  if (input->executableReloaded) {
    printf("RELOAD\n");
  }

  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    switch (event.type) {
    case SDL_KEYDOWN:
      switch (state->mode) {
      case AppMode_normal:
        break;
      case AppMode_ex:
        if (event.key.keysym.scancode == SDL_SCANCODE_RETURN) {
          state->mode = AppMode_normal;

          if (strcmp(state->exText, "quit") == 0 ||
              strcmp(state->exText, "q") == 0) {
            return 1;
          } else {
            printf("invalid command %s\n", state->exText);
          }
        } else if (event.key.keysym.scancode == SDL_SCANCODE_BACKSPACE) {
          size_t len = strlen(state->exText);
          if (len > 0) {
            state->exText[len - 1] = '\0';
          }
        } else if (event.key.keysym.scancode == SDL_SCANCODE_ESCAPE) {
          state->mode = AppMode_normal;
          state->exText[0] = '\0';
        }
        break;
      case AppMode_insert:
        if (event.key.keysym.scancode == SDL_SCANCODE_RETURN) {
          size_t len = strlen(state->text);
          state->text[len] = '\n';
          state->text[len + 1] = '\0';
        } else if (event.key.keysym.scancode == SDL_SCANCODE_BACKSPACE) {
          size_t len = strlen(state->text);
          if (len > 0) {
            state->text[len - 1] = '\0';
            state->cursor_position--;
          }
        } else if (event.key.keysym.scancode == SDL_SCANCODE_ESCAPE) {
          state->mode = AppMode_normal;
        }
        break;
      default:
        assert(false);
        break;
      }
      break;
    case SDL_TEXTINPUT:
      switch (state->mode) {
      case AppMode_normal:
        for (size_t x = 0; x < strlen(event.text.text); ++x) {
          if (event.text.text[x] == ':') {
            state->mode = AppMode_ex;
            state->exText[0] = '\0';
          } else if (event.text.text[x] == 'i') {
            state->mode = AppMode_insert;
          } else if (event.text.text[x] == 'h') {
            if (state->cursor_position > 0) {
              state->cursor_position--;
            }
          } else if (event.text.text[x] == 'l') {
            if (state->cursor_position < (strlen(state->text) - 1)) {
              state->cursor_position++;
            }
          }
        }
        break;
      case AppMode_ex:
        strcat(state->exText, event.text.text);
        break;
      case AppMode_insert:
        strcat(state->text, event.text.text);

        state->cursor_position += strlen(event.text.text);
        break;
      default:
        assert(false);
        break;
      }
      break;
    case SDL_QUIT: /* if mouse click to close window */
      return 1;
    }
  }

  // -------- rendering
  const int font_h = TTF_FontHeight(state->font);
  {
    // render text frame
    const int font_w = font_h / 2;

    const int margin_x = buffer->width * 0.01;
    const int margin_y = buffer->height * 0.01;

    int x = margin_x;
    int y = margin_y;

    unsigned long cursor_ix = state->cursor_position; // TODO naming
    V2i cursor_pos = {-1, -1};

    SDL_Rect dest;

    for (unsigned long i = 0; i < strlen(state->text); ++i) {
      uint32 ch = state->text[i];

      if (ch == '\n') {
        y += font_h;
        x = margin_x;
        cursor_ix++;
        continue;
      }

      SDL_Surface *surface = TTF_cpointer(TTF_RenderGlyph_Solid(state->font, ch, sdlFontColor));
      SurfaceRenderer sr = SR_create(buffer->renderer, surface);
      int w = sr.surface->w;
      SR_renderFullSizeAndDestroy(&sr, x, y);
      if (i == cursor_ix) {
        cursor_pos.x = x;
        cursor_pos.y = y;
      }
      x += w;
    }

    {
      // render cursor
      dest.x = cursor_pos.x == -1 ? x : cursor_pos.x;
      dest.y = cursor_pos.y == -1 ? y : cursor_pos.y;
      dest.w = font_w;
      dest.h = font_h;

      SDL_ccode(SDL_SetRenderDrawColor(buffer->renderer, UNHEX(cursorColor)));
      SDL_ccode(
          SDL_SetRenderDrawBlendMode(buffer->renderer, SDL_BLENDMODE_BLEND));
      if (state->mode != AppMode_ex) {
        SDL_ccode(SDL_RenderFillRect(buffer->renderer, &dest));
      } else {
        SDL_ccode(SDL_RenderDrawRect(buffer->renderer, &dest));
      }
    }
  }

  // render mode name
  {
    const char *modeName = NULL;
    switch (state->mode) {
    case AppMode_normal: {
      modeName = normalModeName;
      break;
    }
    case AppMode_ex: {
      modeName = exModeName;
      break;
    }
    case AppMode_insert: {
      modeName = insertModeName;
      break;
    }
    default:
      assert(false);
      break;
    }

    SDL_Surface *text_surface =
      TTF_cpointer(TTF_RenderText_Solid(state->font, modeName, sdlModeColor));

    SurfaceRenderer sr = SR_create(buffer->renderer, text_surface);
    SR_renderFullSizeAndDestroy(
                                &sr,
                                buffer->width - text_surface->w - buffer->width * 0.01,
                                buffer->height - text_surface->h - 0.03 * buffer->height);
  }

  if (state->mode == AppMode_ex) {
    char *fText = (char *)pushSize(&transientState->arena,
                                   strlen(state->exText) + 1, DEFAULT_ALIGMENT);
    sprintf(fText, ":%s", state->exText);

    SDL_Surface *text_surface =
      TTF_cpointer(TTF_RenderText_Solid(state->font, fText, sdlFontColor));

    SurfaceRenderer sr = SR_create(buffer->renderer, text_surface);
    SR_renderFullSizeAndDestroy(
                                &sr,
                                0.01 * buffer->width,
                                buffer->height - text_surface->h - 0.01 * buffer->height);
  }

#if DEBUG_WINDOW
  {
    char *text =
        (char *)pushSize(&transientState->arena, 1000, DEFAULT_ALIGMENT);
    SDL_Color color = {UNHEX(debugFontColor)};
    sprintf(text, "Cursor position: %lu\nText length: %lu",
            state->cursor_position, strlen(state->text));

    const int margin_x = buffer->width * 0.01;
    const int margin_y = buffer->height * 0.01;

    int x = margin_x;
    int y = margin_y;

    for (unsigned long i = 0; i < strlen(text); ++i) {
      uint32 ch = text[i];

      if (ch == '\n') {
        y += font_h;
        x = margin_x;
        continue;
      }

      SDL_Surface *surface = TTF_cpointer(TTF_RenderGlyph_Solid(state->font, ch, color));
      SurfaceRenderer sr = SR_create(buffer->debugRenderer, surface);
      int w = sr.surface->w;
      SR_renderFullSizeAndDestroy(&sr, x, y);
      x += w;
    }
  }
#endif

  return 0;
}
