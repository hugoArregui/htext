#include "htext_app.h"
#include "htext_la.h"
#include "htext_sdl.h"
#include <SDL2/SDL_blendmode.h>
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_keyboard.h>
#include <SDL2/SDL_keycode.h>
#include <SDL2/SDL_main.h>
#include <SDL2/SDL_rect.h>
#include <SDL2/SDL_render.h>
#include <SDL2/SDL_scancode.h>
#include <SDL2/SDL_ttf.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>

// TODO load and write file
// TODO better debug logging
// TODO playback

const char *normalModeName = "NORMAL";
const char *exModeName = "EX";
const char *insertModeName = "INSERT";

const uint32 backgroundColor = 0x00000000;
const uint32 fontColor = 0xFFFFFF00;
const uint32 cursorColor = fontColor | 0x9F;

// NOTE: I use this commented code to set a breakpoint when an assert is
// triggered
/* #define assert(cond) my_assert(cond, __FILE__, __LINE__) */
/* void my_assert(bool32 cond, char *file, int linenum) { */
/*   if (!cond) { */
/*     printf("%s:%d\n", file, linenum); */
/*     exit(1); */
/*   } */
/* } */

#define ASSERT_LINE_INTEGRITY 1

#if ASSERT_LINE_INTEGRITY
#define assert_line_integrity(state)                                           \
  _assert_line_integrity(state, __FILE__, __LINE__)

void assert_main_buffer_integrity(EditorBuffer *mainBuffer, char *file,
                                  int linenum) {
  Line *prev_line = NULL;
  for (Line *line = mainBuffer->line; line != NULL; line = line->next) {
    if (line->prev == line) {
      printf("%s:%d\n", file, linenum);
      assert(line->prev != line);
    }
    if (line->next == line) {
      printf("%s:%d\n", file, linenum);
      assert(line->next != line);
    }
    if (line->prev != prev_line) {
      printf("%s:%d\n", file, linenum);
      assert(line->prev == prev_line);
    }
    if (line->max_size == 0) {
      printf("%s:%d\n", file, linenum);
      assert(line->max_size > 0);
    }

    for (uint32 i = 0; i < line->size; ++i) {
      if (line->text[i] < 32) {
        printf("%s:%d\n", file, linenum);
        assert(line->text[i] > 32);
      }
    }
    prev_line = line;
  }

  for (Line *line = mainBuffer->deleted_line; line != NULL; line = line->next) {
    if (line->max_size == 0) {
      printf("%s:%d\n", file, linenum);
      assert(line->max_size > 0);
    }
  }
}

void _assert_line_integrity(State *state, char *file, int linenum) {
  assert(state->exBuffer.line->next == NULL);
  assert(state->exBuffer.deleted_line == NULL);

  assert_main_buffer_integrity(&state->mainBuffer, file, linenum);
}
#else
void _assert_line_integrity(State *state, char *file, int linenum);
#endif

void render_cursor(SDL_Renderer *renderer, SDL_Rect dest, bool32 fill) {
  SDL_ccode(SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND));
  SDL_ccode(SDL_SetRenderDrawColor(renderer, UNHEX(cursorColor)));
  if (fill) {
    SDL_ccode(SDL_RenderFillRect(renderer, &dest));
  } else {
    SDL_ccode(SDL_RenderDrawRect(renderer, &dest));
  }
}

bool32 line_eq(Line *line, char *str) {
  return strncmp(line->text, str, line->size) == 0;
}

void line_insert_next(Line *line, Line *next_line) {
  assert(line != NULL);
  assert(next_line != NULL);
  assert(line != next_line);

  next_line->next = line->next;
  next_line->prev = line;
  if (next_line->next != NULL) {
    next_line->next->prev = next_line;
  }
  line->next = next_line;
}

