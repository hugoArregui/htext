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


  Line *line = line_create(&arena);
  MainFrame frame =
    (MainFrame){.line = line,
                      .cursor = (Cursor){.line = line, .column = 0},
                      .line_count = 1};

  cursor_insert_text(&frame.cursor, "hello world", 11);
  assert(frame.line == frame.cursor.line);
  assert(frame.cursor.column
         == 11);
  assert(frame.line->prev == NULL);
  assert(frame.line->size == 11);

  //----
  frame.cursor.column = 5;
  cursor_insert_text(&frame.cursor, ",", 1);
  assert(frame.line->size == 12);
  assert(frame.line->prev == NULL);
  assert(line_eq(frame.line, "hello, world"));
  assert(frame.cursor.column == 6);

  //----
  frame.cursor.column = 5;
  main_frame_remove_char(&frame);
  assert(line_eq(frame.line, "hello world"));
  assert(frame.cursor.column == 4);
  assert(frame.line->prev == NULL);

  //----
  frame.cursor.column = 6;
  main_frame_insert_new_line(&arena, &frame);
  /*
    "hello "
    "world" cursor.column = 0
  */
  assert(frame.line->size == 6);
  assert(frame.line->prev == NULL);
  assert(frame.line->next == frame.cursor.line);
  assert(frame.line->next->prev == frame.line);
  assert(frame.cursor.line == frame.line->next);
  assert(frame.cursor.line->size == 5);
  assert(line_eq(frame.line, "hello "));
  assert(line_eq(frame.cursor.line, "world"));
  assert(frame.cursor.column == 0);
  assert(frame.deleted_line == NULL);

  //----
  main_frame_insert_new_line(&arena, &frame);
  /*
    "hello "
    ""
    "world" cursor.column = 0
  */
  assert(frame.line->size == 6);
  assert(frame.line->next->size == 0);
  assert(frame.line->next->next->size == 5);
  assert(frame.cursor.column == 0);
  assert(frame.deleted_line == NULL);

  assert(frame.line->prev == NULL);
  assert(frame.line->next->prev == frame.line);
  assert(frame.line->next->next == frame.cursor.line);
  assert(frame.line->next->next->prev == frame.line->next);

  //----
  main_frame_remove_char(&frame);
  /*
    "hello "
    "world" cursor.column = 0
  */
  assert(frame.line->size == 6);
  assert(frame.line->next->size == 5);
  assert(frame.cursor.line == frame.line->next);
  assert(frame.cursor.column == 0);
  assert(frame.deleted_line != NULL);

  assert(frame.line->prev == NULL);
  assert(frame.line->next->prev == frame.line);
  assert(frame.line->next->next == NULL);
  assert(frame.line->next == frame.cursor.line);

  Line* deleted_line = frame.deleted_line;

  //----
  frame.cursor.column = 5;
  main_frame_insert_new_line(&arena, &frame);
  /*
    "hello "
    "world"
    "" cursor.column = 0
  */
  assert(frame.line->size == 6);
  assert(frame.line->next->size == 5);
  assert(frame.line->next->next->size == 0);
  assert(deleted_line == frame.cursor.line); // reusing deleted line
  assert(frame.cursor.column == 0);
  assert(frame.deleted_line == NULL);

  assert(frame.line->prev == NULL);
  assert(frame.line->next->prev == frame.line);
  assert(frame.line->next->next->prev == frame.line->next);
  assert(frame.line->next->next == frame.cursor.line);



  return 0;
}
