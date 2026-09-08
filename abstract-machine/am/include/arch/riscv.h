#ifndef ARCH_H__
#define ARCH_H__

#ifdef __riscv_e
#define NR_REGS 16
#else
#define NR_REGS 32
#endif

struct Context {
  uintptr_t gpr[NR_REGS];
  uintptr_t mcause;  //异常类型编号
  uintptr_t mstatus;  //机器状态寄存器
  uintptr_t mepc;  //硬件自动填入触发异常那一条指令的 PC 地址。

  void *pdir;

  uintptr_t np;  //0: kernel, 1: user  表示Context恢复后，属于内核线程还是用户进程
};

#ifdef __riscv_e
#define GPR1 gpr[15] // a5
#else
#define GPR1 gpr[17] // a7
#endif

#define GPR2 gpr[10]  // a0: 参数1
#define GPR3 gpr[11]  // a3: 参数2
#define GPR4 gpr[12]  // a2：参数3
#define GPRx gpr[10]  // a0: 返回值

#endif
