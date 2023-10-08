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

const uint32 backgroundColor = 0x00000000;
const uint32 fontColor = 0xFFFFFF00;
const uint32 cursorColor = fontColor | 0x9F;

void render_cursor(SDL_Renderer *renderer, SDL_Rect dest, bool32 fill) {
  SDL_ccode(SDL_SetRenderDrawColor(renderer, UNHEX(cursorColor)));
  if (fill) {
    SDL_ccode(SDL_RenderFillRect(renderer, &dest));
  } else {
    SDL_ccode(SDL_RenderDrawRect(renderer, &dest));
  }
}

void eb_insert_text(MemoryArena *arena, EditorBuffer *buffer, char *text) {
  uint64 text_len = strlen(buffer->text);
  uint64 input_len = strlen(text);

  size_t temp_size = text_len + input_len - buffer->cursor_pos + 1;
  char *temp = (char *)pushSize(arena, temp_size, DEFAULT_ALIGMENT);

  memcpy(temp, text, input_len);
  memcpy(temp + input_len, buffer->text + buffer->cursor_pos,
         text_len - buffer->cursor_pos + 1);
  memcpy(buffer->text + buffer->cursor_pos, temp, temp_size);
  buffer->cursor_pos += input_len;
}

void eb_remove_char(EditorBuffer *buffer) {
  uint64 len = strlen(buffer->text);
  if (len > 0 && buffer->cursor_pos > 0) {
    uint64 cur_position = buffer->cursor_pos - 1;
    memcpy(buffer->text + cur_position, buffer->text + cur_position + 1,
           len - cur_position);
    buffer->cursor_pos--;
  }
}

void eb_clear(EditorBuffer *buffer) {
  buffer->text[0] = '\0';
  buffer->cursor_pos = 0;
}

uint64 find_bol(char *text, uint64 from_pos) {
  uint64 len = strlen(text);
  for (uint64 i = from_pos; i > 0; --i) {
    if (text[i] == '\n') {
      return (i + 1) > len ? i : i + 1;
    }
  }
  return 0;
}

uint64 find_eol(char *text, uint64 from_pos) {
  uint64 len = strlen(text);
  for (uint64 i = from_pos; i < len; ++i) {
    if (text[i] == '\n') {
      return i > 0 ? i - 1 : 0;
    }
  }
  return len;
}

