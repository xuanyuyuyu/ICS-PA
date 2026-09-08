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

#include "common.h"
#include <isa.h>

/**
* NO：异常原因编号
* epc：发生异常时的指令地址
* mepc：保存异常返回地址
* mcause： 保存异常原因
* mtvec：保存异常处理程序入口
*
* 这个函数就是保存异常信息，返回异常处理程序入口
*/
word_t isa_raise_intr(word_t NO, vaddr_t epc) {
  
#ifdef CONFIG_ETRACE
  log_write(
    "ETRACE: pc=" FMT_WORD
    " cause=" FMT_WORD
    " handler=" FMT_WORD "\n",
    epc, NO, cpu.mtvec
  );
#endif


  cpu.mepc = epc;  //中断结束后，回到哪里执行 
  cpu.mcause = NO;   //中断类型号


  /**
   *  MIE:当前是否允许 M 模式中断
   *  MPIE: 进入trap之前，MIE原来的值（中断开关的备份）
   * 
   */

  //取出进入trap之前的MIE
  word_t old_mie = (cpu.mstatus >> 3) & 1u;
  //先清除原来的MPIE
  cpu.mstatus &= ~(1u << 7);

  //把进入trap前的MIE保存到MPIE(bit 7)
  cpu.mstatus |= old_mie << 7; 
  //实现关中断
  cpu.mstatus &= ~(1u << 3);

  return cpu.mtvec;  //发生中断后，要跳到哪里处理
}

#define IRQ_TIMER 0x80000007u

//cpu每执行完一条指令，就问一次：现在有没有可以响应的时钟中断
word_t isa_query_intr() {
  if(cpu.INTR && (cpu.mstatus & (1u << 3))) {
    cpu.INTR = false;  //表示已经接受了这次中断请求，把pending状态清除，避免同一个中断被重复处理
    return IRQ_TIMER;  //返回0x80000007u
  }
  return INTR_EMPTY;  //若没有中断或屏蔽了中断
}
