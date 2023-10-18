#include "htext_app.h"
#include "htext_sdl.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>

#define BG_COLOR 0x00000000
#define EDITOR_FONT_COLOR 0x08a00300
#define CURSOR_COLOR 0xFFFFFF9F

#define MODELINE_BG_COLOR 0x595959FF
#define MODELINE_FONT_COLOR 0xFFFFFF00

#define EX_FONT_COLOR 0xFFFFFF00

// TODO rewrite load_file  to avoid moving the cursor and such
// TODO dd is not finished

#define ASSERT_LINE_INTEGRITY 1
#if ASSERT_LINE_INTEGRITY
#define assert_line_integrity(state)                                           \
  _assert_line_integrity(state, __FILE__, __LINE__)

#define my_assert(cond, file, linenum)                                         \
  if (!(cond)) {                                                               \
    printf("%s:%d\n", file, linenum);                                          \
    assert(cond);                                                              \
  }

void assert_editor_frame_integrity(EditorFrame *editor_frame, char *file,
                                   int16_t linenum) {
  Line *prev_line = NULL;

  int16_t line_num = 0;
  for (Line *line = editor_frame->line; line != NULL; line = line->next) {
    my_assert(line->prev != line, file, linenum);
    my_assert(line->next != line, file, linenum);
    my_assert(line->prev == prev_line, file, linenum);
    my_assert(line->max_size > 0, file, linenum);

    if (line == editor_frame->cursor.line) {
      my_assert(line_num == editor_frame->cursor.line_num, file, linenum)
    }

    for (int16_t i = 0; i < line->size; ++i) {
      my_assert(line->text[i] >= 32, file, linenum);
    }
    prev_line = line;
    line_num++;
  }

  assert(line_num == editor_frame->line_count);

  for (Line *line = editor_frame->deleted_line; line != NULL;
       line = line->next) {
    my_assert(line->texture == NULL, file, linenum);
    my_assert(line->max_size > 0, file, linenum);
  }
}

void _assert_line_integrity(State *state, char *file, int16_t linenum) {
  my_assert(state->ex_frame.line->next == NULL, file, linenum);
  assert_editor_frame_integrity(&state->editor_frame, file, linenum);
}
#else
#define assert_line_integrity(state) (void)state
#endif

SDL_Texture *texture_from_text(SDL_Renderer *renderer, TTF_Font *font,
                               char *text, SDL_Color color, int16_t *w) {

  SDL_Surface *surface = TTF_cpointer(TTF_RenderText_Solid(font, text, color));
  if (w != NULL) {
    *w = surface->w;
  }
  SDL_Texture *texture =
      SDL_cpointer(SDL_CreateTextureFromSurface(renderer, surface));
  SDL_FreeSurface(surface);
  return texture;
}

CachedTexture cached_texture_create(SDL_Renderer *renderer, TTF_Font *font,
                                    char *text, SDL_Color color) {
  CachedTexture cached;
  cached.texture = texture_from_text(renderer, font, text, color, &cached.w);
  return cached;
}

void render_cursor(SDL_Renderer *renderer, SDL_Rect *dest, bool fill) {
  SDL_ccode(SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND));
  SDL_ccode(SDL_SetRenderDrawColor(renderer, UNHEX(CURSOR_COLOR)));
  if (fill) {
    SDL_ccode(SDL_RenderFillRect(renderer, dest));
  } else {
    SDL_ccode(SDL_RenderDrawRect(renderer, dest));
  }
}

