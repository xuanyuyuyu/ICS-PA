#include <proc.h>
#include <elf.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __LP64__
# define Elf_Ehdr Elf64_Ehdr
# define Elf_Phdr Elf64_Phdr
#else
# define Elf_Ehdr Elf32_Ehdr
# define Elf_Phdr Elf32_Phdr
#endif

size_t ramdisk_read(void *buf, size_t offset, size_t len);



static uintptr_t loader(PCB *pcb, const char *filename) {
  Elf_Ehdr ehdr;

  //randisk只有dummy，从偏移0开始
  ramdisk_read(&ehdr, 0, sizeof(ehdr));

  assert(ehdr.e_ident[EI_MAG0] == ELFMAG0);
  assert(ehdr.e_ident[EI_MAG1] == ELFMAG1);
  assert(ehdr.e_ident[EI_MAG2] == ELFMAG2);
  assert(ehdr.e_ident[EI_MAG3] == ELFMAG3);

  for(int i = 0; i < ehdr.e_phnum; i ++) {
    Elf_Phdr phdr;

    size_t ph_offset = ehdr.e_phoff + i * ehdr.e_phentsize;

    ramdisk_read(&phdr, ph_offset, sizeof(phdr));
    //只加载PT_LOAD段
    if(phdr.p_type != PT_LOAD) {
      continue;
    }

    assert(phdr.p_memsz >= phdr.p_filesz);

    //将ELF文件中的段复制到它要求的虚拟地址
    ramdisk_read((void *)(uintptr_t)phdr.p_vaddr, phdr.p_offset, phdr.p_filesz);

    //.bss区域在ELF文件中不占空间，需要手动清零
    memset((void *)(uintptr_t)(phdr.p_vaddr + phdr.p_filesz),
        0,
        phdr.p_memsz - phdr.p_filesz
      );
  }
  return ehdr.e_entry;
}

void naive_uload(PCB *pcb, const char *filename) {
  uintptr_t entry = loader(pcb, filename);
  Log("Jump to entry = %p", entry);
  ((void(*)())entry) ();
}

