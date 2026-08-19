#include <common.h>
#include "syscall.h"

/*
ecall 触发异常，使 CPU 进入 trap.S；trap.S 保存现场并调用 __am_irq_handle()；
CTE 将硬件异常转换成 Event 后调用已注册的 user_handler，
当前就是 nanos-lite 的 do_event()；
最后 do_event() 根据事件类型分发给相应的事件处理函数。
*/

static Context* do_event(Event e, Context* c) {
  switch (e.event) {
    case EVENT_YIELD :
      printf("EVENT_YIELD is trapped\n");
      break;
    case EVENT_SYSCALL:
      do_syscall(c);
      break;
    default: panic("Unhandled event ID = %d", e.event);
  }

  return c;
}

void init_irq(void) {
  Log("Initializing interrupt/exception handler...");
  cte_init(do_event);
}
