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

void assert_main_frame_integrity(MainFrame *main_frame, char *file,
                                 int linenum) {
  Line *prev_line = NULL;

  uint32 total_lines = 0;
  // TODO check cursor line_num integrity
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

Glyph *render_char(State *state, SDL_Renderer *renderer, int ch, int x, int y) {
  Glyph *glyph = state->glyph_cache + (ch - ASCII_LOW);
  assert(glyph != NULL);

  SDL_Rect dest;
  dest.x = x;
  dest.y = y;
  dest.w = glyph->w;
  dest.h = glyph->h;
  SDL_ccode(SDL_RenderCopy(renderer, glyph->texture, NULL, &dest));
  return glyph;
}

void render_text(State *state, SDL_Renderer *renderer, const char *text,
                 int len, int x, int y) {
  for (int i = 0; i < len; ++i) {
    char ch = text[i];
    Glyph *glyph = state->glyph_cache + (ch - ASCII_LOW);
    assert(glyph != NULL);

    SDL_Rect dest;
    dest.x = x;
    dest.y = y;
    dest.w = glyph->w;
    dest.h = glyph->h;
    SDL_ccode(SDL_RenderCopy(renderer, glyph->texture, NULL, &dest));
    x += glyph->w;
  }
}

void render_lines(State *state, SDL_Renderer *renderer, Line *start_line,
                  Line *end_line, Cursor cursor, int x_start, int y_start,
                  bool32 is_cursor_active) {
  int x = x_start;
  int y = y_start;

  const int cursor_w = state->font_h / 2;
  bool32 cursor_rendered = false;

  for (Line *line = start_line; line != end_line; line = line->next) {
    for (uint64 i = 0; i < line->size; ++i) {
      uint32 ch = line->text[i];
      if (line == cursor.line && i == cursor.column) {
        SDL_Rect dest;
        dest.x = x;
        dest.y = y;
        dest.w = cursor_w;
        dest.h = state->font_h;
        render_cursor(renderer, dest, is_cursor_active);
        cursor_rendered = true;
      }

      x += render_char(state, renderer, ch, x, y)->w;
    }

    if (!cursor_rendered && line == cursor.line) {
      SDL_Rect dest;
      dest.x = x;
      dest.y = y;
      dest.w = cursor_w;
      dest.h = state->font_h;

      render_cursor(renderer, dest, is_cursor_active);
    }

    y += state->font_h;
    x = x_start;
  }
}

Line *line_create(MemoryArena *arena) {
  Line *line = pushStruct(arena, Line, DEFAULT_ALIGMENT);
  line->max_size = 200;
  line->size = 0;
  line->text = (char *)pushSize(arena, line->max_size, DEFAULT_ALIGMENT);
  line->prev = NULL;
  line->next = NULL;
  return line;
}

void frame_insert_text(Cursor *cursor, char *text, uint32 text_size) {
  Line *line = cursor->line;

  assert(line->max_size >= (line->size + text_size));
  memcpy(line->text + cursor->column + text_size, line->text + cursor->column,
         line->size - cursor->column);
  memcpy(line->text + cursor->column, text, text_size);
  line->size += text_size;
  cursor->column += text_size;
}

// TODO: reindex using the previous index
void main_frame_reindex(MainFrame *frame) {
  uint32 i = 0;
  for (Line *line = frame->line; line != NULL; line = line->next) {
    frame->index[i] = line;
    ++i;
    assert(i < INDEX_SIZE);
  }
  assert(i == frame->line_count);
}

void main_frame_remove_char(MainFrame *frame) {
  if (frame->cursor.column == 0) {
    if (frame->cursor.line->prev != NULL) {
      Line *line_to_remove = frame->cursor.line;

      // we remove line_to_remove
      frame->cursor.line = line_to_remove->prev;
      frame->cursor.line->next = line_to_remove->next;
      if (frame->cursor.line->next != NULL) {
        frame->cursor.line->next->prev = frame->cursor.line;
      }
      frame->line_count--;

      frame->cursor.column = frame->cursor.line->size;
      if (line_to_remove->size > 0) {
        // we need to join line_to_remove with prev cursor line
        assert(frame->cursor.line->max_size >=
               (frame->cursor.line->size + line_to_remove->size));
        memcpy(frame->cursor.line->text + frame->cursor.line->size,
               line_to_remove->text, line_to_remove->size);
        frame->cursor.line->size += line_to_remove->size;
      }

      if (frame->deleted_line != NULL) {
        line_to_remove->next = NULL;
        line_to_remove->prev = NULL;
        line_insert_next(line_to_remove, frame->deleted_line);
      }
      frame->deleted_line = line_to_remove;
      main_frame_reindex(frame);
    }
  } else {
    assert(frame->cursor.column <= frame->cursor.line->size);
    memcpy(frame->cursor.line->text + frame->cursor.column,
           frame->cursor.line->text + frame->cursor.column + 1,
           frame->cursor.line->size - frame->cursor.column);
    frame->cursor.line->size--;
    frame->cursor.column--;
  }
}

void main_frame_insert_new_line(MemoryArena *arena, MainFrame *frame) {
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

  if (frame->cursor.column < frame->cursor.line->size) {
    new_line->size = frame->cursor.line->size - frame->cursor.column;
    memcpy(new_line->text, frame->cursor.line->text + frame->cursor.column,
           new_line->size);
    frame->cursor.line->size = frame->cursor.column;
  } else {
    new_line->size = 0;
  }

  line_insert_next(frame->cursor.line, new_line);
  frame->cursor.line = new_line;
  frame->cursor.line_num++;
  frame->cursor.column = 0;
  frame->line_count++;
  main_frame_reindex(frame);
}

void ex_frame_remove_char(ExFrame *frame) {
  assert(frame->cursor.column <= frame->cursor.line->size);
  memcpy(frame->cursor.line->text + frame->cursor.column,
         frame->cursor.line->text + frame->cursor.column + 1,
         frame->cursor.line->size - frame->cursor.column);
  frame->cursor.line->size--;
  frame->cursor.column--;
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
      main_frame_insert_new_line(&state->arena, &state->main_frame);
    } else {
      frame_insert_text(&state->main_frame.cursor, &c, 1);
    }
    read_size = fread(&c, sizeof(char), 1, f);
  }

  state->main_frame.cursor.line = state->main_frame.line;
  state->main_frame.cursor.line_num = 0;
  state->main_frame.cursor.column = 0;
  main_frame_reindex(&state->main_frame);
  return 0;
}