bool line_eq(Line *line, char *str) {
  return ((int16_t)strlen(str)) == line->size &&
         strncmp(line->text, str, line->size) == 0;
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

void line_invalidate_texture(Line *line) {
  if (line->texture != NULL) {
    SDL_DestroyTexture(line->texture);
    line->texture = NULL;
  }
}

void render_line(State *state, SDL_Renderer *renderer, Line *line,
                 int16_t *cursor_column, int16_t x_start, int16_t y_start,
                 bool is_cursor_active, SDL_Color color) {
  const int16_t cursor_w = state->font_h / 2;

  SDL_Rect dest;
  dest.x = x_start;
  dest.y = y_start;
  dest.h = state->font_h;

  if (cursor_column != NULL) {
    int16_t cursor_x = dest.x;
    for (int16_t i = 0; i < *cursor_column; ++i) {
      cursor_x += state->glyph_width[line->text[i] - ASCII_LOW];
    }

    SDL_Rect cursorDest;
    cursorDest.x = cursor_x;
    cursorDest.y = dest.y;
    cursorDest.h = state->font_h;
    cursorDest.w = cursor_w;
    render_cursor(renderer, &cursorDest, is_cursor_active);
  }

  if (line->size > 0) {
    if (line->texture == NULL) {
      line->text[line->size] = '\0';
      line->texture = texture_from_text(renderer, state->font, line->text,
                                        color, &line->texture_width);
    }

    dest.w = line->texture_width;
    SDL_ccode(SDL_RenderCopy(renderer, line->texture, NULL, &dest));
  }

  dest.y += state->font_h;
  dest.x = x_start;
}

Line *line_create(MemoryArena *arena) {
  Line *line = pushStruct(arena, Line, DEFAULT_ALIGMENT);
  line->max_size = 200;
  line->size = 0;
  line->text = (char *)pushSize(arena, line->max_size, DEFAULT_ALIGMENT);
  line->prev = NULL;
  line->next = NULL;
  line->texture = NULL;
  return line;
}

void line_insert_text(Line *line, int16_t *column, char *text,
                      int16_t text_size) {
  assert(line->max_size >= (line->size + text_size));
  memcpy(line->text + *column + text_size, line->text + *column,
         line->size - *column);
  memcpy(line->text + *column, text, text_size);
  line_invalidate_texture(line);
  line->size += text_size;
  *column += text_size;
}

// TODO: reindex using the previous index
void editor_frame_reindex(EditorFrame *frame) {
  int16_t i = 0;
  for (Line *line = frame->line; line != NULL; line = line->next) {
    frame->index[i] = line;
    ++i;
    assert(i < INDEX_SIZE);
  }
  assert(i == frame->line_count);
}

void editor_frame_cursor_reset(EditorFrame *frame, int16_t *column) {
  frame->viewport_start = 0;
  frame->cursor.line_num = 0;
  frame->cursor.line = frame->line;
  if (column != NULL) {
    frame->cursor.column = *column;
  }
}

void editor_frame_move_cursor(EditorFrame *frame, int16_t d, int16_t *column) {
  int16_t prev_line_num = frame->cursor.line_num;
  int16_t line_num = frame->cursor.line_num + d;
  if (line_num < 0) {
    line_num = 0;
  } else if (line_num > frame->line_count) {
    line_num = frame->line_count - 1;
  }

  frame->cursor.line_num = line_num;
  frame->cursor.line = frame->index[frame->cursor.line_num];

  d = line_num - prev_line_num;
  if ((frame->cursor.line_num < frame->viewport_start) ||
      (frame->cursor.line_num >=
       (frame->viewport_start + frame->viewport_length))) {
    frame->viewport_start += d;
  }

  if (frame->viewport_start < 0) {
    frame->viewport_start = 0;
  } else if (frame->viewport_start >= frame->line_count) {
    frame->viewport_start = frame->line_count - 1;
  }

  if (column != NULL) {
    frame->cursor.column = *column;
  } else {
    if (frame->cursor.column > frame->cursor.line->size) {
      frame->cursor.column = frame->cursor.line->size;
    }
  }
}

void editor_frame_clear(EditorFrame *frame) {
  for (Line *line = frame->line->next; line != NULL;) {
    line_invalidate_texture(line);
    Line *next_line = line->next;
    line->next = NULL;
    if (frame->deleted_line == NULL) {
      frame->deleted_line = line;
    } else {
      line_insert_next(frame->deleted_line, line);
    }
    line = next_line;
  }
  int16_t new_column = 0;
  editor_frame_cursor_reset(frame, &new_column);
  frame->cursor.line->next = NULL;
  frame->line->size = 0;
  frame->line_count = 1;
}

void editor_frame_remove_char(EditorFrame *frame) {
  if (frame->cursor.column == 0) {
    if (frame->cursor.line->prev != NULL) {
      Line *line_to_remove = frame->cursor.line;
      line_invalidate_texture(line_to_remove);

      // we remove line_to_remove
      frame->cursor.line = line_to_remove->prev;
      frame->cursor.line->next = line_to_remove->next;
      frame->cursor.line_num--;
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

      if (frame->deleted_line == NULL) {
        frame->deleted_line = line_to_remove;
        frame->deleted_line->next = NULL;
      } else {
        line_insert_next(frame->deleted_line, line_to_remove);
      }
      editor_frame_reindex(frame);
    }
  } else {
    assert(frame->cursor.column <= frame->cursor.line->size);
    memcpy(frame->cursor.line->text + frame->cursor.column - 1,
           frame->cursor.line->text + frame->cursor.column,
           frame->cursor.line->size - frame->cursor.column);
    frame->cursor.line->size--;
    frame->cursor.column--;
  }

  line_invalidate_texture(frame->cursor.line);
}

void editor_frame_insert_new_line(MemoryArena *arena, EditorFrame *frame) {
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
    line_invalidate_texture(frame->cursor.line);
    frame->cursor.line->size = frame->cursor.column;
  } else {
    new_line->size = 0;
  }

  line_insert_next(frame->cursor.line, new_line);
  frame->line_count++;
  editor_frame_reindex(frame);
  static int16_t new_column = 0;
  editor_frame_move_cursor(frame, 1, &new_column);
}

