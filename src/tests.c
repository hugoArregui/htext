#include "htext_app.c"
#include "htext_app.h"
#include <sys/mman.h>

int main(void) {
  void *baseAddress = (void *)(0);

  uint64 totalSize = Megabytes(256);
  // NOTE: MAP_ANONYMOUS content initialized to zero
  void* gameMemoryBlock =
      mmap(baseAddress, totalSize, PROT_READ | PROT_WRITE,
           MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
  assert(gameMemoryBlock != MAP_FAILED);

  MemoryArena arena;

  initializeArena(&arena, totalSize, gameMemoryBlock);

  Frame frame = frame_create(&arena);

  frame_insert_text(&frame, "hello world");
  assert(frame.line == frame.cursor_line);
  assert(frame.line->prev == NULL);
  assert(frame.line->size == 11);
  assert(frame.cursor_pos == 11);

  //----
  frame.cursor_pos = 5;
  frame_insert_text(&frame, ",");
  assert(frame.line->size == 12);
  assert(frame.line->prev == NULL);
  assert(line_eq(frame.line, "hello, world"));
  assert(frame.cursor_pos == 6);

  //----
  frame.cursor_pos = 5;
  frame_remove_char(&frame);
  assert(line_eq(frame.line, "hello world"));
  assert(frame.cursor_pos == 4);
  assert(frame.line->prev == NULL);

  //----
  frame.cursor_pos = 6;
  frame_insert_new_line(&arena, &frame);
  /*
    "hello "
    "world" cursor_pos = 0
  */
  assert(frame.line->size == 6);
  assert(frame.line->prev == NULL);
  assert(frame.line->next == frame.cursor_line);
  assert(frame.line->next->prev == frame.line);
  assert(frame.cursor_line == frame.line->next);
  assert(frame.cursor_line->size == 5);
  assert(line_eq(frame.line, "hello "));
  assert(line_eq(frame.cursor_line, "world"));
  assert(frame.cursor_pos == 0);
  assert(frame.deleted_line == NULL);

  //----
  frame_insert_new_line(&arena, &frame);
  /*
    "hello "
    ""
    "world" cursor_pos = 0
  */
  assert(frame.line->size == 6);
  assert(frame.line->next->size == 0);
  assert(frame.line->next->next->size == 5);
  assert(frame.cursor_pos == 0);
  assert(frame.deleted_line == NULL);

  assert(frame.line->prev == NULL);
  assert(frame.line->next->prev == frame.line);
  assert(frame.line->next->next == frame.cursor_line);
  assert(frame.line->next->next->prev == frame.line->next);

  //----
  frame_remove_char(&frame);
  /*
    "hello "
    "world" cursor_pos = 0
  */
  assert(frame.line->size == 6);
  assert(frame.line->next->size == 5);
  assert(frame.cursor_line == frame.line->next);
  assert(frame.cursor_pos == 0);
  assert(frame.deleted_line != NULL);

  assert(frame.line->prev == NULL);
  assert(frame.line->next->prev == frame.line);
  assert(frame.line->next->next == NULL);
  assert(frame.line->next == frame.cursor_line);

  Line* deleted_line = frame.deleted_line;

  //----
  frame.cursor_pos = 5;
  frame_insert_new_line(&arena, &frame);
  /*
    "hello "
    "world"
    "" cursor_pos = 0
  */
  assert(frame.line->size == 6);
  assert(frame.line->next->size == 5);
  assert(frame.line->next->next->size == 0);
  assert(deleted_line == frame.cursor_line); // reusing deleted line
  assert(frame.cursor_pos == 0);
  assert(frame.deleted_line == NULL);

  assert(frame.line->prev == NULL);
  assert(frame.line->next->prev == frame.line);
  assert(frame.line->next->next->prev == frame.line->next);
  assert(frame.line->next->next == frame.cursor_line);



  return 0;
}