void eb_render(State *state, SDL_Renderer *renderer, EditorBuffer *mainBuffer,
               int x_start, int y_start, bool32 cursor_active) {
  int x = x_start;
  int y = y_start;

  const int cursor_w = state->font_h / 2;
  bool32 cursor_rendered = false;

  SDL_Color sdlFontColor = {UNHEX(fontColor)};
  for (Line *line = mainBuffer->line; line != NULL; line = line->next) {
    for (uint64 i = 0; i < line->size; ++i) {
      uint32 ch = line->text[i];
      if (line == mainBuffer->cursor_line && i == mainBuffer->cursor_pos) {
        SDL_Rect dest;
        dest.x = x;
        dest.y = y;
        dest.w = cursor_w;
        dest.h = state->font_h;
        render_cursor(renderer, dest, cursor_active);
        cursor_rendered = true;
      }

      SDL_Surface *surface =
          TTF_cpointer(TTF_RenderGlyph_Solid(state->font, ch, sdlFontColor));

      SurfaceRenderer sr = SR_create(renderer, surface);
      int w = sr.surface->w;
      SR_render_fullsize_and_destroy(&sr, x, y);
      x += w;
    }

    if (!cursor_rendered && line == mainBuffer->cursor_line) {
      SDL_Rect dest;
      dest.x = x;
      dest.y = y;
      dest.w = cursor_w;
      dest.h = state->font_h;

      render_cursor(renderer, dest, cursor_active);
    }

    y += state->font_h;
    x = x_start;
  }
}

EditorBuffer eb_create(MemoryArena *arena) {
  Line *line = pushStruct(arena, Line, DEFAULT_ALIGMENT);
  line->max_size = 200;
  line->size = 0;
  line->text = (char *)pushSize(arena, line->max_size, DEFAULT_ALIGMENT);
  line->prev = NULL;
  line->next = NULL;
  return (EditorBuffer){.line = line, .cursor_line = line, .cursor_pos = 0};
}

void eb_insert_text(EditorBuffer *buffer, char *text) {
  uint64 text_len = strlen(text);

  Line *line = buffer->cursor_line;

  assert(line->max_size >= (line->size + text_len));
  memcpy(line->text + buffer->cursor_pos + text_len,
         line->text + buffer->cursor_pos, line->size - buffer->cursor_pos);
  memcpy(line->text + buffer->cursor_pos, text, text_len);
  line->size += text_len;
  buffer->cursor_pos += text_len;
}

void eb_new_line(MemoryArena *arena, EditorBuffer *buffer) {
  Line *new_line;
  if (buffer->deleted_line != NULL) {
    new_line = buffer->deleted_line;
    buffer->deleted_line = buffer->deleted_line->next;
  } else {
    new_line = pushStruct(arena, Line, DEFAULT_ALIGMENT);
    new_line->max_size = 200;
    new_line->text =
        (char *)pushSize(arena, new_line->max_size, DEFAULT_ALIGMENT);
  }

  if (buffer->cursor_pos < buffer->cursor_line->size) {
    new_line->size = buffer->cursor_line->size - buffer->cursor_pos;
    memcpy(new_line->text, buffer->line->text + buffer->cursor_pos,
           new_line->size);
    buffer->cursor_line->size = buffer->cursor_pos;
  } else {
    new_line->size = 0;
  }

  line_insert_next(buffer->cursor_line, new_line);
  buffer->cursor_line = new_line;
  buffer->cursor_pos = 0;
}

void eb_remove_char(EditorBuffer *buffer) {
  if (buffer->cursor_pos == 0) {
    if (buffer->cursor_line->prev != NULL) {
      Line *line_to_remove = buffer->cursor_line;

      // we remove line_to_remove
      buffer->cursor_line = line_to_remove->prev;
      buffer->cursor_line->next = line_to_remove->next;
      if (buffer->cursor_line->next != NULL) {
        buffer->cursor_line->next->prev = buffer->cursor_line;
      }

      buffer->cursor_pos = buffer->cursor_line->size;
      if (line_to_remove->size > 0) {
        // we need to join line_to_remove with prev cursor line
        assert(buffer->cursor_line->max_size >=
               (buffer->cursor_line->size + line_to_remove->size));
        memcpy(buffer->cursor_line->text + buffer->cursor_line->size,
               line_to_remove->text, line_to_remove->size);
        buffer->cursor_line->size += line_to_remove->size;
      }

      if (buffer->deleted_line != NULL) {
        line_to_remove->next = NULL;
        line_to_remove->prev = NULL;
        line_insert_next(line_to_remove, buffer->deleted_line);
      }
      buffer->deleted_line = line_to_remove;
    }
  } else {
    assert(buffer->cursor_pos <= buffer->cursor_line->size);
    memcpy(buffer->cursor_line->text + buffer->cursor_pos,
           buffer->cursor_line->text + buffer->cursor_pos + 1,
           buffer->cursor_line->size - buffer->cursor_pos);
    buffer->cursor_line->size--;
    buffer->cursor_pos--;
  }
}