void ex_frame_remove_char(ExFrame *frame) {
  if (frame->cursor_column > 0) {
    assert(frame->cursor_column <= frame->line->size);
    memcpy(frame->line->text + frame->cursor_column,
           frame->line->text + frame->cursor_column + 1,
           frame->line->size - frame->cursor_column);
    frame->line->size--;
    frame->cursor_column--;
    line_invalidate_texture(frame->line);
  }
}

#if DEBUG_PLAYBACK == PLAYBACK_RECORDING
int16_t poll_event(Input *input, SDL_Event *event) {
  int16_t pending_event = SDL_PollEvent(event);
  if (pending_event) {
    // NOTE: this is insane
    assert(fwrite(event, sizeof(SDL_Event), 1, input->playbackFile) == 1);
    assert(fflush(input->playbackFile) == 0);
  }
  return pending_event;
}
#elif DEBUG_PLAYBACK == PLAYBACK_PLAYING
int16_t poll_event(Input *input, SDL_Event *event) {
  int16_t read_size = fread(event, sizeof(SDL_Event), 1, input->playbackFile);
  if (read_size == 1) {
    return 1;
  } else {
    return SDL_PollEvent(event);
  }
}
#else
int16_t poll_event(Input *input, SDL_Event *event) {
  (void)(input); // avoid unused parameter warning
  return SDL_PollEvent(event);
}
#endif

void key_state_machine_reset(KeyStateMachine *ksm) {
  ksm->state = KeyStateMachine_Repetitions;
  ksm->keys_size = 0;
  ksm->operator_size = 0;

  if (ksm->texture != NULL) {
    SDL_DestroyTexture(ksm->texture);
    ksm->texture = NULL;
  }
}

enum KeyStateMachineState key_dispatch(State *state, KeyStateMachine *ksm) {
  ExFrame *ex_frame = &state->ex_frame;
  EditorFrame *editor_frame = &state->editor_frame;

  char *operator= ksm->operator;

