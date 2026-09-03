#define SDL_malloc  malloc
#define SDL_free    free
#define SDL_realloc realloc

#define SDL_STBIMAGE_IMPLEMENTATION
#include "SDL_stbimage.h"
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>


SDL_Surface* IMG_Load_RW(SDL_RWops *src, int freesrc) {
  assert(src->type == RW_TYPE_MEM);
  assert(freesrc == 0);
  return NULL;
}

SDL_Surface* IMG_Load(const char *filename) {
  FILE *fp = fopen(filename, "rb");
  if(fp == NULL) {
    return NULL;
  }
  //移动到文件末尾，用ftell获得文件大小
  if(fseek(fp, 0, SEEK_END) != 0) {
    fclose(fp);
    return NULL;
  }

  long file_size = ftell(fp);  //ftell返回当前位置相对于文件开头的偏移量
  if (file_size <= 0 || file_size > INT_MAX) {
    return NULL;
  }
  //回到文件开头，准备读取文件
  if(fseek(fp, 0, SEEK_SET) != 0) {
    fclose(fp);
    return NULL;
  }

  unsigned char *buf = (unsigned char *)malloc(file_size);
  if(buf == NULL) {
    fclose(fp);
    return NULL;
  }

  //将整个文件读入buf
  size_t nread = fread(buf, 1, file_size, fp);
  fclose(fp);

  //文件未能完整读出
  if(nread != (size_t)file_size) {
    free(buf);
    return NULL;
  }

  SDL_Surface *surface = STBIMG_LoadFromMemory(buf, (int)file_size);

  free(buf);

  return surface;
}

int IMG_isPNG(SDL_RWops *src) {
  return 0;
}

SDL_Surface* IMG_LoadJPG_RW(SDL_RWops *src) {
  return IMG_Load_RW(src, 0);
}

char *IMG_GetError() {
  return "Navy does not support IMG_GetError()";
}
