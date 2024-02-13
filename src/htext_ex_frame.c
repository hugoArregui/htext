#include "htext_app.h"

void ex_frame_invalidate_texture(ExFrame *frame) {
  if (frame->texture != NULL) {
    SDL_DestroyTexture(frame->texture);
    frame->texture = NULL;
  }
}

void ex_frame_insert_text(ExFrame *frame, char *text, int16_t text_size) {
  assert(text_size > 0);
  assert(frame->size + text_size < frame->max_size);
  strncpy(frame->text + frame->cursor_column + text_size,
          frame->text + frame->cursor_column,
          frame->size - frame->cursor_column);
  strncpy(frame->text + frame->cursor_column, text, text_size);
  ex_frame_invalidate_texture(frame);
  frame->size += text_size;
  frame->cursor_column += text_size;
}

void ex_frame_remove_char(ExFrame *frame) {
  if (frame->cursor_column > 0) {
    assert(frame->cursor_column <= frame->size);
    strncpy(frame->text + frame->cursor_column,
            frame->text + frame->cursor_column + 1,
            frame->size - frame->cursor_column);
    frame->size--;
    frame->cursor_column--;
    ex_frame_invalidate_texture(frame);
  }
}

ExFrame ex_frame_create(MemoryArena *arena) {
  ExFrame ex_frame = (ExFrame){
      .size = 0, .max_size = 200, .cursor_column = 0, .texture = NULL};
  ex_frame.text = pushSize(arena, ex_frame.max_size, DEFAULT_ALIGMENT);
  return ex_frame;
}
