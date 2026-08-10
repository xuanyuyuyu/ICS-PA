#include <am.h>
#include <nemu.h>
#include <stdint.h>
#include <klib.h>
#define SYNC_ADDR (VGACTL_ADDR + 4)

void __am_gpu_init() {
 

}

void __am_gpu_config(AM_GPU_CONFIG_T *cfg) {
  printf("成功进入__am_gpu_config\n");
  uint32_t config = inl(VGACTL_ADDR);
  int width = config >> 16;
  int height = config & 0xffff;

  *cfg = (AM_GPU_CONFIG_T) {
    .present = true, .has_accel = false,
    .width = width, .height = height,
    .vmemsz = width * height * sizeof(uint32_t)
  };
}

void __am_gpu_fbdraw(AM_GPU_FBDRAW_T *ctl) {
  printf("成功进入__am_gpu_fbdraw\n");

  
  uint32_t config = inl(VGACTL_ADDR);
  int screen_w = config >> 16;
  int screen_h = config & 0xffff;

  assert(ctl->x >= 0 && ctl->y >= 0);
  assert(ctl->w >= 0 && ctl->h >= 0);
  assert(ctl->x + ctl->w <= screen_w);
  assert(ctl->y + ctl->h <= screen_h);

  volatile uint32_t *fb = (volatile uint32_t *)(uintptr_t)FB_ADDR;
  uint32_t *pixels = (uint32_t *)ctl->pixels;


  for (int y = 0; y < ctl->h; y++) {
    for (int x = 0; x < ctl->w; x++) {
      int dst = (ctl->y + y) * screen_w + (ctl->x + x);
      int src = y * ctl->w + x;

      fb[dst] = pixels[src];
    }
  }
  printf("fb[0]=%x\n", fb[0]);
  if (ctl->sync) {
    outl(SYNC_ADDR, 1);
  }
  printf(
  "draw x=%d y=%d w=%d h=%d first=%x\n",
  ctl->x,
  ctl->y,
  ctl->w,
  ctl->h,
  pixels[0]
  );


}

void __am_gpu_status(AM_GPU_STATUS_T *status) {
  status->ready = true;
}