  if (ksm->operator_size == 1) {
    switch (operator[0]) {
    case ':': {
      state->mode = AppMode_ex;
      ex_frame->line->size = 0;
      ex_frame->cursor_column = 0;
    } break;
    case 'i': {
      state->mode = AppMode_insert;
    } break;
    case 'h': {
      int16_t column = editor_frame->cursor.column - ksm->repetitions;
      if (column < 0) {
        column = 0;
      }
      editor_frame->cursor.column = column;
    } break;
    case 'l': {
      int16_t column = editor_frame->cursor.column + ksm->repetitions;
      if (column > editor_frame->cursor.line->size) {
        column = editor_frame->cursor.line->size;
      }
      editor_frame->cursor.column = column;
    } break;
    case 'k': {
      editor_frame_move_cursor(editor_frame, -1 * (ksm->repetitions), NULL);
    } break;
    case 'j': {
        editor_frame_move_cursor(editor_frame, ksm->repetitions, NULL);
    } break;
    case 'H': {
      editor_frame->cursor.column = 0;
    } break;
    case 'L': {
      editor_frame->cursor.column = editor_frame->cursor.line->size;
    } break;
    case 'A': {
      editor_frame->cursor.column = editor_frame->cursor.line->size;
      state->mode = AppMode_insert;
    } break;
    case 'G': {
      editor_frame_move_cursor(
          editor_frame,
          (editor_frame->line_count - editor_frame->cursor.line_num -1),
          &editor_frame->cursor.column);
    } break;
    case 'd':
    case 'g': {
      return KeyStateMachine_Operator;
    } break;
    }
  } else if (operator[0] == 'd' && operator[1] == 'd') {
    key_state_machine_reset(ksm);

    if (editor_frame->line == editor_frame->cursor.line) {
      editor_frame->line->size = 0;
      editor_frame->cursor.column = 0;
    } else {
      // TODO
    }
  } else if (operator[0] == 'g' && operator[1] == 'g') {
    editor_frame_move_cursor(editor_frame, -1 * (editor_frame->cursor.line_num),
                             &editor_frame->cursor.column);
  }
  return KeyStateMachine_Done;
}

void key_state_machine_add_key(KeyStateMachine *ksm, char c, State *state) {
  if (ksm->texture != NULL) {
    SDL_DestroyTexture(ksm->texture);
    ksm->texture = NULL;
  }
  enum KeyStateMachineState new_state = ksm->state;
  switch (ksm->state) {
  case KeyStateMachine_Repetitions: {
    if (c < '0' || c > '9') {
      if (ksm->keys_size == 0) {
        ksm->repetitions = 1;
      } else {
        ksm->state = KeyStateMachine_Operator;
        ksm->keys[ksm->keys_size] = '\0';
        ksm->repetitions = atoi(ksm->keys);
      }
      ksm->operator[ksm->operator_size] = c;
      ksm->operator_size++;
      new_state = key_dispatch(state, ksm);
    }
  } break;
  case KeyStateMachine_Operator: {
    ksm->operator[ksm->operator_size] = c;
    ksm->operator_size++;
    new_state = key_dispatch(state, ksm);
  } break;
  case KeyStateMachine_Done: {
    assert(false);
  } break;
  }
  ksm->keys[ksm->keys_size] = c;
  ksm->keys_size++;

  if (new_state == KeyStateMachine_Done) {
    key_state_machine_reset(ksm);
  } else {
    ksm->state = new_state;
  }
}

int16_t load_file(SDL_Renderer *renderer, State *state, char *filename) {
  FILE *f = fopen(filename, "r");
  if (!f) {
    return -1;
  }

  editor_frame_clear(&state->editor_frame);
  assert_line_integrity(state);
  assert(state->editor_frame.line_count == 1);

  char c;
  int16_t read_size = fread(&c, sizeof(char), 1, f);
  while (read_size > 0) {
    if (c == '\n') {
      editor_frame_insert_new_line(&state->arena, &state->editor_frame);
    } else {
      assert(c >= 32);
      line_insert_text(state->editor_frame.cursor.line,
                       &state->editor_frame.cursor.column, &c, 1);
    }
    read_size = fread(&c, sizeof(char), 1, f);
  }
  fclose(f);

  static int16_t column = 0;
  editor_frame_cursor_reset(&state->editor_frame, &column);
  editor_frame_reindex(&state->editor_frame);

  // TODO: reuse string if already is reserved
  state->filename = pushString(&state->arena, filename);

  SDL_Color color = {UNHEX(MODELINE_FONT_COLOR)};

  if (state->filename_texture != NULL) {
    SDL_DestroyTexture(state->filename_texture);
  }
  state->filename_texture =
      texture_from_text(renderer, state->font, state->filename, color,
                        &state->filename_texture_width);
  return 0;
}

