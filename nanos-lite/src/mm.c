#include "klib-macros.h"
#include <memory.h>
#include <proc.h>
#include <stdint.h>
static void *pf = NULL;

void* new_page(size_t nr_page) {
  void *p = pf;

  assert((uintptr_t)pf + nr_page * PGSIZE <= (uintptr_t)heap.end);

  pf = (void *)((uintptr_t)pf + nr_page * PGSIZE);  //pf指向下一块可分配的空闲物理页的起始地址

  return p;
}

#ifdef HAS_VME
//申请页表
static void* pg_alloc(int n) {
  assert(n % PGSIZE == 0) ;
  void *p = new_page(n / PGSIZE);
  memset(p, 0, n);
  return p;
}
#endif

void free_page(void *p) {
  panic("not implement yet");
}

/* The brk() system call handler. */
int mm_brk(uintptr_t brk) {
  //brk没有超过历史最高位时，不需要申请新页面
  if(brk <= current->max_brk) {
    return 0;
  }

  uintptr_t map_start = ROUNDUP(current->max_brk, PGSIZE);

  uintptr_t map_end = ROUNDUP(brk, PGSIZE);

  for(uintptr_t va = map_start; va < map_end; va += PGSIZE) {
    void *pa = new_page(1);

    memset(pa, 0, PGSIZE);

    map(&current->as, (void *)va, pa, MMAP_READ | MMAP_WRITE);

  }
  current->max_brk = brk; //更新最高地址
  return 0;
}

void init_mm() {
  pf = (void *)ROUNDUP(heap.start, PGSIZE);
  Log("free physical pages starting from %p", pf);

#ifdef HAS_VME
  vme_init(pg_alloc, free_page);
#endif
}
