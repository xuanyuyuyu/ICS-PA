#include "sys/_default_fcntl.h"
#include "sys/types.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <fcntl.h>


static int evtdev = -1;
static int fbdev = -1;
static int screen_w = 0, screen_h = 0;

//获取程序运行到现在经过了多少毫秒
uint32_t NDL_GetTicks() {
  struct timeval tv;

  if(gettimeofday(&tv, NULL) != 0) {
    return 0;
  }

  uint64_t milliseconds = (uint64_t)tv.tv_sec * 1000 + (uint64_t)tv.tv_usec / 1000;

  return (uint32_t)milliseconds;
}

int NDL_PollEvent(char *buf, int len) {
  if(buf == NULL || len <= 1 || evtdev < 0) {
    return 0;
  }

  int nread = read(evtdev, buf, len);

  if(nread <= 0) {
    return 0;
  }

  if(nread < len) {
    buf[nread] = '\0';
  } else {
    buf[len - 1] = '\0';
  }
  return 1;
}

void NDL_OpenCanvas(int *w, int *h) {
  if (getenv("NWM_APP")) {
    int fbctl = 4;
    fbdev = 5;
    screen_w = *w; screen_h = *h;
    char buf[64];
    int len = sprintf(buf, "%d %d", screen_w, screen_h);
    // let NWM resize the window and create the frame buffer
    write(fbctl, buf, len);
    while (1) {
      // 3 = evtdev
      int nread = read(3, buf, sizeof(buf) - 1);
      if (nread <= 0) continue;
      buf[nread] = '\0';
      if (strcmp(buf, "mmap ok") == 0) break;
    }
    close(fbctl);
  }
}

void NDL_DrawRect(uint32_t *pixels, int x, int y, int w, int h) {
}

void NDL_OpenAudio(int freq, int channels, int samples) {
}

void NDL_CloseAudio() {
}

int NDL_PlayAudio(void *buf, int len) {
  return 0;
}

int NDL_QueryAudio() {
  return 0;
}

int NDL_Init(uint32_t flags) {
  (void)flags;
  if (getenv("NWM_APP")) {
    evtdev = 3;
  } else {
    evtdev = open("/dev/events", O_RDONLY);
    if(evtdev < 0) {
      return -1;
    }
  }
  return 0;
}

void NDL_Quit() {

  if(!getenv("NWM_APP") && evtdev >= 0) {
    close(evtdev);
  }
  evtdev = -1;
}
