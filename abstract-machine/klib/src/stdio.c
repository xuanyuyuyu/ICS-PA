#include <am.h>
#include <klib.h>
#include <klib-macros.h>
#include <stdarg.h>


#if !defined(__ISA_NATIVE__) || defined(__NATIVE_USE_KLIB__)


int append_uint(char *out, unsigned int value, unsigned int base) {
  char *start = out;
  char digits[] = "0123456789ABCDEF";
  char temp[32];
  int len = 0;

  do {
    temp[len ++] = digits[value % base]; 
    value /= base;
  } while(value);

  while(len --) {
    *out ++ = temp[len];
  }

  return out - start;
}

int printf(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  char out[1024];
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

    //处理结尾处单独出现的 %
    if(*fmt == '\0') {
      *out ++ = '%';
      break;
    }
    
    switch(*fmt ++) {
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
        if(value < 0) {
          *out ++ = '-';
          magnitude = 0u - (unsigned int)value;
        } else {
          magnitude = (unsigned int)value;
        }
        append_uint(out, magnitude, 10);
        break;
      }
      case 'u': {
        unsigned int value = va_arg(ap, unsigned int);
        append_uint(out, value, 10);
        break;
      }
      case 'x': {
        unsigned int value = va_arg(ap, unsigned int);
        append_uint(out, value, 16);
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
