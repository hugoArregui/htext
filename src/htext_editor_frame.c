#include "htext_app.h"
#include <sys/stat.h>

#define TEXT_LINE_ALLOCATION_SIZE 100

static Line *line_create(MemoryArena *arena) {
  Line *line = pushStruct(arena, Line, DEFAULT_ALIGNMENT);
  line->len = 0;
  line->text = pushSize(arena, TEXT_LINE_ALLOCATION_SIZE, DEFAULT_ALIGNMENT);
  line->max_len = TEXT_LINE_ALLOCATION_SIZE - 1;
  line->prev = NULL;
  line->next = NULL;
  line->texture = NULL;
  return line;
}

static void line_invalidate_texture(Line *line) {
  if (line->texture != NULL) {
    SDL_DestroyTexture(line->texture);
    line->texture = NULL;
  }
}

static void line_insert_next(Line *line, Line *next_line) {
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

#if 1
#define assert_editor_frame_integrity(editor_frame)                            \
  _assert_editor_frame_integrity(editor_frame, __FILE__, __LINE__)

#define my_assert(cond, file, linenum)                                         \
  if (!(cond)) {                                                               \
    printf("%s:%d\n", file, linenum);                                          \
    assert(cond);                                                              \
  }

void _assert_editor_frame_integrity(EditorFrame *frame, char *file,
                                    int16_t linenum) {
  my_assert(frame->cursor.line != NULL, file, linenum);
  Line *prev_line = NULL;

  int16_t line_num = 0;
  for (Line *line = frame->line; line != NULL; line = line->next) {
    my_assert(line->len <= line->max_len, file, linenum);
    my_assert(line->prev != line, file, linenum);
    my_assert(line->next != line, file, linenum);
    my_assert(line->prev == prev_line, file, linenum);

    if (line == frame->cursor.line) {
      my_assert(line_num == frame->cursor.line_num, file, linenum)
    }

    for (int16_t i = 0; i < line->len; ++i) {
      my_assert(line->text[i] >= 32, file, linenum);
    }
    prev_line = line;
    line_num++;
  }

  assert(line_num == frame->line_count);

  for (Line *line = frame->deleted_line; line != NULL; line = line->next) {
    my_assert(line->texture == NULL, file, linenum);
  }
}
#else
#define assert_editor_frame_integrity(editor_frame) (void)editor_frame
#endif

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

void editor_frame_cursor_reset(EditorFrame *frame) {
  frame->viewport_v.start = 0;
  frame->viewport_h.start = 0;
  frame->cursor.line_num = 0;
  frame->cursor.line = frame->line;
  frame->cursor.column = 0;
}

void editor_frame_update_viewport(EditorFrame *frame) {
  if ((frame->cursor.line_num < frame->viewport_v.start) ||
      (frame->cursor.line_num >=
       (frame->viewport_v.start + frame->viewport_v.size))) {
    frame->viewport_v.start =
        frame->cursor.line_num - frame->viewport_v.size / 2;
  }

  if (frame->viewport_v.start < 0) {
    frame->viewport_v.start = 0;
  } else if (frame->viewport_v.start >= frame->line_count) {
    frame->viewport_v.start = frame->line_count - 1;
  }

  if ((frame->cursor.column < frame->viewport_h.start) ||
      (frame->cursor.column >=
       (frame->viewport_h.start + frame->viewport_h.size))) {
    frame->viewport_h.start = frame->cursor.column - frame->viewport_h.size / 2;
  }

  if (frame->viewport_h.start < 0) {
    frame->viewport_h.start = 0;
    // TODO
    /* } else if (frame->viewport_h.start >= frame->viewport_h.size) { */
    /*   frame->viewport_h.start = frame->viewport_h.size - 1; */
  }
}

// NOTE: moves cursor vertically,
// if a column is specified it will be used as cursor.column
void editor_frame_move_cursor_v(EditorFrame *frame, int16_t d,
                                int16_t *column) {
  assert(frame->cursor.line != NULL);
  int16_t cursor_line_num = frame->cursor.line_num + d;
  if (cursor_line_num >= frame->line_count) {
    cursor_line_num = frame->line_count - 1;
  }
  if (cursor_line_num < 0) {
    cursor_line_num = 0;
  }

  frame->cursor.line_num = cursor_line_num;
  frame->cursor.line = frame->index[frame->cursor.line_num];
  assert(frame->cursor.line != NULL);

  editor_frame_update_viewport(frame);

  if (column != NULL) {
    frame->cursor.column = *column;
  } else {
    if (frame->cursor.column > frame->cursor.line->len) {
      frame->cursor.column = frame->cursor.line->len;
    }
  }
}

void editor_frame_invalidate_viewport_textures(EditorFrame *frame) {
  for (int16_t i = frame->viewport_v.start; i < frame->viewport_v.size; ++i) {
    if (i == frame->line_count) {
      break;
    }
    line_invalidate_texture(frame->index[i]);
  }
}

void editor_frame_move_cursor_h(EditorFrame *frame, int16_t d) {
  assert(frame->cursor.line != NULL);

  int16_t column = frame->cursor.column + d;
  if (column < 0) {
    column = 0;
  }
  if (column > frame->cursor.line->len) {
    column = frame->cursor.line->len;
  }

  frame->cursor.column = column;
  if (column < frame->viewport_h.start ||
      column >= (frame->viewport_h.start + frame->viewport_h.size)) {
    editor_frame_invalidate_viewport_textures(frame);
  }

  editor_frame_update_viewport(frame);
}

void editor_frame_delete_line(EditorFrame *frame, Line *line) {
  Line *prev_line = line->prev;
  Line *next_line = line->next;

  line_invalidate_texture(line);

  if (frame->line == line) {
    assert(next_line != NULL);
    frame->line = next_line;
    frame->line->prev = NULL;
  } else {
    assert(prev_line != NULL);
    prev_line->next = next_line;
    if (next_line != NULL) {
      next_line->prev = prev_line;
    }
  }

  line->next = NULL;
  if (frame->deleted_line == NULL) {
    frame->deleted_line = line;
  } else {
    line_insert_next(frame->deleted_line, line);
  }

  frame->line_count--;
}

void editor_frame_clear(EditorFrame *frame) {
  while (frame->line->next != NULL) {
    editor_frame_delete_line(frame, frame->line->next);
  }
  editor_frame_cursor_reset(frame);
  frame->cursor.line->next = NULL;
  frame->line->len = 0;
  frame->line_count = 1;

  assert_editor_frame_integrity(frame);
}

void editor_frame_remove_char(EditorFrame *frame) {
  if (frame->cursor.column == 0) {
    if (frame->cursor.line->prev != NULL) {
      Line *line_to_remove = frame->cursor.line;

      int16_t column = frame->cursor.line->prev->len;
      editor_frame_move_cursor_v(frame, -1, &column);
      editor_frame_delete_line(frame, line_to_remove);

      if (line_to_remove->len > 0) {
        // we need to join line_to_remove with prev cursor line
        strncpy(frame->cursor.line->text + frame->cursor.line->len,
                line_to_remove->text, line_to_remove->len);
        frame->cursor.line->len += line_to_remove->len;
      }

      editor_frame_reindex(frame);
    }
  } else {
    assert(frame->cursor.column <= frame->cursor.line->len);
    strncpy(frame->cursor.line->text + frame->cursor.column - 1,
            frame->cursor.line->text + frame->cursor.column,
            frame->cursor.line->len - frame->cursor.column);
    frame->cursor.line->len--;
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
    new_line = line_create(arena);
  }

  if (frame->cursor.column < frame->cursor.line->len) {
    new_line->len = frame->cursor.line->len - frame->cursor.column;
    strncpy(new_line->text, frame->cursor.line->text + frame->cursor.column,
            new_line->len);
    line_invalidate_texture(frame->cursor.line);
    frame->cursor.line->len = frame->cursor.column;
  } else {
    new_line->len = 0;
  }

  line_insert_next(frame->cursor.line, new_line);
  frame->line_count++;
  editor_frame_reindex(frame);
  static int16_t new_column = 0;
  editor_frame_move_cursor_v(frame, 1, &new_column);
}

void editor_frame_remove_lines(EditorFrame *frame, int16_t n) {
  for (int16_t i = 0; i < n; ++i) {
    Line *line_to_remove = frame->cursor.line;
    if (frame->line_count == 1) {
      frame->line->len = 0;
      frame->cursor.column = 0;
      break;
    } else if (line_to_remove->next) {
      frame->cursor.line = line_to_remove->next;
    } else {
      frame->cursor.line = line_to_remove->prev;
      frame->cursor.line_num--;
      editor_frame_update_viewport(frame);
    }
    editor_frame_delete_line(frame, line_to_remove);
  }
  editor_frame_reindex(frame);
}

void editor_frame_insert_text(MemoryArena *arena, MemoryArena *transient_arena,
                              EditorFrame *frame, char *text,
                              int16_t text_size) {
  assert(text_size > 0);

  Line *line = frame->cursor.line;
  if (line->max_len < (line->len + text_size)) {
    line->text[line->len] = 0;
    char *s = pushString(transient_arena, line->text);

    double chunks = ((double)(line->len + text_size + 1)) /
                    (double)TEXT_LINE_ALLOCATION_SIZE;
    int16_t new_size = ceil(chunks) * TEXT_LINE_ALLOCATION_SIZE;

    line->max_len = new_size - 1;
    line->text = pushSize(arena, new_size, DEFAULT_ALIGNMENT);
    strcpy(line->text, s);
  }

  assert(line->max_len >= (line->len + text_size));

  int16_t column = frame->cursor.column;
  strncpy(line->text + column + text_size, line->text + column,
          line->len - column);
  strncpy(line->text + column, text, text_size);
  line_invalidate_texture(line);
  line->len += text_size;
  frame->cursor.column += text_size;
}

// TODO rewrite to avoid moving the cursor and such
int editor_frame_load_file(MemoryArena *arena, MemoryArena *transient_arena,
                           EditorFrame *editor_frame, char *filename) {
  FILE *f = fopen(filename, "r");
  if (!f) {
    return -1;
  }

  editor_frame_clear(editor_frame);
  assert_editor_frame_integrity(editor_frame);
  assert(editor_frame->line_count == 1);

  char c;
  int16_t read_size = fread(&c, sizeof(char), 1, f);
  while (read_size > 0) {
    if (c == '\n') {
      editor_frame_insert_new_line(arena, editor_frame);
    } else {
      assert(c >= 32);
      editor_frame_insert_text(arena, transient_arena, editor_frame, &c, 1);
    }
    read_size = fread(&c, sizeof(char), 1, f);
  }
  fclose(f);

  editor_frame_cursor_reset(editor_frame);
  editor_frame_reindex(editor_frame);
  assert_editor_frame_integrity(editor_frame);

  return 0;
}

EditorFrame editor_frame_create(MemoryArena *arena) {
  Line *line = line_create(arena);
  EditorFrame editor_frame =
      (EditorFrame){.line = line,
                    .cursor = (Cursor){.line = line, .column = 0},
                    .line_count = 1,
                    .viewport_v.start = 0,
                    .viewport_h.start = 0};
  editor_frame_reindex(&editor_frame);
  return editor_frame;
}
