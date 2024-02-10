#include "htext_app.h"

// NOTE: defined before ths included, adding them here so emacs doesn't complain
void line_invalidate_texture(Line *line);
void line_insert_next(Line *line, Line *next_line);

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
    my_assert(line->prev != line, file, linenum);
    my_assert(line->next != line, file, linenum);
    my_assert(line->prev == prev_line, file, linenum);
    my_assert(line->max_size > 0, file, linenum);

    if (line == frame->cursor.line) {
      my_assert(line_num == frame->cursor.line_num, file, linenum)
    }

    for (int16_t i = 0; i < line->size; ++i) {
      my_assert(line->text[i] >= 32, file, linenum);
    }
    prev_line = line;
    line_num++;
  }

  assert(line_num == frame->line_count);

  for (Line *line = frame->deleted_line; line != NULL; line = line->next) {
    my_assert(line->texture == NULL, file, linenum);
    my_assert(line->max_size > 0, file, linenum);
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

void editor_frame_cursor_reset(EditorFrame *frame, int16_t *column) {
  frame->viewport_v.start = 0;
  frame->viewport_h.start = 0;
  frame->cursor.line_num = 0;
  frame->cursor.line = frame->line;
  if (column != NULL) {
    frame->cursor.column = *column;
  }
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
    if (frame->cursor.column > frame->cursor.line->size) {
      frame->cursor.column = frame->cursor.line->size;
    }
  }
}

void editor_frame_move_cursor_h(EditorFrame *frame, int16_t d) {
  assert(frame->cursor.line != NULL);

  int16_t column = frame->cursor.column + d;
  if (column < 0) {
    column = 0;
  }
  if (column > frame->cursor.line->size) {
    column = frame->cursor.line->size;
  }

  frame->cursor.column = column;
  if (column < frame->viewport_h.start ||
      column >= (frame->viewport_h.start + frame->viewport_h.size)) {

    for (int16_t i = frame->viewport_v.start; i < frame->viewport_v.size; ++i) {
      line_invalidate_texture(frame->index[i]);
    }
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
  static int16_t new_column = 0;
  editor_frame_cursor_reset(frame, &new_column);
  frame->cursor.line->next = NULL;
  frame->line->size = 0;
  frame->line_count = 1;

  assert_editor_frame_integrity(frame);
}

void editor_frame_remove_char(EditorFrame *frame) {
  if (frame->cursor.column == 0) {
    if (frame->cursor.line->prev != NULL) {
      Line *line_to_remove = frame->cursor.line;

      int16_t column = frame->cursor.line->prev->size;
      editor_frame_move_cursor_v(frame, -1, &column);
      editor_frame_delete_line(frame, line_to_remove);

      if (line_to_remove->size > 0) {
        // we need to join line_to_remove with prev cursor line
        assert(frame->cursor.line->max_size >=
               (frame->cursor.line->size + line_to_remove->size));
        memcpy(frame->cursor.line->text + frame->cursor.line->size,
               line_to_remove->text, line_to_remove->size);
        frame->cursor.line->size += line_to_remove->size;
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
  editor_frame_move_cursor_v(frame, 1, &new_column);
}

void editor_frame_remove_lines(EditorFrame *frame, int16_t n) {
  for (int16_t i = 0; i < n; ++i) {
    Line *line_to_remove = frame->cursor.line;
    if (frame->line_count == 1) {
      frame->line->size = 0;
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
