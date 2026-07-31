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

/* We use the POSIX regex functions to process regular expressions.
 * Type 'man regex' for more information about POSIX regex functions.
 */
#include <regex.h>
enum {
  TK_NOTYPE = 256, TK_HEX,
  TK_NUM, TK_REG,
  TK_EQ, TK_NEQ, 
  TK_AND, TK_OR,
  TK_ASSIGN,
};

static struct rule {
  const char *regex;
  int token_type;
} rules[] = {
  {" +", TK_NOTYPE},    // spaces
  {"0[xX][0-9a-fA-F]+", TK_HEX},
  {"[0-9]+", TK_NUM},
  {"\\$[a-zA-Z][a-zA-Z0-9]+", TK_REG},
  {"==", TK_EQ},        // equal
  {"!=", TK_NEQ},
  {"&&", TK_AND},
  {"\\|\\|", TK_OR},
  {"\\+", '+'},         // plus
  {"-", '-'},
  {"\\*", '*'},
  {"\\/", '/'},
  
  {"\\(", '('},
  {"\\)", ')'},
  
  {"=", TK_ASSIGN}
};

#define NR_REGEX ARRLEN(rules)

static regex_t re[NR_REGEX] = {};

/* Rules are used for many times.
 * Therefore we compile them only once before any usage.
 */
void init_regex() {
  int i;
  char error_msg[128];
  int ret;

  for (i = 0; i < NR_REGEX; i ++) {
    ret = regcomp(&re[i], rules[i].regex, REG_EXTENDED);
    if (ret != 0) {
      regerror(ret, &re[i], error_msg, 128);
      panic("regex compilation failed: %s\n%s", error_msg, rules[i].regex);
    }
  }
}

typedef struct token {
  int type;
  char str[32];
} Token;

static Token tokens[1000] __attribute__((used)) = {};
static int nr_token __attribute__((used))  = 0;

static bool make_token(char *e) {
  int position = 0;
  int i;
  regmatch_t pmatch;

  nr_token = 0;

  while (e[position] != '\0') {
    /* Try all rules one by one. */
    for (i = 0; i < NR_REGEX; i ++) {
      if (regexec(&re[i], e + position, 1, &pmatch, 0) == 0 && pmatch.rm_so == 0) {
        char *substr_start = e + position;
        int substr_len = pmatch.rm_eo;

        Log("match rules[%d] = \"%s\" at position %d with len %d: %.*s",
            i, rules[i].regex, position, substr_len, substr_len, substr_start);

        position += substr_len;

        //把匹配到的字符串放到tokens里面
        int copy_len = substr_len < 31 ? substr_len : 31;
        strncpy(tokens[nr_token].str, substr_start, copy_len);
        tokens[nr_token].str[copy_len] = '\0';

        switch (rules[i].token_type) {
          case TK_NOTYPE:
            tokens[nr_token].type = TK_NOTYPE; break;
          case TK_HEX:
            tokens[nr_token].type = TK_HEX; break;
          case TK_NUM:
            tokens[nr_token].type = TK_NUM; break;
          case TK_EQ:
            tokens[nr_token].type = TK_REG; break;
          case TK_NEQ:
            tokens[nr_token].type = TK_REG; break;
          case TK_AND:
            tokens[nr_token].type = TK_AND; break;
          case TK_OR:
            tokens[nr_token].type = TK_OR; break;
          case '+':
            tokens[nr_token].type = '+'; break;
          case '-':
            tokens[nr_token].type = '-'; break;
          case '*':
            tokens[nr_token].type = '*'; break;
          case '/':
            tokens[nr_token].type = '/'; break;
          case '(':
            tokens[nr_token].type = '('; break;
          case ')':
            tokens[nr_token].type = ')'; break;
          case '=':
            tokens[nr_token].type = TK_ASSIGN; break;
          default: 
            panic("unhandled token type: %d", rules[i].token_type);
        }
        nr_token ++;
        break;
      }
    }

    if (i == NR_REGEX) {
      printf("no match at position %d\n%s\n%*.s^\n", position, e, position, "");
      return false;
    }
  }

  return true;
}

bool check_parentheses(word_t p, word_t q) {
  if(tokens[p].type != '(' || tokens[q].type != ')') return false;
  int count = 0;
  for(int i = p + 1; i < q; i ++) {
    int token_type = tokens[i].type;
    if(token_type == '(') count ++;
    if(token_type == ')') count --; 
    if(count < 0){
      panic("表达式求值出现错误：括号不匹配");
      return false;
    } 
  }
  
  if(count != 0) {
    panic("表达式求值出现错误：括号不匹配");
    return false;
  }
  return true;
}

/*找到当前表达式优先级最低的运算符*/
int position_of_main_operation(word_t p, word_t q) {
  
  int main_position = -1;
  int min_priority = 100;

  int level = 0;  //括号层数

  for(int i = p; i <= q; i ++) {
    int token_type = tokens[i].type;
    //遇到左括号，进入括号内部
    if(token_type == '(') {
      level ++;
      continue;
    }
    //遇到右括号，退出括号内部
    if(token_type == ')') {
      level --;
      continue;
    }
    //括号内部的运算符不能作为主运算符
    if(level > 0) {
      continue;
    }
    int priority = 0;
    switch (token_type) {
      //低优先级
      case '+':
      case '-':
        priority = 1;
        break;
      case '*':
      case '/':
        priority = 2;
        break;
      //不是运算符
      default:
        continue;
    }
    /*找最低运算符(最右边的)*/
    if (priority <= min_priority) {
      min_priority = priority;
      main_position = i;
    }
  }
  
  return main_position;
}


word_t eval(word_t p, word_t q) {
  if(p > q) {
    panic("表达式求值出现错误: p > q");
  } else if (p == q) {
    return strtoul(tokens[p].str, NULL, 0);
  } else if (check_parentheses(p, q) == true) {
    return eval(p + 1, q - 1);
  } else {
    int op = position_of_main_operation(p, q);
    word_t val1 = eval(p, op - 1);
    word_t val2 = eval(op + 1, q);
    word_t op_type = tokens[op].type;
    switch (op_type) {
      case '+': return val1 + val2;
      case '-': return val1 - val2;
      case '*': return val1 * val2;
      case '/':
        {
          if(val2 == 0) panic("表达式除零错误 ！");
          return val1 / val2;
        } 
      default: assert(0);
    }
  }
}

word_t expr(char *e, bool *success) {
  if (!make_token(e)) {
    *success = false;
    return 0;
  }
  *success = true;
  return eval(0, nr_token-1);;
}