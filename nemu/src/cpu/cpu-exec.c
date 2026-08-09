/***************************************************************************************
* Copyright (c) 2014-2024 Zihao Yu, Nanjing University
*
* NEMU is licensed under Mulan PSL v2.
* You can use this software according to the terms and conditions of the Mulan PSL v2.
* You may obtain a copy of Mulan PSL v2 at:
*          http://license.coscl.org.cn/MulanPSL2
*
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
* EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
* MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
*
* See the Mulan PSL v2 for more details.
***************************************************************************************/

#include <cpu/cpu.h>
#include <cpu/decode.h>
#include <cpu/difftest.h>
#include <locale.h>
#include <monitor/watchpoint.h>

/* The assembly code of instructions executed is only output to the screen
 * when the number of instructions executed is less than this value.
 * This is useful when you use the `si' command.
 * You can modify this value as you want.
 */
#define MAX_INST_TO_PRINT 10

#ifdef CONFIG_ITRACE
//定义环型缓冲区
#define IRINGBUF_SIZE 32
static ITrace iringbuf[IRINGBUF_SIZE];
static size_t iringbuf_pos = 0;
static size_t iringbuf_count = 0;
#endif
CPU_state cpu = {};
uint64_t g_nr_guest_inst = 0;
static uint64_t g_timer = 0; // unit: us
static bool g_print_step = false;

void device_update();

static void trace_and_difftest(Decode *_this, vaddr_t dnpc) {
#ifdef CONFIG_ITRACE_COND
  if (ITRACE_COND) { log_write("%s\n", _this->logbuf); }
#endif
  if (g_print_step) { IFDEF(CONFIG_ITRACE, puts(_this->logbuf)); }
  IFDEF(CONFIG_DIFFTEST, difftest_step(_this->pc, dnpc));
  
}

#ifdef CONFIG_ITRACE
static void iringbuf_push(Decode *s) {
  iringbuf[iringbuf_pos].pc = s->pc;

  strcpy(iringbuf[iringbuf_pos].logbuf, s->logbuf);

  iringbuf_pos ++;

  if(iringbuf_pos >= IRINGBUF_SIZE)
    iringbuf_pos = 0;

  if(iringbuf_count < IRINGBUF_SIZE) 
    iringbuf_count ++;
}
#endif

