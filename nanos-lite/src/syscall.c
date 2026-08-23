#include <common.h>
#include <stdint.h>
#include "amdev.h"
#include "syscall.h"
#include <fs.h>
#include <sys/time.h>
#ifdef CONFIG_STRACE
static const char *syscall_name[] = {
  [SYS_exit]         = "exit",
  [SYS_yield]        = "yield",
  [SYS_open]         = "open",
  [SYS_read]         = "read",
  [SYS_write]        = "write",
  [SYS_kill]         = "kill",
  [SYS_getpid]       = "getpid",
  [SYS_close]        = "close",
  [SYS_lseek]        = "lseek",
  [SYS_brk]          = "brk",
  [SYS_fstat]        = "fstat",
  [SYS_time]         = "time",
  [SYS_signal]       = "signal",
  [SYS_execve]       = "execve",
  [SYS_fork]         = "fork",
  [SYS_link]         = "link",
  [SYS_unlink]       = "unlink",
  [SYS_wait]         = "wait",
  [SYS_times]        = "times",
  [SYS_gettimeofday] = "gettimeofday",
};

static const char *get_syscall_name(uintptr_t id) {
  if (id < LENGTH(syscall_name) && syscall_name[id] != NULL) {
    return syscall_name[id];
  }

  return "unknown";
}

#ifdef __LP64__
# define STRACE_WORD_FMT "0x%lx"
# define STRACE_WORD_ARG(value) ((unsigned long)(value))
#else
# define STRACE_WORD_FMT "0x%x"
# define STRACE_WORD_ARG(value) ((unsigned int)(value))
#endif
#endif

static int sys_gettimeofday(struct timeval *tv, void *tz) {
  //当前不处理时区
  (void)tz;

  if(tv == NULL) {
    return -1;
  }
  AM_TIMER_UPTIME_T uptime = io_read(AM_TIMER_UPTIME);

  tv->tv_sec = uptime.us / 1000000;  //秒
  tv->tv_usec = uptime.us % 1000000;  //微秒
  return 0;
}



void do_syscall(Context *c) {
  uintptr_t a[4];
  a[0] = c->GPR1;  //系统调用号
  a[1] = c->GPR2;  //参数1
  a[2] = c->GPR3;  //参数2
  a[3] = c->GPR4;  //参数3

#ifdef CONFIG_STRACE
  const char *name = get_syscall_name(a[0]);
  printf(
    "[strace] %s(" STRACE_WORD_FMT ", " STRACE_WORD_FMT ", "
    STRACE_WORD_FMT ")\n",
    name,
    STRACE_WORD_ARG(a[1]),
    STRACE_WORD_ARG(a[2]),
    STRACE_WORD_ARG(a[3])
  );
#endif

  switch (a[0]) {
    case SYS_yield:
      yield();
      c->GPRx = 0;
      break;

    case SYS_exit:
      halt(0);
      break;

    case SYS_write: 
      c->GPRx = fs_write(
        (int)a[1],
        (const void *)a[2],
        (size_t)a[3]);
      break;

    case SYS_brk:
      c->GPRx = 0;
      break;

    case SYS_open:
      c->GPRx = fs_open(
        (const char *)a[1],
        (int)a[2],
        (int)a[3]
      );    
      break;
    
    case SYS_read:
      c->GPRx = fs_read(
        (int)a[1],
        (void *)a[2],
        (size_t)a[3]
      );
      break;

    case SYS_close:
      c->GPRx = fs_close((int)a[1]);
      break;
    
    case SYS_lseek:
      c->GPRx = fs_lseek((int)a[1], (size_t)a[2], (int)a[3]);
      break;
        
    case SYS_gettimeofday:
      c->GPRx = sys_gettimeofday((struct timeval *)a[1], (void *)a[2]);
      break;
      
    default: panic("Unhandled syscall ID = %d", a[0]);
  }

#ifdef CONFIG_STRACE
  printf(
    "[strace] %s = " STRACE_WORD_FMT "\n",
    name,
    STRACE_WORD_ARG(c->GPRx)
  );
#endif
}
