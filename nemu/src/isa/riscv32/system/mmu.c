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

#include <isa.h>
#include <memory/vaddr.h>
#include <memory/paddr.h>

typedef word_t PTE;
#define PTE_V (1u << 0)

//将虚拟地址vaddr翻译为物理地址
paddr_t isa_mmu_translate(vaddr_t vaddr, int len, int type) {
  //一次内存访问不能跨越两个页
  //页内偏移 + 访问长度必须不超过一页大小
  assert((vaddr & PAGE_MASK) + len <= PAGE_SIZE);
  //获得vpn1
  uint32_t vpn1 = (vaddr >> 22) & 0x3ff;
  //获得vpn0
  uint32_t vpn0 = (vaddr >> 12) & 0x3ff;

  //offset：虚拟地址在该4KB页内的偏移，翻译前后保持不变
  uint32_t offset = vaddr & PAGE_MASK;

  //获得根页表
  paddr_t pdir = (cpu.satp & 0x003fffff) << 12;

  //从根页表中读取第vpn1个页目录项PDE
  PTE pde = paddr_read(pdir + vpn1 * sizeof(PTE), sizeof(PTE));

  assert(pde & PTE_V);  //一级页表项必须有效

  paddr_t ptab = (pde >> 10) << 12;

  PTE pte = paddr_read(ptab + vpn0 * sizeof(PTE), sizeof(PTE));

  assert(pte & PTE_V);  //二级页表项必须有效

  // PTE 的 bit 10 及以上保存最终物理页号。
  // 恢复物理页起始地址后，拼上不变的页内偏移，得到最终物理地址。
  return ((pte >> 10) << 12) | offset;
}
