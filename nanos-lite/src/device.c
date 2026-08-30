#include "klib-macros.h"
#include <common.h>

#if defined(MULTIPROGRAM) && !defined(TIME_SHARING)
# define MULTIPROGRAM_YIELD() yield()
#else
# define MULTIPROGRAM_YIELD()
#endif

#define NAME(key) \
  [AM_KEY_##key] = #key,

static const char *keyname[256] __attribute__((used)) = {
  [AM_KEY_NONE] = "NONE",
  AM_KEYS(NAME)
};

size_t serial_write(const void *buf, size_t offset, size_t len) {
  (void)offset;
  
  const char *str = (const char *)buf;

  for(size_t i = 0; i < len; i ++) {
    putch(str[i]);
  }
  return len;
}

size_t events_read(void *buf, size_t offset, size_t len) {
  (void)offset;

  if(buf == NULL || len == 0) {
    return 0;
  }

  MULTIPROGRAM_YIELD();

  AM_INPUT_KEYBRD_T event = io_read(AM_INPUT_KEYBRD);

  if(event.keycode == AM_KEY_NONE) {
    return 0;
  }
  assert(event.keycode < LENGTH(keyname));
  assert(keyname[event.keycode] != NULL);

  int n = snprintf((char *)buf, len,
  "%s %s\n", event.keydown ? "kd" : "ku", keyname[event.keycode]);

  if(n < 0) return 0;

  if((size_t)n >= len) {
    return len - 1;
  }

  return (size_t)n;
}

size_t dispinfo_read(void *buf, size_t offset, size_t len) {
  return 0;
}

size_t fb_write(const void *buf, size_t offset, size_t len) {
  return 0;
}

void init_device() {
  Log("Initializing devices...");
  ioe_init();
}
