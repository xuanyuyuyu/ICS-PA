#include <am.h>
#include <klib.h>
#include <klib-macros.h>
#include <stdarg.h>


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

int snprintf(char *out, size_t n, const char *fmt, ...) {
  panic("Not implemented");
}

int vsnprintf(char *out, size_t n, const char *fmt, va_list ap) {
  panic("Not implemented");
}

#endif
