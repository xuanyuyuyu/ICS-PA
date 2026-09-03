#include <NDL.h>
#include <stdint.h>

extern uint32_t sdl_init_ticks;


int SDL_Init(uint32_t flags) {
  int ret = NDL_Init(flags);

  if(ret == 0) {
    sdl_init_ticks = NDL_GetTicks(); 
  }
  return ret;
}

void SDL_Quit() {
  NDL_Quit();
}

char *SDL_GetError() {
  return "Navy does not support SDL_GetError()";
}

int SDL_SetError(const char* fmt, ...) {
  return -1;
}

int SDL_ShowCursor(int toggle) {
  return 0;
}

void SDL_WM_SetCaption(const char *title, const char *icon) {
}