void state_create(State *state, SDL_Renderer *renderer, Memory *memory) {
  state->mode = AppMode_normal;

  // NOTE: this is executed once
  // TODO: there are a bunch of things that should be cleared here at exit,
  // not sure if we care
  state->font = TTF_cpointer(
      TTF_OpenFont("/usr/share/fonts/TTF/IosevkaNerdFont-Regular.ttf", 20));

  state->font_h = TTF_FontHeight(state->font);

  SDL_Color sdlFontColor = {UNHEX(EDITOR_FONT_COLOR)};
  for (int16_t i = ASCII_LOW; i < ASCII_HIGH; ++i) {
    SDL_Surface *surface =
        TTF_cpointer(TTF_RenderGlyph_Solid(state->font, i, sdlFontColor));
    state->glyph_width[i - ASCII_LOW] = surface->w;
    SDL_FreeSurface(surface);
  }

  {
    SDL_Color color = {UNHEX(EX_FONT_COLOR)};
    SDL_Surface *surface =
        TTF_cpointer(TTF_RenderGlyph_Solid(state->font, ':', color));
    state->ex_prefix_texture_width = surface->w;
    state->ex_prefix_texture =
        SDL_cpointer(SDL_CreateTextureFromSurface(renderer, surface));
    SDL_FreeSurface(surface);
  }

  initializeArena(&state->arena, memory->permanentStorageSize - sizeof(State),
                  (uint8_t *)memory->permanentStorage + sizeof(State));

  {
    Line *line = line_create(&state->arena);
    state->editor_frame =
        (EditorFrame){.line = line,
                      .cursor = (Cursor){.line = line, .column = 0},
                      .line_count = 1,
                      .viewport_start = 0};
    editor_frame_reindex(&state->editor_frame);
  }

  state->ex_frame =
      (ExFrame){.line = line_create(&state->arena), .cursor_column = 0};

  SDL_Color modeColor = {UNHEX(MODELINE_FONT_COLOR)};
  state->appModeTextures[AppMode_ex] =
      cached_texture_create(renderer, state->font, "EX | ", modeColor);
  state->appModeTextures[AppMode_normal] =
      cached_texture_create(renderer, state->font, "NORMAL | ", modeColor);
  state->appModeTextures[AppMode_insert] =
      cached_texture_create(renderer, state->font, "INSERT | ", modeColor);

  state->filename = NULL;

  key_state_machine_reset(&state->normal_ksm);
}

void state_destroy(State *state) {
  TTF_CloseFont(state->font);

  SDL_DestroyTexture(state->ex_prefix_texture);
  SDL_DestroyTexture(state->appModeTextures[AppMode_ex].texture);
  SDL_DestroyTexture(state->appModeTextures[AppMode_normal].texture);
  SDL_DestroyTexture(state->appModeTextures[AppMode_insert].texture);
}

