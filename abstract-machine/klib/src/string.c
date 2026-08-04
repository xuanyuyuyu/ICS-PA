#include <klib.h>
#include <klib-macros.h>
#include <stddef.h>
#include <stdint.h>

#if !defined(__ISA_NATIVE__) || defined(__NATIVE_USE_KLIB__)

size_t strlen(const char *s) {
  size_t len = 0;
  while(*s != '\0') {
    s ++;
    len ++;
  }  
  return len;
}

char *strcpy(char *dst, const char *src) {
  size_t i;
  for(i = 0; src[i] != '\0'; i ++) {
    dst[i] = src[i];
  }
  dst[i] = '\0';
  return dst;
}

char *strncpy(char *dst, const char *src, size_t n) {
  size_t i;
  for(i = 0; i < n && src[i] != '\0'; i ++) {
    dst[i] = src[i];
  }
  for( ; i < n; i ++) {
    dst[i] = '\0';
  }
  return dst;
}

char *strcat(char *dst, const char *src) {
  size_t dst_len = strlen(dst);
  size_t i;
  for(i = 0; src[i] != '\0' ; i ++) {
    dst[dst_len ++] = src[i];
  }
  dst[dst_len] = '\0';
  return dst;
}

int strcmp(const char *s1, const char *s2) {
  while (*s1 != '\0' && *s1 == *s2) {
        s1++;
        s2++;
    }

  return *(unsigned char *)s1 -
         *(unsigned char *)s2;
}

int strncmp(const char *s1, const char *s2, size_t n)
{
  while (n > 0 && *s1 && *s1 == *s2) {
      s1++;
      s2++;
      n--;
  }

  if (n == 0)
      return 0;

  return (unsigned char)*s1 -
          (unsigned char)*s2;
}
void *memset(void *s, int c, size_t n) {
  unsigned char *p = s;

  while(n --) {
    *p = (unsigned char)c;
    p ++;
  }
  return s;
}

void *memmove(void *dst, const void *src, size_t n) {
  unsigned char *d = dst;
  const unsigned char *s = src;
 
  if(d < s) {
    //从前往后复制，防止覆盖
    for(size_t i = 0; i < n; i ++) {
      d[i] = s[i];
    }
  } else {
    //d >= s，如果从前往后复制，会覆盖掉s的内容，应该从后往前复制
    for(size_t i = n; i > 0; i --) {
      d[i - 1] = s[i - 1];
    }
  }

  return dst;
}

void *memcpy(void *out, const void *in, size_t n) {

    unsigned char *d = out;
    const unsigned char *s = in;

    while(n--) {
        *d++ = *s++;
    }

    return out;
}

int memcmp(const void *s1, const void *s2, size_t n)
{
    const unsigned char *p1 = s1;
    const unsigned char *p2 = s2;

    for(size_t i = 0; i < n; i++) {

        if(p1[i] != p2[i]) {
            return p1[i] - p2[i];
        }
    }

    return 0;
}
#endif