/**
*  exec_once() 先执行一条机器指令并更新 cpu.pc，
*  再把“地址、机器码、汇编指令”拼接到 s->logbuf，
*  供 ITRACE 打印和调试使用。
*/
static void exec_once(Decode *s, vaddr_t pc) {
  s->pc = pc;
  s->snpc = pc;

  isa_exec_once(s);

  cpu.pc = s->dnpc;
#ifdef CONFIG_ITRACE
  char *p = s->logbuf; 
  p += snprintf(p, sizeof(s->logbuf), FMT_WORD ":", s->pc);
  int ilen = s->snpc - s->pc;  //指令长度
  int i;
  uint8_t *inst = (uint8_t *)&s->isa.inst;
#ifdef CONFIG_ISA_x86
  for (i = 0; i < ilen; i ++) {
#else
  for (i = ilen - 1; i >= 0; i --) {  //反向打印，小端
#endif
    p += snprintf(p, 4, " %02x", inst[i]);
  }
  int ilen_max = MUXDEF(CONFIG_ISA_x86, 8, 4);
  int space_len = ilen_max - ilen;
  if (space_len < 0) space_len = 0;
  space_len = space_len * 3 + 1;
  memset(p, ' ', space_len);
  p += space_len;

  void disassemble(char *str, int size, uint64_t pc, uint8_t *code, int nbyte);
  disassemble(p, s->logbuf + sizeof(s->logbuf) - p,
      MUXDEF(CONFIG_ISA_x86, s->snpc, s->pc), (uint8_t *)&s->isa.inst, ilen);
#endif
}

//若n是-1，被转换为无符号整数，是一个极大的数
static void execute(uint64_t n) {
  Decode s;  //创建一份指令译码信息，用来保存当前指令的内容
  for (;n > 0; n --) {
  
    exec_once(&s, cpu.pc);  //这里的cpu.pc表示从cpu.pc取指
    g_nr_guest_inst ++;  //统计已执行指令数量

    trace_and_difftest(&s, cpu.pc);  //s保存刚执行完的指令，cpu.pc代表下一条指令的地址

#ifdef CONFIG_ITRACE
    iringbuf_push(&s);   //实现环型缓冲区
#endif

    check_watchpoint();
    if (nemu_state.state != NEMU_RUNNING) break;
    IFDEF(CONFIG_DEVICE, device_update());
  }
}

static void statistic() {
  IFNDEF(CONFIG_TARGET_AM, setlocale(LC_NUMERIC, ""));
#define NUMBERIC_FMT MUXDEF(CONFIG_TARGET_AM, "%", "%'") PRIu64
  Log("host time spent = " NUMBERIC_FMT " us", g_timer);
  Log("total guest instructions = " NUMBERIC_FMT, g_nr_guest_inst);
  if (g_timer > 0) Log("simulation frequency = " NUMBERIC_FMT " inst/s", g_nr_guest_inst * 1000000 / g_timer);
  else Log("Finish running in less than 1 us and can not calculate the simulation frequency");
}

void assert_fail_msg() {
  isa_reg_display();
  statistic();
}


#ifdef CONFIG_ITRACE
static void iringbuf_display(void) {
  printf(" ------- instruction ring buffer -----\n");

  size_t start = (iringbuf_count < IRINGBUF_SIZE)
                   ? 0
                   : iringbuf_pos;

  for (size_t i = 0; i < iringbuf_count; i++) {
    size_t index = (start + i) % IRINGBUF_SIZE;

    printf("%s %s\n",
           iringbuf[index].pc == nemu_state.halt_pc ? "-->" : "   ",
           iringbuf[index].logbuf);
  }
}
#endif
/* Simulate how the CPU works. */
void cpu_exec(uint64_t n) {
  g_print_step = (n < MAX_INST_TO_PRINT);
  switch (nemu_state.state) {
    case NEMU_END: case NEMU_ABORT: case NEMU_QUIT:
      printf("Program execution has ended. To restart the program, exit NEMU and run again.\n");
      return;
    default: nemu_state.state = NEMU_RUNNING;
  }

  uint64_t timer_start = get_time();

  execute(n);

  uint64_t timer_end = get_time();
  g_timer += timer_end - timer_start;
  switch (nemu_state.state) {
    case NEMU_RUNNING: nemu_state.state = NEMU_STOP; break;

    case NEMU_END: 
      Log("nemu: %s at pc = " FMT_WORD,
          (nemu_state.state == NEMU_ABORT ? ANSI_FMT("ABORT", ANSI_FG_RED) :
           (nemu_state.halt_ret == 0 ? ANSI_FMT("HIT GOOD TRAP", ANSI_FG_GREEN) :
            ANSI_FMT("HIT BAD TRAP", ANSI_FG_RED))),
          nemu_state.halt_pc);
      break;
    
    case NEMU_ABORT:
#ifdef CONFIG_ITRACE
      iringbuf_display();
#endif
      Log("nemu: %s at pc = " FMT_WORD,
          (nemu_state.state == NEMU_ABORT ? ANSI_FMT("ABORT", ANSI_FG_RED) :
           (nemu_state.halt_ret == 0 ? ANSI_FMT("HIT GOOD TRAP", ANSI_FG_GREEN) :
            ANSI_FMT("HIT BAD TRAP", ANSI_FG_RED))),
          nemu_state.halt_pc);
      // fall through
    case NEMU_QUIT: statistic();
    // case NEMU_STOP: 
  }
}
