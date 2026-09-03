#include <proc.h>
#include <elf.h>
#include <stddef.h>
#include <stdint.h>
#include <fs.h>
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

    //移动到当前段在ELF文件中的位置
    assert(fs_lseek(fd, phdr.p_offset, SEEK_SET) != (size_t)-1);
    //加载到ELF指定的运行地址
    assert(fs_read(fd, (void *)(uintptr_t)phdr.p_vaddr, phdr.p_filesz) == phdr.p_filesz); 

    //.bss区域在ELF文件中不占空间，需要手动清零
    memset((void *)(uintptr_t)(phdr.p_vaddr + phdr.p_filesz),
        0,
        phdr.p_memsz - phdr.p_filesz
      );
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

