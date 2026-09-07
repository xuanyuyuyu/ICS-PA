#include <memory.h>

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
  return 0;
}

void init_mm() {
  pf = (void *)ROUNDUP(heap.start, PGSIZE);
  Log("free physical pages starting from %p", pf);

#ifdef HAS_VME
  vme_init(pg_alloc, free_page);
#endif
}
