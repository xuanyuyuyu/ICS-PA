# 已发现问题

## miniSDL 的 `SDL_Init()` 未返回初始化结果

文件：`navy-apps/libs/libminiSDL/src/general.c`

当前实现中，`SDL_Init()` 的返回类型为 `int`，但函数末尾没有返回值：

```c
int SDL_Init(uint32_t flags) {
  int ret = NDL_Init(flags);

  if (ret == 0) {
    sdl_init_ticks = NDL_GetTicks();
  }
}
```

这会导致 C 的未定义行为：调用者得到的返回值不确定。Bird 等应用会检查
`SDL_Init() < 0`；当随机返回值被误判为失败时，应用可能立即退出。若应用由
NTerm 通过 `execve()` 启动，就会表现为短暂跳转后又回到 NTerm，光标继续闪烁。

修复方式是在函数末尾返回 `ret`：

```c
int SDL_Init(uint32_t flags) {
  int ret = NDL_Init(flags);

  if (ret == 0) {
    sdl_init_ticks = NDL_GetTicks();
  }

  return ret;
}
```
