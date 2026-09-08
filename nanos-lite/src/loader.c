#include "klib-macros.h"
#include <proc.h>
#include <elf.h>
#include <stddef.h>
#include <stdint.h>
#include <fs.h>
#include <memory.h>
#ifdef __LP64__
# define Elf_Ehdr Elf64_Ehdr
# define Elf_Phdr Elf64_Phdr
#else
# define Elf_Ehdr Elf32_Ehdr
# define Elf_Phdr Elf32_Phdr
#endif

#ifdef __LP64__
# define Elf_Ehdr Elf64_Ehdr
# define Elf_Phdr Elf64_Phdr
#else
# define Elf_Ehdr Elf32_Ehdr
# define Elf_Phdr Elf32_Phdr
#endif

#if defined(__ISA_AM_NATIVE__)
# define EXPECT_TYPE EM_X86_64
#elif defined(__ISA_X86__)
# define EXPECT_TYPE EM_386
#elif defined(__ISA_MIPS32__)
# define EXPECT_TYPE EM_MIPS
#elif defined(__ISA_RISCV32__) || defined(__ISA_RISCV64__)
# define EXPECT_TYPE EM_RISCV
#elif defined(__ISA_LOONGARCH32R__)
# define EXPECT_TYPE EM_LOONGARCH
#else
# error Unsupported ISA
#endif

size_t ramdisk_read(void *buf, size_t offset, size_t len);



static uintptr_t loader(PCB *pcb, const char *filename) {
  Elf_Ehdr ehdr;

  int fd = fs_open(filename, 0, 0);
  assert(fd >= 0);

  size_t ret = fs_read(fd, &ehdr, sizeof(ehdr));
  //randisk只有dummy，从偏移0开始
  assert(ret == sizeof(ehdr));

  assert(ehdr.e_ident[EI_MAG0] == ELFMAG0);
  assert(ehdr.e_ident[EI_MAG1] == ELFMAG1);
  assert(ehdr.e_ident[EI_MAG2] == ELFMAG2);
  assert(ehdr.e_ident[EI_MAG3] == ELFMAG3);
  assert(ehdr.e_machine == EXPECT_TYPE);
  
  for(int i = 0; i < ehdr.e_phnum; i ++) {
    Elf_Phdr phdr;

    size_t ph_offset = ehdr.e_phoff + i * ehdr.e_phentsize;

    assert(fs_lseek(fd, ph_offset, SEEK_SET) != (size_t)-1);
    assert(fs_read(fd, &phdr, sizeof(phdr)) == sizeof(phdr));

    //只加载PT_LOAD段
    if(phdr.p_type != PT_LOAD) {
      continue;
    }

    assert(phdr.p_memsz >= phdr.p_filesz);

    // 空段不需要分配物理页。
    if (phdr.p_memsz == 0) {
      continue;
    }

    // 这个 PT_LOAD 段在用户虚拟地址空间中的范围：
    //   文件内容：[seg_start, file_end)
    //   内存内容：[seg_start, mem_end)
    // file_end 到 mem_end 之间是 ELF 文件中不占空间的 BSS。
    uintptr_t seg_start = (uintptr_t)phdr.p_vaddr;
    uintptr_t file_end = seg_start + (uintptr_t)phdr.p_filesz;
    uintptr_t mem_end = seg_start + (uintptr_t)phdr.p_memsz;

    assert(file_end >= seg_start);
    assert(mem_end >= seg_start);

    assert(seg_start >= (uintptr_t)pcb->as.area.start);
    assert(mem_end <= (uintptr_t)pcb->as.area.end);

    //记录最高地址
    if(mem_end > pcb->max_brk) {
      pcb->max_brk = mem_end;
    }

    // 页表只能按整页建立映射，因此需要把段覆盖的首尾地址
    // 分别向下、向上对齐到页边界。
    uintptr_t page_va = ROUNDDOWN(seg_start, PGSIZE);
    uintptr_t page_end = ROUNDUP(mem_end, PGSIZE);

    // AM 没有单独的 MMAP_EXEC 标志，因此可执行段也按可读段处理。
    int prot = MMAP_NONE;
    if (phdr.p_flags & (PF_R | PF_X)) {
      prot |= MMAP_READ;
    }
    if (phdr.p_flags & PF_W) {
      prot |= MMAP_WRITE;
    }

    // 为该段覆盖的每一个虚拟页分配物理页，并逐页装载。
    for (; page_va < page_end; page_va += PGSIZE) {
      uint8_t *page_pa = (uint8_t *)new_page(1);

      // 先清零整页：没有被文件内容覆盖的部分（包括 BSS）
      // 会自然保持为 0。
      memset(page_pa, 0, PGSIZE);

      // 建立“用户虚拟页 page_va -> 物理页 page_pa”的映射。
      map(&pcb->as, (void *)page_va, page_pa, prot);

      // 计算当前页面与文件内容区间 [seg_start, file_end) 的交集。
      uintptr_t page_limit = page_va + PGSIZE;
      uintptr_t copy_start = page_va > seg_start ? page_va : seg_start;
      uintptr_t copy_end = page_limit < file_end ? page_limit : file_end;

      // 纯 BSS 页面与文件内容没有交集，只需保持清零即可。
      if (copy_start < copy_end) {
        size_t file_offset = (size_t)phdr.p_offset
                           + (size_t)(copy_start - seg_start);
        size_t page_offset = (size_t)(copy_start - page_va);
        size_t copy_len = (size_t)(copy_end - copy_start);

        assert(fs_lseek(fd, file_offset, SEEK_SET) != (size_t)-1);

        // loader 当前运行在内核地址空间中，所以必须写入物理页，
        // 不能把用户虚拟地址 page_va 直接当作目标指针。
        assert(fs_read(fd, page_pa + page_offset, copy_len) == copy_len);
      }
    }
  }

  fs_close(fd);
  return ehdr.e_entry;
}

