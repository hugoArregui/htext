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

  EditorBuffer buffer = eb_create(&arena);

  eb_insert_text(&buffer, "hello world");
  assert(buffer.line == buffer.cursor_line);
  assert(buffer.line->prev == NULL);
  assert(buffer.line->size == 11);
  assert(buffer.cursor_pos == 11);

  //----
  buffer.cursor_pos = 5;
  eb_insert_text(&buffer, ",");
  assert(buffer.line->size == 12);
  assert(buffer.line->prev == NULL);
  assert(line_eq(buffer.line, "hello, world"));
  assert(buffer.cursor_pos == 6);

  //----
  buffer.cursor_pos = 5;
  eb_remove_char(&buffer);
  assert(line_eq(buffer.line, "hello world"));
  assert(buffer.cursor_pos == 4);
  assert(buffer.line->prev == NULL);

  //----
  buffer.cursor_pos = 6;
  eb_new_line(&arena, &buffer);
  /*
    "hello "
    "world" cursor_pos = 0
  */
  assert(buffer.line->size == 6);
  assert(buffer.line->prev == NULL);
  assert(buffer.line->next == buffer.cursor_line);
  assert(buffer.line->next->prev == buffer.line);
  assert(buffer.cursor_line == buffer.line->next);
  assert(buffer.cursor_line->size == 5);
  assert(line_eq(buffer.line, "hello "));
  assert(line_eq(buffer.cursor_line, "world"));
  assert(buffer.cursor_pos == 0);
  assert(buffer.deleted_line == NULL);

  //----
  eb_new_line(&arena, &buffer);
  /*
    "hello "
    ""
    "world" cursor_pos = 0
  */
  assert(buffer.line->size == 6);
  assert(buffer.line->next->size == 0);
  assert(buffer.line->next->next->size == 5);
  assert(buffer.cursor_pos == 0);
  assert(buffer.deleted_line == NULL);

  assert(buffer.line->prev == NULL);
  assert(buffer.line->next->prev == buffer.line);
  assert(buffer.line->next->next == buffer.cursor_line);
  assert(buffer.line->next->next->prev == buffer.line->next);

  //----
  eb_remove_char(&buffer);
  /*
    "hello "
    "world" cursor_pos = 0
  */
  assert(buffer.line->size == 6);
  assert(buffer.line->next->size == 5);
  assert(buffer.cursor_line == buffer.line->next);
  assert(buffer.cursor_pos == 0);
  assert(buffer.deleted_line != NULL);

  assert(buffer.line->prev == NULL);
  assert(buffer.line->next->prev == buffer.line);
  assert(buffer.line->next->next == NULL);
  assert(buffer.line->next == buffer.cursor_line);

  Line* deleted_line = buffer.deleted_line;

  //----
  buffer.cursor_pos = 5;
  eb_new_line(&arena, &buffer);
  /*
    "hello "
    "world"
    "" cursor_pos = 0
  */
  assert(buffer.line->size == 6);
  assert(buffer.line->next->size == 5);
  assert(buffer.line->next->next->size == 0);
  assert(deleted_line == buffer.cursor_line); // reusing deleted line
  assert(buffer.cursor_pos == 0);
  assert(buffer.deleted_line == NULL);

  assert(buffer.line->prev == NULL);
  assert(buffer.line->next->prev == buffer.line);
  assert(buffer.line->next->next->prev == buffer.line->next);
  assert(buffer.line->next->next == buffer.cursor_line);



  return 0;
}
