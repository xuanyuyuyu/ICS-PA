#ifndef __UTILS_FTRACE_H__
#define __UTILS_FTRACE_H__

#include <common.h>

void init_ftrace(const char *elf_file);
void ftrace_call(vaddr_t pc, vaddr_t target);
void ftrace_ret(vaddr_t pc, vaddr_t target);
const char *ftrace_find_function(vaddr_t addr);

#endif