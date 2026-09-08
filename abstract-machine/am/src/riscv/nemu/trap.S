#define concat_temp(x, y) x ## y
#define concat(x, y) concat_temp(x, y)
#define MAP(c, f) c(f)

#if __riscv_xlen == 32
#define LOAD  lw
#define STORE sw
#define XLEN  4
#else
#define LOAD  ld
#define STORE sd
#define XLEN  8
#endif

#define REGS_LO16(f) \
      f( 1)       f( 3) f( 4) f( 5) f( 6) f( 7) f( 8) f( 9) \
f(10) f(11) f(12) f(13) f(14) f(15)
#ifndef __riscv_e
#define REGS_HI16(f) \
                                    f(16) f(17) f(18) f(19) \
f(20) f(21) f(22) f(23) f(24) f(25) f(26) f(27) f(28) f(29) \
f(30) f(31)
#define NR_REGS 32
#else
#define REGS_HI16(f)
#define NR_REGS 16
#endif

#define REGS(f) REGS_LO16(f) REGS_HI16(f)

#define PUSH(n) STORE concat(x, n), (n * XLEN)(sp);
#define POP(n)  LOAD  concat(x, n), (n * XLEN)(sp);

#define CONTEXT_SIZE  ((NR_REGS + 3 + 1 + 1) * XLEN)
#define OFFSET_NP     ((NR_REGS + 4) * XLEN)
#define OFFSET_SP     ( 2 * XLEN)
#define OFFSET_CAUSE  ((NR_REGS + 0) * XLEN)
#define OFFSET_STATUS ((NR_REGS + 1) * XLEN)
#define OFFSET_EPC    ((NR_REGS + 2) * XLEN)

.align 3
.globl __am_asm_trap
__am_asm_trap: 
  # 交换sp和mscratch
  # 来自用户态：sp得到内核栈，mscratch保存用户sp
  # 来自内核态：sp得到0， mscratch保存原内核sp
  csrrw sp, mscratch, sp
  bnez sp, 1f

  # 来自内核态，恢复原来的内核sp
  csrr sp, mscratch
  addi sp, sp, -CONTEXT_SIZE
  MAP(REGS, PUSH)
  li t0, 0
  j 2f

1:
  # 来自用户态，此时sp已经指向内核栈 
  addi sp, sp, -CONTEXT_SIZE
  MAP(REGS, PUSH)
  li t0, 1

2:
  # 保存该Context将返回到哪里
  STORE t0, OFFSET_NP(sp)  # context->np = t0
  
  # mscratch保存的是陷入前的原始sp
  csrr t0, mscratch
  STORE t0, OFFSET_SP(sp)

  # 进入CTE后视为内核态，支持CTE重入
  csrw mscratch, zero

  csrr t0, mcause
  csrr t1, mstatus
  csrr t2, mepc

  STORE t0, OFFSET_CAUSE(sp)
  STORE t1, OFFSET_STATUS(sp)
  STORE t2, OFFSET_EPC(sp)

  # set mstatus.MPRV to pass difftest
  li a0, (1 << 17)
  or t1, t1, a0
  csrw mstatus, t1

  # a0是第一个参数 a0 = sp
  mv a0, sp  
  call __am_irq_handle

  # a0 是 __am_irq_handle() 返回的 Context *
  # 切换 sp 后，后续恢复的是“新线程”的上下文
  mv sp, a0

  LOAD t1, OFFSET_STATUS(sp)
  LOAD t2, OFFSET_EPC(sp)
  csrw mstatus, t1
  csrw mepc, t2

  #若将返回用户态，记录该进程的内核栈栈顶
  LOAD t0, OFFSET_NP(sp)
  beqz t0, 3f  # np == 0 返回内核态，直接跳到3
  
  addi t0, sp, CONTEXT_SIZE
  csrw mscratch, t0

3:
  MAP(REGS, POP)

  LOAD sp, OFFSET_SP(sp) # 恢复栈指针

  mret