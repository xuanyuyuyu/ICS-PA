#include <am.h>
#include <riscv/riscv.h>
#include <klib.h>
#include <stdint.h>

static Context* (*user_handler)(Event, Context*) = NULL;

void __am_get_cur_as(Context *c);
void __am_switch(Context *c);


Context* __am_irq_handle(Context *c) {

  //记录发生陷阱时，cpu当前使用的根页表 该根页表地址会保存在c->pdir
  __am_get_cur_as(c);

  if (user_handler) {
    Event ev = {0};
    switch (c->mcause) {
      case 0x80000007u:
        ev.event = EVENT_IRQ_TIMER;
        break;
      case 11:
        if((intptr_t)c->GPR1 == -1) {
          ev.event = EVENT_YIELD;
        } else {
          ev.event = EVENT_SYSCALL;
        }

        //跳过ecall指令
        c->mepc += 4;
        break;

      default: ev.event = EVENT_ERROR; break;
    }
    //可能调用schedule
    c = user_handler(ev, c);
    assert(c != NULL);
  }

  //读取即将恢复的Context中的c->pdir，并写入CPU的satp
  __am_switch(c);
  return c;
}

extern void __am_asm_trap(void);

bool cte_init(Context*(*handler)(Event, Context*)) {
  // initialize exception entry
  asm volatile("csrw mtvec, %0" : : "r"(__am_asm_trap));

  // register event handler
  user_handler = handler;

  return true;
}

//为一个“还从没运行过的新线程”，伪造一份初始CPU现场
Context *kcontext(Area kstack, void (*entry)(void *), void *arg) {

  // c 就是指向这份新线程初始现场的指针。  end为高地址
  Context *c = (Context *)((uintptr_t)kstack.end - sizeof(Context));
  
  //寄存器初值设置为0
  memset(c, 0, sizeof(Context));

  //entry为新线程要运行的函数，mepc为恢复后继续执行的地址，这一行决定“新线程从哪里开始执行”
  c->mepc = (uintptr_t)entry;

  //x2是sp，新进程使用自己的栈顶
  c->gpr[2] = (uintptr_t)kstack.end;

  c->gpr[10] = (uintptr_t)arg;  //x10即a0
  //mret后仍运行在M-mode
  c->mstatus = 0x1880;

  (void)arg;

  return c;
}


void yield() {
#ifdef __riscv_e
  asm volatile("li a5, -1; ecall");
#else
  asm volatile("li a7, -1; ecall");
#endif
}

bool ienabled() {
  return false;
}

void iset(bool enable) {
}






