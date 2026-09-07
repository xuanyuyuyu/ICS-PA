#include <stdint.h>
#include <stdlib.h>
#include <assert.h>

int main(int argc, char *argv[], char *envp[]);
extern char **environ;
void call_main(uintptr_t *args) {
  int argc = (int)args[0];
  char **argv = (char **)args[1];
  char **envp = (char **)args[2];
  environ = envp; //把进程启动时传递的环境变量，放在全局变量中，供getenv()等函数使用

  exit(main(argc, argv, envp));
  assert(0);
}
