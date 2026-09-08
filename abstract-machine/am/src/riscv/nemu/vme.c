#include <am.h>
#include <nemu.h>
#include <klib.h>
#include <stdint.h>

static AddrSpace kas = {};
static void* (*pgalloc_usr)(int) = NULL;
static void (*pgfree_usr)(void*) = NULL;
static int vme_enable = 0;

static Area segments[] = {      // Kernel memory mappings
  NEMU_PADDR_SPACE
};

#define USER_SPACE RANGE(0x40000000, 0x80000000)

static inline void set_satp(void *pdir) {
  uintptr_t mode = 1ul << (__riscv_xlen - 1);
  asm volatile("csrw satp, %0" : : "r"(mode | ((uintptr_t)pdir >> 12)));
}

static inline uintptr_t get_satp() {
  uintptr_t satp;
  asm volatile("csrr %0, satp" : "=r"(satp));
  return satp << 12;
}

bool vme_init(void* (*pgalloc_f)(int), void (*pgfree_f)(void*)) {
  pgalloc_usr = pgalloc_f;
  pgfree_usr = pgfree_f;

  kas.ptr = pgalloc_f(PGSIZE);

  int i;
  for (i = 0; i < LENGTH(segments); i ++) {
    void *va = segments[i].start;
    for (; va < segments[i].end; va += PGSIZE) {
      map(&kas, va, va, 0);
    }
  }

  set_satp(kas.ptr);
  vme_enable = 1;

  return true;
}

void protect(AddrSpace *as) {
  PTE *updir = (PTE*)(pgalloc_usr(PGSIZE));
  as->ptr = updir;
  as->area = USER_SPACE;
  as->pgsize = PGSIZE;
  // map kernel space
  memcpy(updir, kas.ptr, PGSIZE);
}

void unprotect(AddrSpace *as) {
}

//把当前satp中的根页表的地址保存到context中
void __am_get_cur_as(Context *c) {
  c->pdir = (vme_enable ? (void *)get_satp() : NULL);
}

//将即将运行的进程的根页表写进satp
void __am_switch(Context *c) {
  if (vme_enable && c->pdir != NULL) {
    set_satp(c->pdir);
  }
}
//建立一条虚拟地址到物理地址的映射
void map(AddrSpace *as, void *va, void *pa, int prot) {
  uintptr_t v = (uintptr_t)va;
  uintptr_t p = (uintptr_t)pa;

  // 4KB 页时，低 12 位必须全为 0。
  assert((v & (PGSIZE - 1)) == 0);
  assert((p & (PGSIZE - 1)) == 0);

  //把地址空间中的“根页表地址”取出来  as是当前进程的AddrSpace，as->ptr保存这个进程的根页表的起始地址
  PTE *pdir = (PTE *)as->ptr;

  //用于索引一级页表
  int vpn1 = (v >> 22) & 0x3ff;
  //用于索引二级页表
  int vpn0 = (v >> 12) & 0x3ff;

  PTE *pt;
  //查看一级页表是否有效，无效则意味着这个区域还没有对应的二级页表
  if(!(pdir[vpn1] & PTE_V)) {
    //申请一页，作为新的二级页表
    //一共有4096 / sizeof(PTE)个页表项
    pt = pgalloc_usr(PGSIZE);
    //将二级页表的物理页号，写入一级页表项
    pdir[vpn1] = ((uintptr_t)pt >> 12 << 10) | PTE_V;
  } else {
    //一级页表项已经存在，从中取出二级页表的物理页号，再左移12位，恢复为二级页表的实际物理地址
    pt = (PTE *)((pdir[vpn1] >> 10) << 12);
  }
  // 在二级页表中填写最终映射：
  //
  // pt[vpn0] 对应虚拟页 va；
  // 写入 pa 的物理页号，以及该页的访问权限。
  pt[vpn0] = ((p >> 12) << 10) |
               PTE_V |  // 映射有效
               PTE_R |  // 可读
               PTE_W |  // 可写
               PTE_X;   // 可执行
}

Context *ucontext(AddrSpace *as, Area kstack, void *entry) {
  Context *c = (Context *)((uintptr_t)kstack.end - sizeof(Context));

  memset(c, 0, sizeof(Context));

  c->mepc = (uintptr_t)entry;

  c->pdir = as->ptr;

  c->mstatus = 0x1880;

  return c;
}
