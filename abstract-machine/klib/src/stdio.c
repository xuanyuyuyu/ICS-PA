#include <am.h>
#include <klib.h>
#include <klib-macros.h>
#include <stdarg.h>
#include <stddef.h>


#if !defined(__ISA_NATIVE__) || defined(__NATIVE_USE_KLIB__)


static int append_uint(char *out, unsigned int value, unsigned int base, int width, char pad, bool negative) {

  const char digits[] = "0123456789abcdef";
  char temp[32];
  int len = 0;
  int written = 0;

  //逆序保存数字
  do {
    temp[len ++] = digits[value % base]; 
    value /= base;
  } while(value);

  //总长度需要包含负号
  int total_len = len + (negative ? 1 : 0);
  int padding = width > total_len ? width - total_len : 0;
  
  //空格补齐应该放在负号前面
  if(pad == ' ') {
    while(padding -- > 0) {
      out[written ++] = ' ';
    }
  }
  //输出符号
  if(negative) {
    out[written ++] = '-';
  }
  //补0应该放在负号后
  if(pad == '0') {
    while(padding -- > 0) {
      out[written ++] = '0';
    }
  }

  //把数字正序写入out
  while(len) {
    out[written++] = temp[--len];
  }
  return written;
}

int printf(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  char out[2048];
  int ret = vsprintf(out, fmt, ap);
  va_end(ap);

  for(char *p = out; *p != '\0'; p ++) {
    putch(*p);
  }

  return ret;
}


int vsprintf(char *out, const char *fmt, va_list ap) {
  char *start = out;
  while(*fmt != '\0') {
    if(*fmt != '%') {
      *out ++ = *fmt ++;
      continue;
    }
    //此时遇到%
    fmt ++;  //跳过%
    
    char pad = ' ';
    int width = 0;

    //解析前导0
    if(*fmt == '0') {
      pad = '0';
      fmt ++;
    }
    //解析宽度
    while(*fmt >= '0' && *fmt <= '9') {
      width = width * 10 + (*fmt - '0');
      fmt ++;
    }
    //处理结尾处单独出现的 %
    if(*fmt == '\0') {
      *out ++ = '%';
      break;
    }
    char spec = *fmt ++;
    switch(spec) {
      case '%':
        *out++ = '%';
        break;
      case 'c': {
        int ch = va_arg(ap, int);  //从参数列表取一个
        *out++ = (char)ch;
        break;
      }
      case 's': {
        const char *str = va_arg(ap, const char *);
        if(str == NULL) {
          str = "(null)";
        }
        while(*str != '\0') {
          *out ++ = *str ++;
        }
        break;
      }
      case 'd': {
        unsigned int magnitude;
        int value = va_arg(ap, int);
        int negative = value < 0;
        if(negative) {
          magnitude = 0u - (unsigned int)value;
        } else {
          magnitude = (unsigned int)value;
        }
        out += append_uint(out, magnitude, 10, width, pad, negative);
        break;
      }
      case 'u': {
        unsigned int value = va_arg(ap, unsigned int);
        out += append_uint(out, value, 10, width, pad, false);
        break;
      }
      case 'x': {
        unsigned int value = va_arg(ap, unsigned int);
        out += append_uint(out, value, 16, width, pad, false);
        break;
      }
      default:
        //不支持的格式原样返回
        *out ++ = '%';
        *out ++ = fmt[-1];  //switch(*fmt ++) 已经让 fmt 往后移动了一位，所以这里退回一位
    }
  }
  *out = '\0';
  return out - start;
}

int sprintf(char *out, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);

  int ret = vsprintf(out, fmt, ap);
  va_end(ap);
  return ret;
}






static void append_char_n(char *out, size_t n, size_t *count, char ch) {
  if(n > 0 && *count < n - 1) {
    out[*count] = ch;
  }
  (*count) ++;

}

static void append_uint_n(char *out, size_t n, size_t *count, unsigned int value, unsigned base, int width, char pad, bool negative) {
  const char digits[] = "0123456789abcdef";
  char temp[32];
  int len = 0;

  do{
    temp[len ++] = digits[value % base];
    value /= base;
  } while(value != 0);

  int total_len = len + (negative ? 1 : 0);
  int padding = width > total_len ? width - total_len : 0;

  //空格补齐放在负号前面
  if(pad == ' ') {
    while(padding -- > 0) {
      append_char_n(out, n, count, ' ');
    }
  }

  //0补齐放在负号后面
  if(pad == '0') {
    while(padding -- > 0) {
      append_char_n(out, n, count, '0');
    }
  }

  //正序输出数字
  while(len > 0) {
    append_char_n(out, n, count, temp[--len]);
  }

}





int snprintf(char *out, size_t n, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);

  int ret = vsnprintf(out, n, fmt, ap);

  va_end(ap);
  return ret;
}

int vsnprintf(char *out, size_t n, const char *fmt, va_list ap) { 
  size_t count = 0;

  while(*fmt != '\0') {
    if(*fmt != '%') {
      append_char_n(out, n, &count, *fmt);
      fmt ++;
      continue;
    }

    fmt ++;

    char pad = ' ';
    int width = 0;

    //解析前导0
    if(*fmt == '0') {
      pad = '0';
      fmt ++;
    }
    while(*fmt >= '0' && *fmt <= '9') {
      width = width * 10 + (*fmt - '0');
      fmt ++;
    }

    //处理格式字符串末尾单独出现的%
    if(*fmt == '\0') {
      append_char_n(out, n, &count, '%');
      break;
    }

    char spec = *fmt ++;

     switch (spec) {
      case '%':
        append_char_n(out, n, &count, '%');
        break;

      case 'c': {
        int ch = va_arg(ap, int);
        append_char_n(out, n, &count, (char)ch);
        break;
      }

      case 's': {
        const char *str = va_arg(ap, const char *);

        if (str == NULL) {
          str = "(null)";
        }

        while (*str != '\0') {
          append_char_n(out, n, &count, *str);
          str++;
        }
        break;
      }

      case 'd': {
        int value = va_arg(ap, int);
        bool negative = value < 0;
        unsigned int magnitude;

        if (negative) {
          /*
           * 这样写可以正确处理 INT_MIN，
           * 避免直接使用 -value 产生有符号溢出。
           */
          magnitude = 0u - (unsigned int)value;
        } else {
          magnitude = (unsigned int)value;
        }

        append_uint_n(
            out, n, &count,
            magnitude, 10,
            width, pad, negative);
        break;
      }

      case 'u': {
        unsigned int value = va_arg(ap, unsigned int);

        append_uint_n(
            out, n, &count,
            value, 10,
            width, pad, false);
        break;
      }

      case 'x': {
        unsigned int value = va_arg(ap, unsigned int);

        append_uint_n(
            out, n, &count,
            value, 16,
            width, pad, false);
        break;
      }

      default:
        // 不支持的格式原样输出
        append_char_n(out, n, &count, '%');
        append_char_n(out, n, &count, spec);
        break;
    }
  }

  /*
   * n > 0 时，保证结果以 '\0' 结尾。
   */
  if (n > 0) {
    if (count < n) {
      out[count] = '\0';
    } else {
      out[n - 1] = '\0';
    }
  }

  /*
   * 返回完整结果本来应该有的长度，
   * 不包含结尾的 '\0'。
   */
  return (int)count;



}

#endif
