#include "klib-macros.h"
#include <fs.h>
#include <stdint.h>

typedef size_t (*ReadFn) (void *buf, size_t offset, size_t len);
typedef size_t (*WriteFn) (const void *buf, size_t offset, size_t len);

typedef struct {
  char *name;
  size_t size;
  size_t disk_offset;
  ReadFn read;
  WriteFn write;
  size_t open_offset;
} Finfo;

enum {FD_STDIN, FD_STDOUT, FD_STDERR, FD_FB};

size_t ramdisk_read(void *buf, size_t offset, size_t len);
size_t ramdisk_write(const void *buf, size_t offset, size_t len);
size_t serial_write(const void *buf, size_t offset, size_t len);

size_t invalid_read(void *buf, size_t offset, size_t len) {
  panic("should not reach here");
  return 0;
}

size_t invalid_write(const void *buf, size_t offset, size_t len) {
  panic("should not reach here");
  return 0;
}

/* This is the information about all files in disk. */
static Finfo file_table[] __attribute__((used)) = {
  [FD_STDIN]  = {"stdin", 0, 0, invalid_read, invalid_write},
  [FD_STDOUT] = {"stdout", 0, 0, invalid_read, serial_write},
  [FD_STDERR] = {"stderr", 0, 0, invalid_read, serial_write},
#include "files.h"
};

void init_fs() {
  // TODO: initialize the size of /dev/fb
}


int fs_open(const char *pathname, int flags, int mode) {
  (void)flags;
  (void)mode;

  for(size_t i = 0; i < LENGTH(file_table); i ++) {
    if(strcmp(pathname, file_table[i].name) == 0) {
      file_table[i].open_offset = 0;
      return i;  //返回数组下标作为fd（file_table中的下标）
    }
  }
  return -1;
}

size_t fs_read(int fd, void *buf, size_t len) {
  assert(fd >= 0 && fd < LENGTH(file_table));

  Finfo *file = &file_table[fd];
  size_t ret;

  if(file->read != NULL) {
    //设备文件使用自己的读取函数
    ret = file->read(buf, file->open_offset, len);         
  } else {
    //普通文件不能读取超过文件末尾
    if(file->open_offset >= file->size) {
      return 0;
    }

    //请求读取的数据超过文件剩余空间，但是还有一部分数据可以返回。
    size_t remain = file->size - file->open_offset;
    if(len > remain) {
      len = remain;
    }
    //offset = 文件在整个 ramdisk 的位置 + 文件内部的位置
    ret = ramdisk_read(buf, file->disk_offset + file->open_offset, len);
  }

  file->open_offset += ret;
  return ret;
}

size_t fs_lseek(int fd, size_t offset, int whence) {
  assert(fd >= 0 && fd < LENGTH(file_table));

  Finfo *file = &file_table[fd];
  intptr_t base;

  switch(whence) {
    //文件开头
    case SEEK_SET:
      base = 0;
      break;

    //当前位置
    case SEEK_CUR:
      base = (intptr_t)file->open_offset;
      break;

    //文件末尾
    case SEEK_END:
      base = (intptr_t)file->size;
      break;

    default:
      return (size_t) - 1;
  }
 
  intptr_t new_offset = base + (intptr_t)offset;

  if(new_offset < 0 || (size_t)new_offset > file->size) {
    return (size_t)-1;
  }
  file->open_offset = (size_t)new_offset;
  return file->open_offset;
}

int fs_close(int fd) {
  if(fd < 0 || fd >= LENGTH(file_table)) {
    return -1;
  }
  file_table[fd].open_offset = 0;
  return 0;
}

size_t fs_write(int fd, const void *buf, size_t len) {
  assert(fd >= 0 && fd < LENGTH(file_table)); 

  Finfo *file = &file_table[fd];
  size_t ret;

  if(file->write != NULL) {
    //设备文件使用自己的函数
    ret = file->write(buf, file->open_offset, len);
  } else {
    //普通文件不能写过文件末尾
    if(file->open_offset >= file->size) {
      return 0;
    }

    size_t remain = file->size - file->open_offset;
    if(len > remain) {
      len = remain;  //写到文件末尾
    }

    ret = ramdisk_write(buf, file->disk_offset + file->open_offset, len);
  }
  file->open_offset += ret;
  return ret;
}

