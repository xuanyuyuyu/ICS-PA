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
#include <cpu/cpu.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <monitor/watchpoint.h>
#include <stdio.h>
#include "common.h"
#include "sdb.h"
#include "utils.h"

static int is_batch_mode = false;

void init_regex();
void init_wp_pool();

/* We use the `readline' library to provide more flexibility to read from stdin. */
static char* rl_gets() {
  static char *line_read = NULL;

  if (line_read) {
    free(line_read);
    line_read = NULL;
  }

  line_read = readline("(nemu) ");
  //如果读取成功，并且不是空行
  if (line_read && *line_read) {
    add_history(line_read);
  }

  return line_read;
}

static int cmd_c(char *args) {
  //-1会被转换成无穷大
  cpu_exec(-1);  //给一个几乎不可能执行完的指令数量，让程序持续运行，直到遇到结束、异常、断点等事件主动退出循环。
  return 0;
}

static int cmd_si(char *args) {
  /* extract the first argument */
  char *arg = strtok(NULL, " ");
  int step = 1;
  if(arg != NULL) {
    step = atoi(arg);
  }
  cpu_exec(step); 
  return 0;
}

static int cmd_info(char *args) {
  /* extract the first argument */
  char *arg = strtok(NULL, " ");
  if(strcmp(arg, "r") == 0) {
    //看寄存器
    isa_reg_display();
  } else if(strcmp(arg, "w") == 0) {
    //看监视点
    watchpoint_display();
  }
  return 0;
}

static int cmd_x(char *args) {
  char *arg1 = strtok(NULL, " ");
  char *arg2 = strtok(NULL, " ");
  char *end;
  vaddr_t addr = strtoul(arg2, &end, 16);
  if(*end != '\0') {
    printf("地址格式错误！\n");
  } 
  for(int i = 0; i < atoi(arg1); i ++) {
    word_t value = vaddr_read(addr, 4);
    printf(FMT_WORD ": " FMT_WORD "\n", addr, value);
    
    addr += 4;
  }

  return 0;

}

static int cmd_q(char *args) {
  //nemu_state.state = NEMU_QUIT;
  return -1;
}

static int cmd_p(char *args) {
  if(args == NULL) {
    printf("Usage: p EPR\n");
    return 0;
  }


  bool success = false;

  word_t result = expr(args, &success);

  if(!success) {
    printf("表达式求值失败!\n");
    return 0;
  }

  printf("result = %u\n" ,result);
  return 0;
}
static int cmd_w(char *args) {
  if(args == NULL) {
     printf("Usage: w EPR\n");
    return 0;
  }
  new_wp(args);

  return 0;
}

static int cmd_d(char *args) {
  if(args == NULL) {
     printf("Usage: d\n");
    return 0;
  }
  char *num = strtok(NULL, " ");
  int no = strtoul(num, NULL, 0);
  watchpoint_delete(no);

  return 0;
}

static int cmd_help(char *args);

static struct {
  const char *name;
  const char *description;
  //函数指针
  int (*handler) (char *);
} cmd_table [] = {
  { "help", "Display information about all supported commands", cmd_help },
  { "c", "Continue the execution of the program", cmd_c },
  { "q", "Exit NEMU", cmd_q },
  
  
  { "si", "single step to execution", cmd_si },
  { "info", "show some info about reg or watchpointer", cmd_info},
  { "x", "show some value", cmd_x},
  { "p", "calculate a expression", cmd_p},
  { "w", "watchpoint", cmd_w},
  { "d", "delete a watchpoint", cmd_d}
};

#define NR_CMD ARRLEN(cmd_table)

static int cmd_help(char *args) {
  /* extract the first argument */
  char *arg = strtok(NULL, " ");
  int i;

  if (arg == NULL) {
    /* no argument given */
    for (i = 0; i < NR_CMD; i ++) {
      printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
    }
  }
  else {
    for (i = 0; i < NR_CMD; i ++) {
      if (strcmp(arg, cmd_table[i].name) == 0) {
        printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
        return 0;
      }
    }
    printf("Unknown command '%s'\n", arg);
  }
  return 0;
}

void sdb_set_batch_mode() {
  is_batch_mode = true;
}

void sdb_mainloop() {
  if (is_batch_mode) {
    cmd_c(NULL);
    
    printf("is_batch_mode=%d\n", is_batch_mode);
    return;
  }

  for (char *str; (str = rl_gets()) != NULL; ) {
    char *str_end = str + strlen(str);

    /* extract the first token as the command */
    char *cmd = strtok(str, " ");
    if (cmd == NULL) { continue; }

    /* treat the remaining string as the arguments,
     * which may need further parsing
     */
    char *args = cmd + strlen(cmd) + 1;
    //如果参数起点在命令行传入的字符串之后，则没有参数
    if (args >= str_end) {
      args = NULL;
    }

#ifdef CONFIG_DEVICE
    extern void sdl_clear_event_queue();
    sdl_clear_event_queue();
#endif

    int i;
    //NR_CMD是cmdtable的长度
    for (i = 0; i < NR_CMD; i ++) {
      if (strcmp(cmd, cmd_table[i].name) == 0) {
        if (cmd_table[i].handler(args) < 0) { return; }
        break;
      }
    }

    if (i == NR_CMD) { printf("Unknown command '%s'\n", cmd); }
  }
}

void init_sdb() {
  /* Compile the regular expressions. */
  init_regex();

  /* Initialize the watchpoint pool. */
  init_wp_pool();
}
