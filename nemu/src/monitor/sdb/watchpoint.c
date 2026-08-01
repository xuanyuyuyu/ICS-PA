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
#include "sdb.h"
#include <stdio.h>
#include <stdlib.h>
#include <monitor/watchpoint.h>


#define NR_WP 32
#define WP_EXPR_MAX 256
struct watchpoint {
  int NO;
  struct watchpoint *next;

  char expr[WP_EXPR_MAX];
  word_t old_value;

};


static WP wp_pool[] = {};
static WP *head = NULL, *free_ = NULL;  //head用于组织使用中的监视点结构, free_用于组织空闲的监视点结构,

void init_wp_pool() {
  int i;
  for (i = 0; i < NR_WP; i ++) {
    wp_pool[i].NO = i;
    wp_pool[i].next = (i == NR_WP - 1 ? NULL : &wp_pool[i + 1]);
  }

  head = NULL;
  free_ = wp_pool;
}


WP* new_wp(char *expression) {
  if(free_ == NULL) {
    panic("监视点链表不够用了");
  }
  // 1.插入节点
  WP *ret = free_;
  free_ = free_->next;
  //处理表达式
  if (strlen(expression) >= WP_EXPR_MAX) {
    printf("监视点表达式过长\n");
    return NULL;
  }
  strcpy(ret->expr, expression);

  ret->next = head;
  head = ret;

  // 2.计算值
  bool success = false;

  ret->old_value = expr(expression, &success);
  if(!success) {
    printf("表达式求值失败!\n");
    return NULL;
  }
  return ret;
}

void free_wp(WP *wp) {
  wp->expr[0] = '\0';
  wp->old_value = 0;

  wp->next = free_;  //直接指向头节点
  free_ = wp;  //把自己变成头节点
}

void check_watchpoint() {
  for(WP *p = head; p != NULL; p = p->next) {
    
    bool success = false;

    word_t new_value = expr(p->expr, &success);
    if(!success) {
      printf("表达式求值失败!\n");
      return;
    }
    if(new_value != p->old_value) {
      printf("Watchpoint %d triggered!\n", p->NO);
      printf("  expr: %s\n", p->expr);
      printf("  old value = " FMT_WORD "\n", p->old_value);
      printf("  new value = " FMT_WORD "\n", new_value);

      p->old_value = new_value;

      nemu_state.state = NEMU_STOP;
    }
  }
}

void watchpoint_display() {
  printf("Num\tExpression\tValue\n");

  for(WP *p = head; p != NULL; p = p->next) {
    printf("%d\t%s\t\t" FMT_WORD "\n",
        p->NO,
        p->expr,
        p->old_value
    );

  }
}

void watchpoint_delete(int num) {

  WP *prev = NULL;
  WP *p = head;

  while(p != NULL) {
    if(p->NO == num) {
      if(prev == NULL) {
        head = p->next;
      } else {
        prev->next = p->next;
      }
      free_wp(p);
      printf("Delete watchpoint: %d\n", num);
      return ;
    }
    

    prev = p;
    p = p->next;
  }

  printf("Not found watchpoint number : %d\n", num);
}