extern UPDATE_AND_RENDER(UpdateAndRender) {
  assert(sizeof(State) <= memory->permanentStorageSize);

#if DEBUG_WINDOW
  uint32 debugBackgroundColor = 0xFFFFFFFF;
  uint32 debugFontColor = 0x00000000;
  SDL_SetRenderDrawColor(buffer->debugRenderer, UNHEX(debugBackgroundColor));
  SDL_RenderClear(buffer->debugRenderer);
#endif

  SDL_ccode(SDL_SetRenderDrawColor(buffer->renderer, UNHEX(BG_COLOR)));
  SDL_ccode(SDL_RenderClear(buffer->renderer));

  State *state = (State *)memory->permanentStorage;

  if (!state->isInitialized) {
    state_create(state, buffer->renderer, memory);
    state->isInitialized = true;
  }

  // NOTE(casey): Transient initialization
  assert(sizeof(TransientState) <= memory->transientStorageSize);
  TransientState *transientState = (TransientState *)memory->transientStorage;
  if (!transientState->isInitialized) {
    initializeArena(&transientState->arena,
                    memory->transientStorageSize - sizeof(TransientState),
                    (uint8_t *)memory->transientStorage +
                        sizeof(TransientState));

    transientState->isInitialized = true;
  }

  if (input->executableReloaded) {
    printf("RELOAD\n");
  }

  EditorFrame *editor_frame = &state->editor_frame;
  ExFrame *ex_frame = &state->ex_frame;

  const int16_t editor_frame_start_y = buffer->height * 0.01;
  const int16_t modeline_frame_start_y = buffer->height - state->font_h * 3;
  const int16_t ex_frame_start_y = modeline_frame_start_y + 1.5 * state->font_h;
  editor_frame->viewport_length =
      (modeline_frame_start_y - editor_frame_start_y) / state->font_h;

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
            state_destroy(state);
            return 1;
          } else if (line_eq(ex_frame->line, "load")) {
            load_file(buffer->renderer, state, "src/htext_app.c");
            assert_line_integrity(state);
          } else if (line_eq(ex_frame->line, "clear")) {
            editor_frame_clear(editor_frame);
            assert_line_integrity(state);
          }
        } else if (event.key.keysym.scancode == SDL_SCANCODE_BACKSPACE) {
          ex_frame_remove_char(ex_frame);
          assert_line_integrity(state);
        } else if (event.key.keysym.scancode == SDL_SCANCODE_ESCAPE) {
          state->mode = AppMode_normal;
          ex_frame->line->size = 0;
          ex_frame->cursor_column = 0;
        }
        break;
      case AppMode_insert:
        if (event.key.keysym.scancode == SDL_SCANCODE_RETURN) {
          editor_frame_insert_new_line(&state->arena, editor_frame);
          assert_line_integrity(state);
        } else if (event.key.keysym.scancode == SDL_SCANCODE_BACKSPACE) {
          editor_frame_remove_char(editor_frame);
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
      case AppMode_normal: {
        for (size_t x = 0; x < strlen(event.text.text); ++x) {
          key_state_machine_add_key(&state->normal_ksm, event.text.text[x],
                                    state);
        }
      } break;
      case AppMode_ex: {
        int16_t text_size = strlen(event.text.text);
        line_insert_text(ex_frame->line, &ex_frame->cursor_column,
                         event.text.text, text_size);
      } break;
      case AppMode_insert: {
        int16_t text_size = strlen(event.text.text);
        line_insert_text(editor_frame->cursor.line,
                         &editor_frame->cursor.column, event.text.text,
                         text_size);
      } break;
      default:
        assert(false);
        break;
      }
      break;
    case SDL_QUIT: /* if mouse click to close window */
      state_destroy(state);
      return 1;
    }
  }

  // -------- rendering
  // render editor frame
  {
    assert(editor_frame->viewport_start >= 0);
    assert(editor_frame->viewport_start < editor_frame->line_count);
    Line *start_line = editor_frame->index[editor_frame->viewport_start];
    assert(start_line != NULL);
    Line *end_line = NULL;

    if ((editor_frame->viewport_start + editor_frame->viewport_length) <
        editor_frame->line_count) {
      end_line = editor_frame->index[editor_frame->viewport_start +
                                     editor_frame->viewport_length];
    }

    int16_t x_start = buffer->width * 0.01;
    SDL_Rect dest;
    dest.x = x_start;
    dest.y = editor_frame_start_y;
    dest.h = state->font_h;

    bool is_cursor_active = state->mode != AppMode_ex;
    int16_t line_number = editor_frame->viewport_start;
    SDL_Color color = {UNHEX(EDITOR_FONT_COLOR)};
    char line_number_str[6];

    for (Line *line = start_line; line != end_line; line = line->next) {
      assert(line_number < LINE_NUMBER_TEXTURE_CACHE_SIZE);
      SDL_Texture *line_number_texture =
          state->line_number_texture_cache[line_number];
      if (line_number_texture == NULL) {
        sprintf(line_number_str, "%4d", line_number);
        line_number_texture =
            texture_from_text(buffer->renderer, state->font, line_number_str,
                              color, &state->line_number_texture_width);
        state->line_number_texture_cache[line_number] = line_number_texture;
      }

      dest.w = state->line_number_texture_width;
      SDL_ccode(
          SDL_RenderCopy(buffer->renderer, line_number_texture, NULL, &dest));

      dest.x += dest.w + state->font_h;

      render_line(state, buffer->renderer, line,
                  line == editor_frame->cursor.line
                      ? &editor_frame->cursor.column
                      : NULL,
                  dest.x, dest.y, is_cursor_active, color);
      dest.y += state->font_h;
      dest.x = x_start;
      line_number++;
    }
  }

  // render modeline
  {
    assert(state->mode < AppMode_count);
    CachedTexture *texture = state->appModeTextures + state->mode;
    SDL_Rect dest;
    dest.y = modeline_frame_start_y;
    dest.h = state->font_h;
    dest.x = 0;
    dest.w = buffer->width;

    SDL_ccode(
        SDL_SetRenderDrawColor(buffer->renderer, UNHEX(MODELINE_BG_COLOR)));
    SDL_ccode(SDL_RenderFillRect(buffer->renderer, &dest));

    dest.x = buffer->width * 0.01;
    dest.w = texture->w;
    SDL_ccode(SDL_RenderCopy(buffer->renderer, texture->texture, NULL, &dest));

    dest.x += dest.w;

    if (state->filename != NULL) {
      dest.w = state->filename_texture_width;
      SDL_RenderCopy(buffer->renderer, state->filename_texture, NULL, &dest);
      dest.x += dest.w + state->font_h;
    }

    if (state->normal_ksm.keys_size > 0) {
      if (state->normal_ksm.texture == NULL) {
        SDL_Color color = {UNHEX(MODELINE_FONT_COLOR)};
        state->normal_ksm.keys[state->normal_ksm.keys_size] = '\0';
        state->normal_ksm.texture = texture_from_text(
            buffer->renderer, state->font, state->normal_ksm.keys, color,
            &state->normal_ksm.texture_width);
      }

      dest.w = state->normal_ksm.texture_width;
      SDL_RenderCopy(buffer->renderer, state->normal_ksm.texture, NULL, &dest);
      dest.x += dest.w;
    }
  }

  if (state->mode == AppMode_ex) {
    assert(ex_frame->line->next == NULL);

    int16_t x = 0.005 * buffer->width;

    SDL_Rect dest;
    dest.x = x;
    dest.y = ex_frame_start_y;
    dest.h = state->font_h;
    dest.w = state->ex_prefix_texture_width;
    SDL_ccode(SDL_RenderCopy(buffer->renderer, state->ex_prefix_texture, NULL,
                             &dest));
    dest.x += dest.w;

    SDL_Color color = {UNHEX(EX_FONT_COLOR)};
    render_line(state, buffer->renderer, ex_frame->line,
                &ex_frame->cursor_column, dest.x, dest.y,
                state->mode == AppMode_ex, color);
  }

