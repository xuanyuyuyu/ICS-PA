#include <nterm.h>
#include <stdarg.h>
#include <unistd.h>
#include <SDL.h>
#include <stdlib.h>
#include <string.h>

char handle_key(SDL_Event *ev);

static void sh_printf(const char *format, ...) {
  static char buf[256] = {};
  va_list ap;
  va_start(ap, format);
  int len = vsnprintf(buf, 256, format, ap);
  va_end(ap);
  term->write(buf, len);
}

static void sh_banner() {
  sh_printf("Built-in Shell in NTerm (NJU Terminal)\n\n");
}

static void sh_prompt() {
  sh_printf("sh> ");
}

static void sh_handle_cmd(const char *cmd) {
  char filename[128];

  int len = strlen(cmd);
  while(len > 0 && (cmd[len - 1] == '\n' || cmd[len - 1] == 'r')) {
    len --;
  }
  if(len == 0) {
    return ;
  }

  int name_len = 0;
  while(name_len < len && cmd[name_len] != ' ') {
    name_len ++;
  }

  if(name_len >= (int)sizeof(filename)){
    sh_printf("command too long\n");
    return;
  }

  memcpy(filename, cmd, name_len);
  filename[name_len] = '\0';

  //让execvp在/bin下搜索程序
  setenv("PATH", "/bin", 0);

  char *argv[] = {filename, NULL};

  execvp(filename, argv);

  // 只有执行失败才会返回到这里
  sh_printf("command not found: %s\n", filename);
}

void builtin_sh_run() {
  sh_banner();
  sh_prompt();

  while (1) {
    SDL_Event ev;
    if (SDL_PollEvent(&ev)) {
      if (ev.type == SDL_KEYUP || ev.type == SDL_KEYDOWN) {
        const char *res = term->keypress(handle_key(&ev));
        if (res) {
          sh_handle_cmd(res);
          sh_prompt();
        }
      }
    }
    refresh_terminal();
  }
}
