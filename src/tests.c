#include "htext_app.c"
#include "htext_app.h"
#include <assert.h>
#include <math.h>
#include <string.h>
#include <sys/mman.h>

int main(void) {
  void *baseAddress = (void *)(0);

  uint64_t persistentSize = Megabytes(512);
  uint64_t transientSize = Megabytes(512);
  void *gameMemoryBlock =
      mmap(baseAddress, persistentSize + transientSize, PROT_READ | PROT_WRITE,
           MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
  assert(gameMemoryBlock != MAP_FAILED);

  MemoryArena arena;
  MemoryArena transient_arena;

  initializeArena(&arena, persistentSize, gameMemoryBlock);
  initializeArena(&transient_arena, transientSize,
                  ((uint8_t *)gameMemoryBlock) + persistentSize);

  EditorFrame frame = editor_frame_create(&arena);

  //-----
  for (int i = 0; i < 102; ++i) {
    editor_frame_insert_text(&transient_arena, &frame, "c", 1);
  }

  assert(strcmp(frame.line->text,
                "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
                "cccccccccccccccccccccccccccccccccccccccc") == 0);
  editor_frame_move_cursor_h(&frame, -100);
  editor_frame_insert_text(&transient_arena, &frame, "1", 1);
  assert(strcmp(frame.line->text,
                "cc1ccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
                "ccccccccccccccccccccccccccccccccccccccccc") == 0);
  editor_frame_close(&frame);

  editor_frame_insert_text(&transient_arena, &frame, "hello world", 11);
  assert(frame.line == frame.cursor.line);
  assert(frame.cursor.column == 11);
  assert(frame.line->prev == NULL);
  assert(frame.line->len == 11);
  //-----

  //----
  frame.cursor.column = 5;
  editor_frame_insert_text(&transient_arena, &frame, ",", 1);
  assert(frame.line->len == 12);
  assert(frame.line->prev == NULL);
  assert(strncmp(frame.line->text, "hello, world", 12) == 0);
  assert(frame.cursor.column == 6);

  //----
  frame.cursor.column = 6;
  editor_frame_remove_char(&frame);
  assert(strncmp(frame.line->text, "hello world", 11) == 0);
  assert(frame.cursor.column == 5);
  assert(frame.line->prev == NULL);

  //----
  frame.cursor.column = 6;
  editor_frame_insert_new_line(&frame);
  /*
    "hello "
    "world" cursor.column = 0
  */
  assert(frame.line->len == 6);
  assert(frame.line->prev == NULL);
  assert(frame.line->next == frame.cursor.line);
  assert(frame.line->next->prev == frame.line);
  assert(frame.cursor.line == frame.line->next);
  assert(frame.cursor.line->len == 5);
  assert(strncmp(frame.line->text, "hello ", 5) == 0);
  assert(strncmp(frame.cursor.line->text, "world", 5) == 0);
  assert(frame.cursor.column == 0);
  assert(frame.deleted_line == NULL);

  //----
  editor_frame_insert_new_line(&frame);
  /*
    "hello "
    ""
    "world" cursor.column = 0
  */
  assert(frame.line->len == 6);
  assert(frame.line->next->len == 0);
  assert(frame.line->next->next->len == 5);
  assert(frame.cursor.column == 0);
  assert(frame.deleted_line == NULL);

  assert(frame.line->prev == NULL);
  assert(frame.line->next->prev == frame.line);
  assert(frame.line->next->next == frame.cursor.line);
  assert(frame.line->next->next->prev == frame.line->next);

  //----
  editor_frame_remove_char(&frame);
  /*
    "hello "
    "world" cursor.column = 0
  */
  assert(frame.line->len == 6);
  assert(frame.line->next->len == 5);
  assert(frame.cursor.line == frame.line->next);
  assert(frame.cursor.column == 0);
  assert(frame.deleted_line != NULL);

  assert(frame.line->prev == NULL);
  assert(frame.line->next->prev == frame.line);
  assert(frame.line->next->next == NULL);
  assert(frame.line->next == frame.cursor.line);

  Line *deleted_line = frame.deleted_line;

  //----
  frame.cursor.column = 5;
  editor_frame_insert_new_line(&frame);
  /*
    "hello "
    "world"
    "" cursor.column = 0
  */
  assert(frame.line->len == 6);
  assert(frame.line->next->len == 5);
  assert(frame.line->next->next->len == 0);
  assert(deleted_line == frame.cursor.line); // reusing deleted line
  assert(frame.cursor.column == 0);
  assert(frame.deleted_line == NULL);

  assert(frame.line->prev == NULL);
  assert(frame.line->next->prev == frame.line);
  assert(frame.line->next->next->prev == frame.line->next);
  assert(frame.line->next->next == frame.cursor.line);

  return 0;
}