extern UPDATE_AND_RENDER(UpdateAndRender) {
  assert(sizeof(State) <= memory->permanentStorageSize);

  SDL_Color sdlModeColor = {UNHEX(0xFF000000)};

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

    state->font_h = TTF_FontHeight(state->font);

    initializeArena(&state->arena, memory->permanentStorageSize - sizeof(State),
                    (uint8 *)memory->permanentStorage + sizeof(State));

    state->mainBuffer = eb_create(&state->arena);
    state->exBuffer = eb_create(&state->arena);

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

          if (line_eq(exBuffer->line, "quit") || line_eq(exBuffer->line, "q")) {
            return 1;
          }
        } else if (event.key.keysym.scancode == SDL_SCANCODE_BACKSPACE) {
          eb_remove_char(exBuffer);
          assert_line_integrity(state);
        } else if (event.key.keysym.scancode == SDL_SCANCODE_ESCAPE) {
          state->mode = AppMode_normal;
          exBuffer->line->size = 0;
          exBuffer->cursor_pos = 0;
        }
        break;
      case AppMode_insert:
        if (event.key.keysym.scancode == SDL_SCANCODE_RETURN) {
          eb_new_line(&state->arena, mainBuffer);
          assert_line_integrity(state);
        } else if (event.key.keysym.scancode == SDL_SCANCODE_BACKSPACE) {
          eb_remove_char(mainBuffer);
          assert_line_integrity(state);
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
            exBuffer->line->size = 0;
            exBuffer->cursor_pos = 0;
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
            if (mainBuffer->cursor_pos < mainBuffer->cursor_line->size) {
              mainBuffer->cursor_pos++;
            }
          } break;
          case 'k': {
            if (mainBuffer->cursor_line->prev != NULL) {
              mainBuffer->cursor_line = mainBuffer->cursor_line->prev;
              if (mainBuffer->cursor_pos > mainBuffer->cursor_line->size) {
                mainBuffer->cursor_pos = mainBuffer->cursor_line->size;
              }
            }
          } break;
          case 'j': {
            if (mainBuffer->cursor_line->next != NULL) {
              mainBuffer->cursor_line = mainBuffer->cursor_line->next;
              if (mainBuffer->cursor_pos > mainBuffer->cursor_line->size) {
                mainBuffer->cursor_pos = mainBuffer->cursor_line->size;
              }
            }
          } break;
          case 'H': {
            mainBuffer->cursor_pos = 0;
          } break;
          case 'L': {
            mainBuffer->cursor_pos = mainBuffer->cursor_line->size;
          } break;
          case 'A': {
            mainBuffer->cursor_pos = mainBuffer->cursor_line->size;
            state->mode = AppMode_insert;
          } break;
          }
        }
        break;
      case AppMode_ex:
        eb_insert_text(exBuffer, event.text.text);
        break;
      case AppMode_insert: {
        eb_insert_text(mainBuffer, event.text.text);
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
  // render main buffer
  eb_render(state, buffer->renderer, mainBuffer, buffer->width * 0.01,
            buffer->height * 0.01, state->mode != AppMode_ex);

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
    assert(exBuffer->line->next == NULL);

    int x = 0.01 * buffer->width;
    int y = buffer->height - state->font_h - 0.01 * buffer->height;

    // first render ':'
    SDL_Color sdlFontColor = {UNHEX(fontColor)};
    SDL_Surface *surface =
        TTF_cpointer(TTF_RenderGlyph_Solid(state->font, ':', sdlFontColor));
    SurfaceRenderer sr = SR_create(buffer->renderer, surface);
    SR_render_fullsize_and_destroy(&sr, x, y);
    x += sr.surface->w;

    eb_render(state, buffer->renderer, exBuffer, x, y,
              state->mode == AppMode_ex);
  }

#if DEBUG_WINDOW
  {
    char *text =
        (char *)pushSize(&transientState->arena, 1000, DEFAULT_ALIGMENT);
    SDL_Color color = {UNHEX(debugFontColor)};
    sprintf(text, "Cursor position: %d\n", mainBuffer->cursor_pos);

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
