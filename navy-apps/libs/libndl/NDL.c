#include "sys/_default_fcntl.h"
#include "sys/types.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <fcntl.h>
#include <assert.h>

static int evtdev = -1;
static int fbdev = -1;
static int screen_w = 0, screen_h = 0;
static int canvas_w = 0;
static int canvas_h = 0;

static int read_screen_size(void) {
  int fd = open("/proc/dispinfo", O_RDONLY, 0);
  
  if(fd < 0) {
    return -1;
  }

  char buf[64];

  int nread = read(fd, buf, sizeof(buf) - 1);
  close(fd);

  if(nread <= 0) {
    return -1;
  }

  buf[nread] = '\0';

  //从 buf 里“读字符串”，再把解析出来的结果写到 screen_w 和 screen_h
  int ret = sscanf(buf, "WIDTH:%d\nHEIGHT:%d\n", &screen_w, &screen_h);

  if(ret != 2) {
    return -1;
  }

  return 0;
}



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
  if(w == NULL || h == NULL) {
    return;
  }
  if (getenv("NWM_APP")) {
    int fbctl = 4;
    fbdev = 5;
    canvas_w = *w; 
    canvas_h = *h;
    char buf[64];
    int len = sprintf(buf, "%d %d", canvas_w, canvas_h);
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
    return;
  }

  //画布宽度或高度为0时，使用对应的屏幕尺寸。
  //NDL_OpenCanvas(&w, &h)会通过指针把实际值返回调用者
  if(*w == 0) {
    *w = screen_w;
  }

  if(*h == 0) {
    *h = screen_h;
  }

  /*
   * 当前阶段不允许画布超过物理屏幕。
   */
  assert(*w > 0 && *h > 0);
  assert(*w <= screen_w);
  assert(*h <= screen_h);

  canvas_w = *w;
  canvas_h = *h;

}

void NDL_DrawRect(uint32_t *pixels, int x, int y, int w, int h) {
  if(pixels == NULL || fbdev < 0) {
    return;
  }
  if(w <= 0 || h <= 0) {
    return;
  }

  //绘制区域必须全部在画布内

  assert(x >= 0);
  assert(y >= 0);
  assert(x + w <= canvas_w);
  assert(y + h <= canvas_h);

  int nwm_mode = getenv("NWM_APP") != NULL;

  int target_width;
  int start_x;
  int start_y;

  if(nwm_mode) {
    //NWM模式下，fd 5本身就是当前窗口的画布。
    target_width = canvas_w;
    start_x = x;
    start_y = y;
  } else {
    //普通模式，画布居中放在整个屏幕中
    int canvas_offset_x = (screen_w - canvas_w) / 2;
    int canvas_offset_y = (screen_h - canvas_h) / 2;

    target_width = screen_w;
    start_x = canvas_offset_x + x;
    start_y = canvas_offset_y + y;
  }

  for (int row = 0; row < h; row ++) {
    off_t offset = ((start_y + row) * target_width + start_x) * sizeof(uint32_t);

    off_t result = lseek(fbdev, offset, SEEK_SET);

    assert(result != (off_t)-1);

    ssize_t written = write(fbdev, pixels + row * w, w * sizeof(uint32_t));

    assert(written == ((ssize_t)(w * sizeof(uint32_t))));
  }


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

    fbdev = open("/dev/fb", O_WRONLY, 0);

    if(fbdev < 0) {
      close(evtdev);
      evtdev = -1;
      return -1;
    }

  }
  if(read_screen_size() != 0) {
    if(!getenv("NWM_APP")) {
      if(evtdev >= 0) {
        close(evtdev);
      }

      if(fbdev >= 0) {
        close(fbdev);
      }
    }
    evtdev = -1;
    fbdev = -1;
    
    return -1;
  }


  return 0;
}

void NDL_Quit() {
  if (!getenv("NWM_APP")) {
    if (evtdev >= 0) {
      close(evtdev);
    }

    if (fbdev >= 0) {
      close(fbdev);
    }
  }

  evtdev = -1;
  fbdev = -1;
}