extern UPDATE_AND_RENDER(UpdateAndRender) {
  assert(sizeof(State) <= memory->permanentStorageSize);

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
    // TODO: this should be cleared at exit, but do we care?
    state->font = TTF_cpointer(
        TTF_OpenFont("/usr/share/fonts/TTF/IosevkaNerdFont-Regular.ttf", 20));

    state->font_h = TTF_FontHeight(state->font);

    SDL_Color sdlFontColor = {UNHEX(fontColor)};
    for (int i = ASCII_LOW; i < ASCII_HIGH; ++i) {
      SDL_Surface *surface =
          TTF_cpointer(TTF_RenderGlyph_Solid(state->font, i, sdlFontColor));
      SDL_Texture *texture =
          SDL_cpointer(SDL_CreateTextureFromSurface(buffer->renderer, surface));
      state->glyph_cache[i - ASCII_LOW] =
          (Glyph){.texture = texture, .w = surface->w, .h = surface->h};
      SDL_FreeSurface(surface);
    }

    initializeArena(&state->arena, memory->permanentStorageSize - sizeof(State),
                    (uint8 *)memory->permanentStorage + sizeof(State));

    {
      Line *line = line_create(&state->arena);
      state->main_frame =
          (MainFrame){.line = line,
                      .cursor = (Cursor){.line = line, .column = 0},
                      .line_count = 1};
    }
    {
      Line *line = line_create(&state->arena);
      state->ex_frame = (ExFrame){
          .line = line,
          .cursor = (Cursor){.line = line, .column = 0},
      };
    }

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

  MainFrame *main_frame = &state->main_frame;
  ExFrame *ex_frame = &state->ex_frame;

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
          ex_frame_remove_char(ex_frame);
          assert_line_integrity(state);
        } else if (event.key.keysym.scancode == SDL_SCANCODE_ESCAPE) {
          state->mode = AppMode_normal;
          ex_frame->line->size = 0;
          ex_frame->cursor.column = 0;
        }
        break;
      case AppMode_insert:
        if (event.key.keysym.scancode == SDL_SCANCODE_RETURN) {
          main_frame_insert_new_line(&state->arena, main_frame);
          assert_line_integrity(state);
        } else if (event.key.keysym.scancode == SDL_SCANCODE_BACKSPACE) {
          main_frame_remove_char(main_frame);
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
            ex_frame->cursor.column = 0;
          } break;
          case 'i': {
            state->mode = AppMode_insert;
          } break;
          case 'h': {
            if (main_frame->cursor.column > 0) {
              main_frame->cursor.column--;
            }
          } break;
          case 'l': {
            if (main_frame->cursor.column < main_frame->cursor.line->size) {
              main_frame->cursor.column++;
            }
          } break;
          case 'k': {
            if (main_frame->cursor.line->prev != NULL) {
              main_frame->cursor.line = main_frame->cursor.line->prev;
              main_frame->cursor.line_num--;
              if (main_frame->cursor.column > main_frame->cursor.line->size) {
                main_frame->cursor.column = main_frame->cursor.line->size;
              }
            }
          } break;
          case 'j': {
            if (main_frame->cursor.line->next != NULL) {
              main_frame->cursor.line = main_frame->cursor.line->next;
              main_frame->cursor.line_num++;
              if (main_frame->cursor.column > main_frame->cursor.line->size) {
                main_frame->cursor.column = main_frame->cursor.line->size;
              }
            }
          } break;
          case 'H': {
            main_frame->cursor.column = 0;
          } break;
          case 'L': {
            main_frame->cursor.column = main_frame->cursor.line->size;
          } break;
          case 'A': {
            main_frame->cursor.column = main_frame->cursor.line->size;
            state->mode = AppMode_insert;
          } break;
          }
        }
        break;
      case AppMode_ex: {
        uint32 text_size = strlen(event.text.text);
        frame_insert_text(&ex_frame->cursor, event.text.text, text_size);
      } break;
      case AppMode_insert: {
        uint32 text_size = strlen(event.text.text);
        frame_insert_text(&main_frame->cursor, event.text.text, text_size);
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

  {
    int h = ex_frame_start_y - main_frame_start_y;
    uint32 lines_to_render = h / state->font_h;
    Line *start_line = main_frame->line;
    assert(start_line != NULL);
    Line *end_line = NULL;

    if (main_frame->line_count > lines_to_render) {
      int start_line_num = main_frame->cursor.line_num - (lines_to_render / 2);
      if (start_line_num < 0) {
        start_line_num = 0;
      }
      uint32 end_line_num = start_line_num + lines_to_render;
      if (end_line_num > main_frame->line_count) {
        end_line_num = main_frame->line_count;
      }
      start_line = main_frame->index[start_line_num];
      end_line = main_frame->index[end_line_num];
      assert(start_line != NULL);
    }

    render_lines(state, buffer->renderer, start_line, end_line,
                 main_frame->cursor, buffer->width * 0.01, main_frame_start_y,
                 state->mode != AppMode_ex);
  }

  // render mode name
  {
    const char *modeName = state_get_mode_name(state);
    render_text(state, buffer->renderer, modeName, strlen(modeName),
                buffer->width - buffer->width * 0.05,
                buffer->height - state->font_h - 0.03 * buffer->height);
  }

  if (state->mode == AppMode_ex) {
    assert(ex_frame->line->next == NULL);

    int x = 0.01 * buffer->width;

    // first render ':'
    x += render_char(state, buffer->renderer, ':', x, ex_frame_start_y)->w;

    render_lines(state, buffer->renderer, ex_frame->line, NULL,
                 ex_frame->cursor, x, ex_frame_start_y,
                 state->mode == AppMode_ex);
  }

#if DEBUG_WINDOW
  {
    char *text =
        (char *)pushSize(&transientState->arena, 1000, DEFAULT_ALIGMENT);
    SDL_Color color = {UNHEX(debugFontColor)};
    sprintf(text, "Cursor position: %d\n", main_frame->cursor.column);

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

      x += render_char(state, renderer, ch, x, y)->w;
    }
  }
#endif

  return 0;
}
