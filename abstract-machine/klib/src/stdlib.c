#include <am.h>
#include <klib.h>
#include <klib-macros.h>
#include <stdint.h>

#if !defined(__ISA_NATIVE__) || defined(__NATIVE_USE_KLIB__)
static unsigned long int next = 1;

int rand(void) {
  // RAND_MAX assumed to be 32767
  next = next * 1103515245 + 12345;
  return (unsigned int)(next/65536) % 32768;
}

void srand(unsigned int seed) {
  next = seed;
}

int abs(int x) {
  return (x < 0 ? -x : x);
}

int atoi(const char* nptr) {
  int x = 0;
  while (*nptr == ' ') { nptr ++; }
  while (*nptr >= '0' && *nptr <= '9') {
    x = x * 10 + *nptr - '0';
    nptr ++;
  }
  return x;
}

static uintptr_t addr;
static bool initialized = false;

void *malloc(size_t size) {
  if(!initialized) {
    addr = ROUNDUP(heap.start, 8);  //8字节向上对齐
    initialized = true;
  }

  //malloc(0)也返回一块可区分的最小空间
  if(size == 0) {
    size = 1;
  }

  size_t aligned_size = ROUNDUP(size, 8);
  uintptr_t heap_end = (uintptr_t)heap.end;
  
  //先用减法判断，避免addr + aligned_size整数溢出
  assert(addr <= heap_end);
  assert(aligned_size <= heap_end - addr);

  void *result = (void *)addr;
  addr += aligned_size;

  return result;
}

void free(void *ptr) {
}

#endif