#if DEBUG_WINDOW
  {
    SDL_Color color = {UNHEX(debugFontColor)};
    const int16_t margin_x = buffer->width * 0.01;
    const int16_t margin_y = buffer->height * 0.01;

    char text[100];

    SDL_Rect dest;
    dest.x = margin_x;
    dest.y = margin_y;
    dest.h = state->font_h;

    {
      sprintf(text, "Main frame cursor position: %d",
              editor_frame->cursor.column);
      SDL_Texture *texture = texture_from_text(
          buffer->debugRenderer, state->font, text, color, &dest.w);
      SDL_RenderCopy(buffer->debugRenderer, texture, NULL, &dest);
      SDL_DestroyTexture(texture);
    }

    dest.y += state->font_h;

    if (editor_frame->line->size > 0) {
      memcpy(text, editor_frame->line->text, editor_frame->line->size);
      text[editor_frame->line->size] = '\0';

      SDL_Texture *texture = texture_from_text(
          buffer->debugRenderer, state->font, text, color, &dest.w);
      SDL_RenderCopy(buffer->debugRenderer, texture, NULL, &dest);
      SDL_DestroyTexture(texture);
      dest.y += state->font_h;
    }

    if (editor_frame->line->size > 0) {
      sprintf(text, "Line size: %d", editor_frame->line->size);

      SDL_Texture *texture = texture_from_text(
          buffer->debugRenderer, state->font, text, color, &dest.w);
      SDL_RenderCopy(buffer->debugRenderer, texture, NULL, &dest);
      SDL_DestroyTexture(texture);
      dest.y += state->font_h;
    }

    {
      sprintf(text, "Ex frame cursor position: %d", ex_frame->cursor.column);
      SDL_Texture *texture = texture_from_text(
          buffer->debugRenderer, state->font, text, color, &dest.w);
      SDL_RenderCopy(buffer->debugRenderer, texture, NULL, &dest);
      SDL_DestroyTexture(texture);
      dest.y += state->font_h;
    }
  }
#endif

  return 0;
}