extern UPDATE_AND_RENDER(UpdateAndRender) {
  assert(sizeof(State) <= memory->permanentStorageSize);

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
    state->mainBuffer.text =
        (char *)pushSize(&state->arena, 1000, DEFAULT_ALIGMENT);
    state->mainBuffer.text[0] = '\0';

    // TODO: how are we going to handle this?
    state->exBuffer.text =
        (char *)pushSize(&state->arena, 1000, DEFAULT_ALIGMENT);
    state->exBuffer.text[0] = '\0';

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

  EditorBuffer *mainBuffer = &state->mainBuffer;
  EditorBuffer *exBuffer = &state->exBuffer;

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

          if (strcmp(exBuffer->text, "quit") == 0 ||
              strcmp(exBuffer->text, "q") == 0) {
            return 1;
          } else if (strcmp(exBuffer->text, "clear") == 0) {
            eb_clear(mainBuffer);
          }
        } else if (event.key.keysym.scancode == SDL_SCANCODE_BACKSPACE) {
          eb_remove_char(exBuffer);
        } else if (event.key.keysym.scancode == SDL_SCANCODE_ESCAPE) {
          state->mode = AppMode_normal;
          eb_clear(exBuffer);
        }
        break;
      case AppMode_insert:
        if (event.key.keysym.scancode == SDL_SCANCODE_RETURN) {
          size_t len = strlen(mainBuffer->text);
          mainBuffer->text[len] = '\n';
          mainBuffer->text[len + 1] = '\0';
          mainBuffer->cursor_pos++;
        } else if (event.key.keysym.scancode == SDL_SCANCODE_BACKSPACE) {
          eb_remove_char(mainBuffer);
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
          switch (event.text.text[x]) {
          case ':': {
            state->mode = AppMode_ex;
            eb_clear(exBuffer);
          } break;
          case 'i': {
            state->mode = AppMode_insert;
          } break;
          case 'h': {
            if (mainBuffer->cursor_pos > 0) {
              mainBuffer->cursor_pos--;
            }
          } break;
          case 'l': {
            if (mainBuffer->cursor_pos < (strlen(mainBuffer->text) - 1)) {
              mainBuffer->cursor_pos++;
            }
          } break;
          case 'H': {
            mainBuffer->cursor_pos =
                find_bol(mainBuffer->text, mainBuffer->cursor_pos);
          } break;
          case 'L': {
            mainBuffer->cursor_pos =
                find_eol(mainBuffer->text, mainBuffer->cursor_pos);
          } break;
          }
        }
        break;
      case AppMode_ex:
        eb_insert_text(&transientState->arena, exBuffer, event.text.text);
        break;
      case AppMode_insert: {
        eb_insert_text(&transientState->arena, mainBuffer, event.text.text);
      } break;
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

    int x = margin_x;
    int y = buffer->height * 0.01;

    bool32 cursor_rendered = false;

    for (uint64 i = 0; i < strlen(mainBuffer->text); ++i) {
      uint32 ch = mainBuffer->text[i];

      if (ch == '\n') {
        y += font_h;
        x = margin_x;
        continue;
      }

      if (i == mainBuffer->cursor_pos) {
        SDL_Rect dest;
        dest.x = x;
        dest.y = y;
        dest.w = font_w;
        dest.h = font_h;
        render_cursor(buffer->renderer, dest, state->mode != AppMode_ex);
        cursor_rendered = true;
      }

      if (ch != '\n') {
        SDL_Surface *surface =
            TTF_cpointer(TTF_RenderGlyph_Solid(state->font, ch, sdlFontColor));
        SurfaceRenderer sr = SR_create(buffer->renderer, surface);
        int w = sr.surface->w;
        SR_render_fullsize_and_destroy(&sr, x, y);
        x += w;
      }
    }

    if (!cursor_rendered) {
      SDL_Rect dest;
      dest.x = x;
      dest.y = y;
      dest.w = font_w;
      dest.h = font_h;

      render_cursor(buffer->renderer, dest, state->mode != AppMode_ex);
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
    SR_render_fullsize_and_destroy(
        &sr, buffer->width - text_surface->w - buffer->width * 0.01,
        buffer->height - text_surface->h - 0.03 * buffer->height);
  }

  if (state->mode == AppMode_ex) {
    char *text = (char *)pushSize(&transientState->arena,
                                  strlen(exBuffer->text) + 1, DEFAULT_ALIGMENT);
    sprintf(text, ":%s", exBuffer->text);

    SDL_Surface *text_surface =
        TTF_cpointer(TTF_RenderText_Solid(state->font, text, sdlFontColor));

    SurfaceRenderer sr = SR_create(buffer->renderer, text_surface);
    SR_render_fullsize_and_destroy(&sr, 0.01 * buffer->width,
                                   buffer->height - text_surface->h -
                                       0.01 * buffer->height);
  }

#if DEBUG_WINDOW
  {
    char *text =
        (char *)pushSize(&transientState->arena, 1000, DEFAULT_ALIGMENT);
    SDL_Color color = {UNHEX(debugFontColor)};
    sprintf(text, "Cursor position: %lu\nText length: %lu",
            mainBuffer->cursor_pos, strlen(mainBuffer->text));

    const int margin_x = buffer->width * 0.01;
    const int margin_y = buffer->height * 0.01;

    int x = margin_x;
    int y = margin_y;

    for (uint64 i = 0; i < strlen(text); ++i) {
      uint32 ch = text[i];

      if (ch == '\n') {
        y += font_h;
        x = margin_x;
        continue;
      }

      SDL_Surface *surface =
          TTF_cpointer(TTF_RenderGlyph_Solid(state->font, ch, color));
      SurfaceRenderer sr = SR_create(buffer->debugRenderer, surface);
      int w = sr.surface->w;
      SR_render_fullsize_and_destroy(&sr, x, y);
      x += w;
    }
  }
#endif

  return 0;
}
