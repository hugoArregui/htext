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

void assert_main_frame_integrity(Frame *main_frame, char *file, int linenum) {
  Line *prev_line = NULL;

  uint32 total_lines = 0;
  for (Line *line = main_frame->line; line != NULL; line = line->next) {
    total_lines++;
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

  assert(total_lines == main_frame->line_count);

  for (Line *line = main_frame->deleted_line; line != NULL; line = line->next) {
    if (line->max_size == 0) {
      printf("%s:%d\n", file, linenum);
      assert(line->max_size > 0);
    }
  }
}

void _assert_line_integrity(State *state, char *file, int linenum) {
  assert(state->ex_frame.line->next == NULL);
  assert(state->ex_frame.deleted_line == NULL);

  assert_main_frame_integrity(&state->main_frame, file, linenum);
}
#else
void _assert_line_integrity(State *state, char *file, int linenum);
#endif

const char *state_get_mode_name(State *state) {
  switch (state->mode) {
  case AppMode_normal:
    return normalModeName;
  case AppMode_ex:
    return exModeName;
  case AppMode_insert:
    return insertModeName;
  default: {
    assert(false);
    break;
  }
  }
}

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

void frame_render(State *state, SDL_Renderer *renderer, Frame *frame,
                  int x_start, int y_start, int h, bool32 cursor_active) {
  int x = x_start;
  int y = y_start;

  const int cursor_w = state->font_h / 2;
  bool32 cursor_rendered = false;

  SDL_Color sdlFontColor = {UNHEX(fontColor)};

  uint32 lines_to_render = h / state->font_h;
  Line *start_line = frame->line;
  Line *end_line = NULL;

  if (lines_to_render < frame->line_count) {
    end_line = start_line;
    for (uint32 i = 0; i < lines_to_render; ++i) {
      end_line = end_line->next;
    }
  }

  for (Line *line = start_line; line != end_line; line = line->next) {
    for (uint64 i = 0; i < line->size; ++i) {
      uint32 ch = line->text[i];
      if (line == frame->cursor_line && i == frame->cursor_pos) {
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

    if (!cursor_rendered && line == frame->cursor_line) {
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

Frame frame_create(MemoryArena *arena) {
  Line *line = pushStruct(arena, Line, DEFAULT_ALIGMENT);
  line->max_size = 200;
  line->size = 0;
  line->text = (char *)pushSize(arena, line->max_size, DEFAULT_ALIGMENT);
  line->prev = NULL;
  line->next = NULL;
  return (Frame){
      .line = line, .cursor_line = line, .cursor_pos = 0, .line_count = 1};
}

void frame_insert_text(Frame *frame, char *text, uint32 text_size) {
  Line *line = frame->cursor_line;

  assert(line->max_size >= (line->size + text_size));
  memcpy(line->text + frame->cursor_pos + text_size,
         line->text + frame->cursor_pos, line->size - frame->cursor_pos);
  memcpy(line->text + frame->cursor_pos, text, text_size);
  line->size += text_size;
  frame->cursor_pos += text_size;
}

void frame_insert_new_line(MemoryArena *arena, Frame *frame) {
  Line *new_line;
  if (frame->deleted_line != NULL) {
    new_line = frame->deleted_line;
    frame->deleted_line = frame->deleted_line->next;
  } else {
    new_line = pushStruct(arena, Line, DEFAULT_ALIGMENT);
    new_line->max_size = 200;
    new_line->text =
        (char *)pushSize(arena, new_line->max_size, DEFAULT_ALIGMENT);
  }

  if (frame->cursor_pos < frame->cursor_line->size) {
    new_line->size = frame->cursor_line->size - frame->cursor_pos;
    memcpy(new_line->text, frame->cursor_line->text + frame->cursor_pos,
           new_line->size);
    frame->cursor_line->size = frame->cursor_pos;
  } else {
    new_line->size = 0;
  }

  line_insert_next(frame->cursor_line, new_line);
  frame->cursor_line = new_line;
  frame->cursor_pos = 0;
  frame->line_count++;
}

void frame_remove_char(Frame *frame) {
  if (frame->cursor_pos == 0) {
    if (frame->cursor_line->prev != NULL) {
      Line *line_to_remove = frame->cursor_line;

      // we remove line_to_remove
      frame->cursor_line = line_to_remove->prev;
      frame->cursor_line->next = line_to_remove->next;
      if (frame->cursor_line->next != NULL) {
        frame->cursor_line->next->prev = frame->cursor_line;
      }
      frame->line_count--;

      frame->cursor_pos = frame->cursor_line->size;
      if (line_to_remove->size > 0) {
        // we need to join line_to_remove with prev cursor line
        assert(frame->cursor_line->max_size >=
               (frame->cursor_line->size + line_to_remove->size));
        memcpy(frame->cursor_line->text + frame->cursor_line->size,
               line_to_remove->text, line_to_remove->size);
        frame->cursor_line->size += line_to_remove->size;
      }

      if (frame->deleted_line != NULL) {
        line_to_remove->next = NULL;
        line_to_remove->prev = NULL;
        line_insert_next(line_to_remove, frame->deleted_line);
      }
      frame->deleted_line = line_to_remove;
    }
  } else {
    assert(frame->cursor_pos <= frame->cursor_line->size);
    memcpy(frame->cursor_line->text + frame->cursor_pos,
           frame->cursor_line->text + frame->cursor_pos + 1,
           frame->cursor_line->size - frame->cursor_pos);
    frame->cursor_line->size--;
    frame->cursor_pos--;
  }
}

#if DEBUG_PLAYBACK == PLAYBACK_RECORDING
int poll_event(Input *input, SDL_Event *event) {
  int pending_event = SDL_PollEvent(event);
  if (pending_event) {
    // NOTE: this is insane
    assert(fwrite(event, sizeof(SDL_Event), 1, input->playbackFile) == 1);
    assert(fflush(input->playbackFile) == 0);
  }
  return pending_event;
}
#elif DEBUG_PLAYBACK == PLAYBACK_PLAYING
int poll_event(Input *input, SDL_Event *event) {
  int read_size = fread(event, sizeof(SDL_Event), 1, input->playbackFile);
  if (read_size == 1) {
    return 1;
  } else {
    return SDL_PollEvent(event);
  }
}
#else
int poll_event(Input *input, SDL_Event *event) {
  (void)(input); // avoid unused parameter warning
  return SDL_PollEvent(event);
}
#endif

int load_file(State *state) {
  FILE *f = fopen("src/htext_app.c", "r");
  if (!f) {
    return -1;
  }

  // TODO clear main_frame first
  char c;
  int read_size = fread(&c, sizeof(char), 1, f);
  while (read_size > 0) {
    if (c == '\n') {
      frame_insert_new_line(&state->arena, &state->main_frame);
    } else {
      frame_insert_text(&state->main_frame, &c, 1);
    }
    read_size = fread(&c, sizeof(char), 1, f);
  }

  state->main_frame.cursor_line = state->main_frame.line;
  state->main_frame.cursor_pos = 0;
  return 0;
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

    state->main_frame = frame_create(&state->arena);
    state->ex_frame = frame_create(&state->arena);

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

  Frame *main_frame = &state->main_frame;
  Frame *ex_frame = &state->ex_frame;

  SDL_Event event;
  while (poll_event(input, &event)) {
    switch (event.type) {
    case SDL_KEYDOWN:
      switch (state->mode) {
      case AppMode_normal:
        break;
      case AppMode_ex:
        if (event.key.keysym.scancode == SDL_SCANCODE_RETURN) {
          state->mode = AppMode_normal;

          if (line_eq(ex_frame->line, "quit") || line_eq(ex_frame->line, "q")) {
            return 1;
          } else if (line_eq(ex_frame->line, "load")) {
            load_file(state);
          }
        } else if (event.key.keysym.scancode == SDL_SCANCODE_BACKSPACE) {
          frame_remove_char(ex_frame);
          assert_line_integrity(state);
        } else if (event.key.keysym.scancode == SDL_SCANCODE_ESCAPE) {
          state->mode = AppMode_normal;
          ex_frame->line->size = 0;
          ex_frame->cursor_pos = 0;
        }
        break;
      case AppMode_insert:
        if (event.key.keysym.scancode == SDL_SCANCODE_RETURN) {
          frame_insert_new_line(&state->arena, main_frame);
          assert_line_integrity(state);
        } else if (event.key.keysym.scancode == SDL_SCANCODE_BACKSPACE) {
          frame_remove_char(main_frame);
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
            ex_frame->line->size = 0;
            ex_frame->cursor_pos = 0;
          } break;
          case 'i': {
            state->mode = AppMode_insert;
          } break;
          case 'h': {
            if (main_frame->cursor_pos > 0) {
              main_frame->cursor_pos--;
            }
          } break;
          case 'l': {
            if (main_frame->cursor_pos < main_frame->cursor_line->size) {
              main_frame->cursor_pos++;
            }
          } break;
          case 'k': {
            if (main_frame->cursor_line->prev != NULL) {
              main_frame->cursor_line = main_frame->cursor_line->prev;
              if (main_frame->cursor_pos > main_frame->cursor_line->size) {
                main_frame->cursor_pos = main_frame->cursor_line->size;
              }
            }
          } break;
          case 'j': {
            if (main_frame->cursor_line->next != NULL) {
              main_frame->cursor_line = main_frame->cursor_line->next;
              if (main_frame->cursor_pos > main_frame->cursor_line->size) {
                main_frame->cursor_pos = main_frame->cursor_line->size;
              }
            }
          } break;
          case 'H': {
            main_frame->cursor_pos = 0;
          } break;
          case 'L': {
            main_frame->cursor_pos = main_frame->cursor_line->size;
          } break;
          case 'A': {
            main_frame->cursor_pos = main_frame->cursor_line->size;
            state->mode = AppMode_insert;
          } break;
          }
        }
        break;
      case AppMode_ex: {
        uint32 text_size = strlen(event.text.text);
        frame_insert_text(ex_frame, event.text.text, text_size);
      } break;
      case AppMode_insert: {
        uint32 text_size = strlen(event.text.text);
        frame_insert_text(main_frame, event.text.text, text_size);
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
  int main_frame_start_y = buffer->height * 0.01;
  int ex_frame_start_y = buffer->height - state->font_h - 0.01 * buffer->height;
  // render main buffer
  frame_render(state, buffer->renderer, main_frame, buffer->width * 0.01,
               main_frame_start_y, ex_frame_start_y - main_frame_start_y,
               state->mode != AppMode_ex);

  // render mode name
  {
    const char *modeName = state_get_mode_name(state);
    SDL_Surface *text_surface =
        TTF_cpointer(TTF_RenderText_Solid(state->font, modeName, sdlModeColor));

    SurfaceRenderer sr = SR_create(buffer->renderer, text_surface);
    SR_render_fullsize_and_destroy(
        &sr, buffer->width - text_surface->w - buffer->width * 0.01,
        buffer->height - text_surface->h - 0.03 * buffer->height);
  }

  if (state->mode == AppMode_ex) {
    assert(ex_frame->line->next == NULL);

    int x = 0.01 * buffer->width;

    // first render ':'
    SDL_Color sdlFontColor = {UNHEX(fontColor)};
    SDL_Surface *surface =
        TTF_cpointer(TTF_RenderGlyph_Solid(state->font, ':', sdlFontColor));
    SurfaceRenderer sr = SR_create(buffer->renderer, surface);
    SR_render_fullsize_and_destroy(&sr, x, ex_frame_start_y);
    x += sr.surface->w;

    frame_render(state, buffer->renderer, ex_frame, x, ex_frame_start_y,
                 buffer->height - ex_frame_start_y, state->mode == AppMode_ex);
  }

#if DEBUG_WINDOW
  {
    char *text =
        (char *)pushSize(&transientState->arena, 1000, DEFAULT_ALIGMENT);
    SDL_Color color = {UNHEX(debugFontColor)};
    sprintf(text, "Cursor position: %d\n", main_frame->cursor_pos);

    const int margin_x = buffer->width * 0.01;
    const int margin_y = buffer->height * 0.01;

    int x = margin_x;
    int y = margin_y;

    for (uint64 i = 0; i < strlen(text); ++i) {
      uint32 ch = text[i];

      if (ch == '\n') {
        y += state->font_h;
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
