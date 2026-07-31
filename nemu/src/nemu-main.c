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

#include <common.h>
#include <stdio.h>
#include<stdbool.h>
#include<stdlib.h>

word_t expr(char *e, bool *success);
void init_regex();


void init_monitor(int, char *[]);
void am_init_monitor();
void engine_start();
int is_exit_status_bad();

// int main(int argc, char *argv[]) {
//   /* Initialize the monitor. */
// #ifdef CONFIG_TARGET_AM
//   am_init_monitor();
// #else
//   init_monitor(argc, argv);
// #endif

//   /* Start engine. */
//   engine_start();


//   return is_exit_status_bad();
// }

int main(int argc, char **argv) {
  FILE *fp = fopen("input", "r");
  if(fp == NULL) {
    perror("input error");
    return 1;
  }

  init_regex();

  unsigned expected;
  char expression[65536];

  while(fscanf(fp, "%u %[^\n]", &expected, expression) == 2) {
    bool success = false;

    printf("============================即将开始计算\n");
    printf("expr    = %s\ns", expression);

    word_t actual = expr(expression, &success);

    if(!success || actual != expected) {
      printf("=================================\n");
      printf("Mismatch!\n");
      printf("expected = %u\n", expected);
      printf("actual   = %u\n", actual);
      printf("expr     = %s\n", expression);
      printf("=================================\n");
      break;
    }

    printf("PASS: %u %s\n", actual, expression);

  }

  fclose(fp);

  return 0;
}