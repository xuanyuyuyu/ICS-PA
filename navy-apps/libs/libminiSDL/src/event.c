#include <NDL.h>
#include <SDL.h>
#include <string.h>

#define keyname(k) #k,

static const char *keyname[] = {
  "NONE",
  _KEYS(keyname)
};

int SDL_PushEvent(SDL_Event *ev) {
  return 0;
}

int SDL_PollEvent(SDL_Event *ev) {
  if(ev == NULL) {
    return 0;
  }

  char buf[64];

  if(NDL_PollEvent(buf, sizeof(buf)) == 0) {
    return 0;
  }

  if(buf[0] == 'k' || (buf[1] != 'u' && buf[1] != 'd') || buf[2] != ' ') {
    return 0;
  }

  char *name = buf + 3;

  char *newline = strchr(name, '\n');
  if(newline != NULL) {
    *newline = '\0';
  }

  // 将字符串按键名转换成 SDLK_J、SDLK_DOWN 等 SDL 键码
  for (int i = 0; i < (int)(sizeof(keyname) /
  sizeof(keyname[0])); i++) {
    if (strcmp(name, keyname[i]) == 0) {
      ev->type = (buf[1] == 'd') ? SDL_KEYDOWN :
      SDL_KEYUP;
      ev->key.keysym.sym = i;
      return 1;
    }
  }


  return 0;
}

int SDL_WaitEvent(SDL_Event *event) {
  if(event == NULL) {
    return 0;
  }

  char buf[64];
  while(1) {
    if(NDL_PollEvent(buf, sizeof(buf)) == 0) {
      continue;
    }

    // 返回的应该为"kd J\n"或"ku DOWN\n"
    if(buf[0] != 'k' || (buf[1] != 'd' && buf[1] != 'u') || buf[2] != ' ') continue;
  
    char *name = buf + 3; //按键名

    //去掉结尾换行符
    char *newline = strchr(name, '\n');
    if(newline != NULL) {
      *newline = '\0';
    }
    for(int i = 0; i < (int)sizeof(keyname) / sizeof(keyname[0]); i ++) {
      if(strcmp(name, keyname[i]) == 0) {
        event->type = (buf[1] == 'd') ? SDL_KEYDOWN : SDL_KEYUP;
        event->key.keysym.sym = i;

        return 1;
      }
    }


  }
  
}

int SDL_PeepEvents(SDL_Event *ev, int numevents, int action, uint32_t mask) {
  return 0;
}

uint8_t* SDL_GetKeyState(int *numkeys) {
  return NULL;
}
