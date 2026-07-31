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

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <string.h>
#include <stdbool.h>

// this should be enough
static char buf[65536] = {};
static char cbuf[65536] = {};
static char code_buf[65536 + 128] = {}; // a little larger than `buf`
static char *code_format =
"#include <stdio.h>\n"
"int main() { "
"  unsigned result = %s; "
"  printf(\"%%u\", result); "
"  return 0; "
"}";

static int choose(int n) {
  return rand() % n;
}

static void gen(char c) {
  size_t len = strlen(buf);
  buf[len] = c;
  buf[len + 1] = '\0';

  len = strlen(cbuf);
  cbuf[len] = c;
  cbuf[len + 1] = '\0';
}


static bool valid;

static unsigned gen_rand_expr() {
  switch(choose(3)) {
    case 0: {
      unsigned value = (unsigned)rand() % 100;
      char num[32];
      sprintf(num, "%u", value);
      strcat(buf, num);

      sprintf(num, "%uu", value);
      strcat(cbuf, num);
      return value;
    }
    case 1: {
      gen('(');
      unsigned value = gen_rand_expr();
      gen(')');
      return value;
    }
    case 2: {
      gen('(');
      unsigned op1 = gen_rand_expr();
      char op = "+-*/"[rand() % 4];
      gen(op);
      unsigned op2 = gen_rand_expr();
      gen(')');
      if(op == '/' && op2 == 0) {
        valid = false;
        return 0;
      }

      switch (op) {
        case '+': return op1 + op2;
        case '-': return op1 - op2;
        case '/': return op1 / op2;
        case '*': return op1 * op2;
      }

    }
  }
  return 0;
}


int main(int argc, char *argv[]) {
  int seed = time(0);
  srand(seed);
  int loop = 1;
  if (argc > 1) {
    sscanf(argv[1], "%d", &loop);
  }
  int i;
  for (i = 0; i < loop; i ++) {
    unsigned result;
    do{
      buf[0] = '\0';
      cbuf[0] = '\0';
      valid = true;
      result = (unsigned)gen_rand_expr();
    } while(!valid);
      

    //把 buf（表达式字符串，如 1 + 2 * 3）嵌入到 code_format 这个 C 程序模板里，结果写入 code_buf
    sprintf(code_buf, code_format, cbuf);

    FILE *fp = fopen("/tmp/.code.c", "w");
    assert(fp != NULL);
    fputs(code_buf, fp);
    fclose(fp);

    int ret = system("gcc /tmp/.code.c -o /tmp/.expr");
    if (ret != 0) continue;
    
    // 执行程序并读取
    fp = popen("/tmp/.expr", "r");
    assert(fp != NULL);
    
    ret = fscanf(fp, "%u", &result);
    pclose(fp);

    printf("%u %s\n", result, buf);
  }
  return 0;
}
