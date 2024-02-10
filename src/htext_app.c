#include "htext_app.h"
#include "htext_sdl.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_pixels.h>
#include <SDL2/SDL_rect.h>
#include <SDL2/SDL_render.h>
#include <SDL2/SDL_ttf.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>

#define BG_COLOR 0x00000000
#define EDITOR_FONT_COLOR 0x08a00300
#define CURSOR_COLOR 0xFFFFFF9F

#define MODELINE_BG_COLOR 0x595959FF
#define MODELINE_FONT_COLOR 0xFFFFFF00

#define STATUS_MESSAGE_FONT_COLOR 0xFFFFFF00

#define EX_FONT_COLOR 0xFFFFFF00

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

void line_invalidate_texture(Line *line) {
  if (line->texture != NULL) {
    SDL_DestroyTexture(line->texture);
    line->texture = NULL;
  }
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

bool line_eq(Line *line, char *str) {
  return ((int16_t)strlen(str)) == line->size &&
         strncmp(line->text, str, line->size) == 0;
}

bool line_starts_with(Line *line, char *str) {
  int16_t len = strlen(str);
  return line->size >= len && strncmp(line->text, str, len) == 0;
}

#include "htext_editor_frame.c"

SDL_Texture *texture_from_text(SDL_Renderer *renderer, TTF_Font *font,
                               char *text, SDL_Color color, int32_t *w) {

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

void editor_frame_render_line(RendererContext context, Line *line,
                              SDL_Point start, SDL_Color color) {
  State *state = context.state;
  EditorFrame *frame = &state->editor_frame;
  Viewport viewport_h = frame->viewport_h;
  int16_t cursor_column = frame->cursor.column;
  const int16_t cursor_w = state->font_h / 2;

  SDL_Rect dest;
  dest.x = start.x;
  dest.y = start.y;
  dest.h = state->font_h;

  if (line == frame->cursor.line) {
    int16_t cursor_x = dest.x;
    for (int16_t i = 0; i < cursor_column - viewport_h.start; ++i) {
      cursor_x += state->glyph_width[line->text[i] - ASCII_LOW];
    }

    SDL_Rect cursorDest;
    cursorDest.x = cursor_x;
    cursorDest.y = dest.y;
    cursorDest.h = state->font_h;
    cursorDest.w = cursor_w;
    SDL_ccode(
        SDL_SetRenderDrawBlendMode(context.renderer, SDL_BLENDMODE_BLEND));
    SDL_ccode(SDL_SetRenderDrawColor(context.renderer, UNHEX(CURSOR_COLOR)));

    if (state->mode != AppMode_ex) {
      SDL_ccode(SDL_RenderFillRect(context.renderer, &cursorDest));
    } else {
      SDL_ccode(SDL_RenderDrawRect(context.renderer, &cursorDest));
    }
  }

  if (line->size > 0) {
    bool should_render_line = line->size > viewport_h.start;
    if (line->texture == NULL && should_render_line) {
      int str_to_render_size = line->size - viewport_h.start;
      if (str_to_render_size > viewport_h.size) {
        str_to_render_size = viewport_h.size;
      }
      char *str_to_render = pushSize(context.transient_arena,
                                     str_to_render_size + 1, DEFAULT_ALIGMENT);
      memcpy(str_to_render, line->text + viewport_h.start, str_to_render_size);
      str_to_render[str_to_render_size] = '\0';

      line->texture =
          texture_from_text(context.renderer, state->font, str_to_render, color,
                            &line->texture_width);
    }
    if (line->texture != NULL && should_render_line) {
      dest.w = line->texture_width;
      SDL_ccode(SDL_RenderCopy(context.renderer, line->texture, NULL, &dest));
    }
  }

  dest.y += state->font_h;
  dest.x = start.x;
}

void ex_frame_render_line(RendererContext context, Line *line,
                          int16_t cursor_column, SDL_Point start,
                          SDL_Color color) {
  State *state = context.state;
  const int16_t cursor_w = state->font_h / 2;

  SDL_Rect dest;
  dest.x = start.x;
  dest.y = start.y;
  dest.h = state->font_h;

  int16_t cursor_x = dest.x;
  for (int16_t i = 0; i < cursor_column; ++i) {
    cursor_x += state->glyph_width[line->text[i] - ASCII_LOW];
  }

  SDL_Rect cursorDest;
  cursorDest.x = cursor_x;
  cursorDest.y = dest.y;
  cursorDest.h = state->font_h;
  cursorDest.w = cursor_w;
  SDL_ccode(SDL_SetRenderDrawBlendMode(context.renderer, SDL_BLENDMODE_BLEND));
  SDL_ccode(SDL_SetRenderDrawColor(context.renderer, UNHEX(CURSOR_COLOR)));
  SDL_ccode(SDL_RenderFillRect(context.renderer, &cursorDest));

  if (line->size > 0) {
    if (line->texture == NULL) {
      line->text[line->size] = '\0';
      line->texture =
          texture_from_text(context.renderer, state->font, line->text, color,
                            &line->texture_width);
    }
    if (line->texture != NULL) {
      dest.w = line->texture_width;
      SDL_ccode(SDL_RenderCopy(context.renderer, line->texture, NULL, &dest));
    }
  }

  dest.y += state->font_h;
  dest.x = start.x;
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

enum KeyStateMachineState key_state_machine_dispatch(State *state,
                                                     KeyStateMachine *ksm) {
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
    case 'I': {
      editor_frame_move_cursor_h(editor_frame,
                                 -1 * editor_frame->cursor.column);
      state->mode = AppMode_insert;
    } break;
    case 'h': {
      editor_frame_move_cursor_h(editor_frame, -1 * (ksm->repetitions));
    } break;
    case 'l': {
      editor_frame_move_cursor_h(editor_frame, 1 * (ksm->repetitions));
    } break;
    case 'k': {
      editor_frame_move_cursor_v(editor_frame, -1 * (ksm->repetitions), NULL);
    } break;
    case 'j': {
      editor_frame_move_cursor_v(editor_frame, ksm->repetitions, NULL);
    } break;
    case 'H': {
      editor_frame_move_cursor_h(editor_frame,
                                 -1 * editor_frame->cursor.column);
    } break;
    case 'L': {
      editor_frame_move_cursor_h(editor_frame, editor_frame->cursor.line->size -
                                                   editor_frame->cursor.column);
    } break;
    case 'A': {
      editor_frame_move_cursor_h(editor_frame, editor_frame->cursor.line->size -
                                                   editor_frame->cursor.column);
      state->mode = AppMode_insert;
    } break;
    case 'G': {
      editor_frame_move_cursor_v(
          editor_frame,
          (editor_frame->line_count - editor_frame->cursor.line_num - 1),
          &editor_frame->cursor.column);
    } break;
    case 'd':
    case 'g': {
      return KeyStateMachine_Operator;
    } break;
    }
  } else if (operator[0] == 'd' && operator[1] == 'd') {
    key_state_machine_reset(ksm);
    editor_frame_remove_lines(editor_frame, ksm->repetitions);
  } else if (operator[0] == 'g' && operator[1] == 'g') {
    editor_frame_move_cursor_v(editor_frame,
                               -1 * (editor_frame->cursor.line_num),
                               &editor_frame->cursor.column);
  }

  assert_editor_frame_integrity(&state->editor_frame);

  state->status_message[0] = '\0';
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
      new_state = key_state_machine_dispatch(state, ksm);
    }
  } break;
  case KeyStateMachine_Operator: {
    ksm->operator[ksm->operator_size] = c;
    ksm->operator_size++;
    new_state = key_state_machine_dispatch(state, ksm);
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

int16_t load_file(RendererContext context, char *filename) {
  State *state = context.state;

  int r = editor_frame_load_file(&state->arena, &state->editor_frame, filename);
  if (r < 0) {
    return r;
  }

  // TODO(leak): reuse string if already is reserved
  state->filename = pushString(&state->arena, filename);

  SDL_Color color = {UNHEX(MODELINE_FONT_COLOR)};

  if (state->filename_texture != NULL) {
    SDL_DestroyTexture(state->filename_texture);
  }
  state->filename_texture =
      texture_from_text(context.renderer, state->font, state->filename, color,
                        &state->filename_texture_width);
  return 0;
}

int16_t dump_file(State *state, char *filename) {
  FILE *f = fopen(filename, "w");
  if (!f) {
    return -1;
  }

  EditorFrame *editor_frame = &state->editor_frame;

  char new_line = '\n';

  for (Line *line = editor_frame->line; line != NULL; line = line->next) {
    fwrite(line->text, line->size, 1, f);
    fwrite(&new_line, 1, 1, f);
  }

  fclose(f);

  return 0;
}

void state_create(State *state, SDL_Renderer *renderer, Memory *memory) {
  state->mode = AppMode_normal;

  state->font = TTF_cpointer(TTF_OpenFont("IosevkaNerdFont-Regular.ttf", 20));
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
                      .viewport_v.start = 0,
                      .viewport_h.start = 0};
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
  state->status_message[0] = '\0';

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
  uint32_t debugBackgroundColor = 0xFFFFFFFF;
  uint32_t debugFontColor = 0x00000000;
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

  RendererContext context =
      (RendererContext){.state = state,
                        .transient_arena = &transientState->arena,
                        .renderer = buffer->renderer};
  ;

  EditorFrame *editor_frame = &state->editor_frame;
  ExFrame *ex_frame = &state->ex_frame;

  const int16_t editor_frame_start_y = buffer->height * 0.01;
  const int16_t modeline_frame_start_y = buffer->height - state->font_h * 3;
  const int16_t ex_frame_start_y = modeline_frame_start_y + 1.5 * state->font_h;
  editor_frame->viewport_v.size =
      (modeline_frame_start_y - editor_frame_start_y) / state->font_h;
  // TODO
  editor_frame->viewport_h.size = 100;

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
            char *filename = "src/htext_app.c";
            if (load_file(context, filename) != 0) {
              sprintf(state->status_message, "Cannot open %s", filename);
            }
          } else if (line_starts_with(ex_frame->line, "load ")) {
            ex_frame->line->text[ex_frame->line->size] = '\0';
            char *filename = ex_frame->line->text + 5;
            if (load_file(context, filename) != 0) {
              sprintf(state->status_message, "Cannot open %s", filename);
            }
          } else if (line_starts_with(ex_frame->line, "dump ")) {
            ex_frame->line->text[ex_frame->line->size] = '\0';
            char *filename = ex_frame->line->text + 5;
            if (dump_file(state, filename) == 0) {
              sprintf(state->status_message, "Wrote to %s", filename);
            } else {
              sprintf(state->status_message, "Cannot write to %s", filename);
            }
          } else if (line_eq(ex_frame->line, "clear")) {
            editor_frame_clear(editor_frame);
          } else if (strlen(ex_frame->line->text) > 0) {
            sprintf(state->status_message, "Unrecognized command: %s",
                    ex_frame->line->text);
          }
        } else if (event.key.keysym.scancode == SDL_SCANCODE_BACKSPACE) {
          ex_frame_remove_char(ex_frame);
        } else if (event.key.keysym.scancode == SDL_SCANCODE_ESCAPE) {
          state->mode = AppMode_normal;
          ex_frame->line->size = 0;
          ex_frame->cursor_column = 0;
        }
        assert_editor_frame_integrity(&state->editor_frame);
        break;
      case AppMode_insert:
        if (event.key.keysym.scancode == SDL_SCANCODE_RETURN) {
          editor_frame_insert_new_line(&state->arena, editor_frame);
        } else if (event.key.keysym.scancode == SDL_SCANCODE_BACKSPACE) {
          editor_frame_remove_char(editor_frame);
        } else if (event.key.keysym.scancode == SDL_SCANCODE_ESCAPE) {
          state->mode = AppMode_normal;
        }
        assert_editor_frame_integrity(&state->editor_frame);
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
    assert(editor_frame->viewport_v.start >= 0);
    assert(editor_frame->viewport_h.start >= 0);
    assert(editor_frame->viewport_v.start < editor_frame->line_count);
    Line *start_line = editor_frame->index[editor_frame->viewport_v.start];
    assert(start_line != NULL);
    Line *end_line = NULL;

    if ((editor_frame->viewport_v.start + editor_frame->viewport_v.size) <
        editor_frame->line_count) {
      end_line = editor_frame->index[editor_frame->viewport_v.start +
                                     editor_frame->viewport_v.size];
    }

    int16_t x_start = buffer->width * 0.01;
    SDL_Rect dest;
    dest.x = x_start;
    dest.y = editor_frame_start_y;
    dest.h = state->font_h;

    int16_t line_number = editor_frame->viewport_v.start;
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

      SDL_Point start = (SDL_Point){.x = dest.x, .y = dest.y};
      editor_frame_render_line(context, line, start, color);
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
    SDL_Point start = (SDL_Point){.x = dest.x, .y = dest.y};
    ex_frame_render_line(context, ex_frame->line, ex_frame->cursor_column,
                         start, color);
  } else if (strlen(state->status_message) > 0) {
    int16_t x = 0.005 * buffer->width;
    SDL_Rect dest;
    dest.x = x;
    dest.y = ex_frame_start_y;
    dest.h = state->font_h;

    SDL_Color color = {UNHEX(STATUS_MESSAGE_FONT_COLOR)};
    SDL_Texture *texture = texture_from_text(
        buffer->renderer, state->font, state->status_message, color, &dest.w);
    SDL_ccode(SDL_RenderCopy(buffer->renderer, texture, NULL, &dest));
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
      sprintf(text, "Editor frame cursor column: %d",
              editor_frame->cursor.column);
      SDL_Texture *texture = texture_from_text(
          buffer->debugRenderer, state->font, text, color, &dest.w);
      SDL_RenderCopy(buffer->debugRenderer, texture, NULL, &dest);
      SDL_DestroyTexture(texture);
      dest.y += state->font_h;
    }

    {
      sprintf(text, "Editor frame cursor line number: %d",
              editor_frame->cursor.line_num);
      SDL_Texture *texture = texture_from_text(
          buffer->debugRenderer, state->font, text, color, &dest.w);
      SDL_RenderCopy(buffer->debugRenderer, texture, NULL, &dest);
      SDL_DestroyTexture(texture);
      dest.y += state->font_h;
    }

    {
      sprintf(text, "Editor frame viewport start: %d, size:%d",
              editor_frame->viewport_v_start, editor_frame->viewport_v_size);
      SDL_Texture *texture = texture_from_text(
          buffer->debugRenderer, state->font, text, color, &dest.w);
      SDL_RenderCopy(buffer->debugRenderer, texture, NULL, &dest);
      SDL_DestroyTexture(texture);
      dest.y += state->font_h;
    }

    dest.y += state->font_h;

    {
      sprintf(text, "Ex frame cursor column: %d", ex_frame->cursor_column);
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