void naive_uload(PCB *pcb, const char *filename) {
  uintptr_t entry = loader(pcb, filename);
  // KLIB 暂不支持 %p，使用 %x
    Log("Loading %s, entry = 0x%x", filename, (uint32_t)entry);

  
  ((void(*)())entry) ();  // PC = entry;
}


void context_uload(PCB *pcb, const char *filename, char *const argv[], char *const envp[]) {
  protect(&pcb->as);

  pcb->max_brk = 0; //可能复用当前PCB，需要重新清零
  
  uintptr_t entry = loader(pcb, filename);

  Log("Loading %s, entry = 0x%x", filename, (uint32_t)entry);

  pcb->cp = ucontext(&pcb->as, RANGE(pcb->stack, pcb->stack + STACK_SIZE), (void *)entry);
  
  int argc = 0;
  while (argv != NULL && argv[argc] != NULL) {
    argc++;
  }

  int envc = 0;
  while (envp != NULL && envp[envc] != NULL) {
    envc++;
  }

  //为新用户程序申请8页 = 32KB
  uint8_t *ustack_pa = (uint8_t *)new_page(8);
  memset(ustack_pa, 0, 8 * PGSIZE);

  //用户栈位于用户虚拟地址空间的最高32KB
  uintptr_t ustack_va = (uintptr_t)pcb->as.area.end - 8 * PGSIZE;

  for(int i = 0; i < 8; i ++){
    map(&pcb->as, (void *)(ustack_va + i * PGSIZE), ustack_pa + i * PGSIZE, MMAP_READ | MMAP_WRITE);
  }

  size_t string_bytes = 0;
  for (int i = 0; i < argc; i++) {
    string_bytes += strlen(argv[i]) + 1;
  }
  for (int i = 0; i < envc; i++) {
    string_bytes += strlen(envp[i]) + 1;
  }

  uintptr_t stack_bottom_pa = (uintptr_t)ustack_pa;
  uintptr_t stack_top_pa = stack_bottom_pa + 8 * PGSIZE;

  // 字符串放在栈的最高处，指针数组和启动参数放在其下方。
  uintptr_t strings_bottom_pa = stack_top_pa - string_bytes;
  uintptr_t arrays_top_pa = ROUNDDOWN(strings_bottom_pa, sizeof(uintptr_t));

  uintptr_t *envp_pa = (uintptr_t *)(arrays_top_pa
      - (envc + 1) * sizeof(uintptr_t));
  uintptr_t *argv_pa = envp_pa - (argc + 1);

  // _start 会把 a0 直接作为 sp，因此保证初始栈 16 字节对齐。
  uintptr_t *args_pa = (uintptr_t *)ROUNDDOWN(
      (uintptr_t)argv_pa - 3 * sizeof(uintptr_t), 16);

  assert((uintptr_t)args_pa >= stack_bottom_pa);

  uintptr_t string_cursor_pa = stack_top_pa;

  for (int i = 0; i < argc; i++) {
    size_t len = strlen(argv[i]) + 1;
    string_cursor_pa -= len;
    memcpy((void *)string_cursor_pa, argv[i], len);
    argv_pa[i] = ustack_va + (string_cursor_pa - stack_bottom_pa);
  }
  argv_pa[argc] = 0;

  for (int i = 0; i < envc; i++) {
    size_t len = strlen(envp[i]) + 1;
    string_cursor_pa -= len;
    memcpy((void *)string_cursor_pa, envp[i], len);
    envp_pa[i] = ustack_va + (string_cursor_pa - stack_bottom_pa);
  }
  envp_pa[envc] = 0;

  uintptr_t argv_va = ustack_va
      + ((uintptr_t)argv_pa - stack_bottom_pa);
  uintptr_t envp_va = ustack_va
      + ((uintptr_t)envp_pa - stack_bottom_pa);
  uintptr_t args_va = ustack_va
      + ((uintptr_t)args_pa - stack_bottom_pa);

  args_pa[0] = argc;
  args_pa[1] = argv_va;
  args_pa[2] = envp_va;

  // a0 是 _start 的参数；同时初始化 sp，避免首条用户指令前到来的中断看到无效栈。
  pcb->cp->GPRx = args_va;
  pcb->cp->gpr[2] = args_va;

}
