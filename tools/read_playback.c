#include <SDL2/SDL.h>
#include <SDL2/SDL_events.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>

int main(void) {
  FILE *f = fopen("playback", "r");
  if (!f) {
    return -1;
  }

  SDL_Event event;
  int16_t read_size = fread(&event, sizeof(SDL_Event), 1, f);
  while(read_size == 1) {
    switch (event.type) {
    case SDL_KEYDOWN: {
      switch (event.key.keysym.scancode) {
      case SDL_SCANCODE_BACKSPACE: {
      printf("keydown: backspace\n");
      } break;
      case SDL_SCANCODE_ESCAPE: {
      printf("keydown: escape\n");
      } break;
      case SDL_SCANCODE_RETURN: {
      printf("keydown: return\n");
      } break;
      default: {
      } break;
      }
    } break;
    case SDL_TEXTINPUT: {
      printf("text input: %s\n", event.text.text);
    } break;
    }

   read_size = fread(&event, sizeof(SDL_Event), 1, f);
  }

  fclose(f);

  return 0;
}
