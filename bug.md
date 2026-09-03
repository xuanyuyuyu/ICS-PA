# ICS2025 Bug 调试记录

本文档用于记录 ICS PA 开发中较难定位的问题。每个案例尽量保留完整的证据链，而不只记录最终修改，以便以后复习和复用调试方法。

> 注意：反汇编地址由具体构建产物决定，重新编译后可能变化。分析时应以当次生成的 ELF 为准。

## 目录

| 编号 | 问题 | 类型 | 核心工具 | 状态 |
| --- | --- | --- | --- | --- |
| BUG-001 | `printf()` 打印 logo 后跳转到非法地址 | 栈缓冲区溢出 | ITRACE、Watchpoint、FTRACE/BT、`addr2line`、`objdump` | 已定位 |
| BUG-002 | NEMU 不断输出“表达式求值失败” | 监视点生命周期错误 | `info w`、代码审查、链表状态分析 | 已修复 |

---

## BUG-001：`printf()` 栈缓冲区溢出破坏返回地址

### 基本信息

- 模块：Abstract Machine KLIB / nanos-lite / NEMU
- 相关文件：
  - `abstract-machine/klib/src/stdio.c`
  - `nanos-lite/src/main.c`
  - `nanos-lite/resources/logo.txt`
  - `nemu/src/cpu/cpu-exec.c`
  - `nemu/src/utils/ftrace.c`
- 架构：`riscv32-nemu`
- 状态：根因已定位；可先扩大缓冲区验证，最终应实现有边界或流式输出

### 现象

nanos-lite 输出 Project-N logo 后，NEMU 尝试从非法地址取指：

```text
address (0x7c24243a) is out of bound at pc = 0x7c24243a
ra = 0x7c24243a
pc = 0x7c24243a
```

宿主机随后在 MMIO 地址检查中触发断言并退出。

### 本次使用的调试工具

#### 1. ITRACE 环形缓冲区

NEMU 在 `nemu/src/cpu/cpu-exec.c` 中保存最近 32 条已执行指令。为了让宿主机 `Assert/panic` 也能显示它，需要在 `assert_fail_msg()` 中调用 `iringbuf_display()`，不能只在 `NEMU_ABORT` 分支调用。

它给出的最后几条指令为：

```asm
0x80000b60: lw   ra, 0x41c(sp)
0x80000b64: lw   s0, 0x418(sp)
0x80000b68: mv   a0, s1
0x80000b6c: lw   s1, 0x414(sp)
0x80000b70: addi sp, sp, 0x440
0x80000b74: ret
```

这是一段典型的函数尾声：恢复被调用者保存寄存器、释放栈帧，然后返回。

#### 2. NEMU Watchpoint

设置 `ra` 监视点：

```text
(nemu) w $ra
(nemu) c
```

关键结果：

```text
Watchpoint triggered!
  expr: $ra
  old value = 0x80000b58
  new value = 0x7c24243a
```

Watchpoint 停止时显示的下一条指令是 `0x80000b64`，因此刚执行完的是：

```asm
lw ra, 0x41c(sp)
```

结论：不是 `ret` 修改了 `ra`，而是栈中保存的 `ra` 早已损坏；`lw` 只是把坏值加载出来。

#### 3. FTRACE 影子调用栈与 `bt`

FTRACE 在每次函数调用时保存函数名、入口地址和预期返回地址。在 SDB 中加入 `bt` 命令后，故障现场得到：

```text
(nemu) bt
FTRACE call stack: depth = 3
  #0 printf target=0x80000afc return=0x800000d8
  #1 main target=0x800000b8 return=0x80000250
  #2 _trm_init target=0x8000023c return=0x80000010
```

这说明当前最内层函数是 `printf()`，而它预期返回 `0x800000d8`，实际返回目标却是 `0x7c24243a`。

`bt` 本身只能缩小范围，不能单独证明缓冲区溢出；还必须结合下面的反汇编、源码和数据长度。

#### 4. ELF 地址解析

使用 ELF 将指令地址映射到函数：

```bash
riscv64-linux-gnu-addr2line \
  -e nanos-lite/build/nanos-lite-riscv32-nemu.elf \
  -f -C 0x80000b60 0x80000b74
```

查看目标地址附近的反汇编：

```bash
riscv64-linux-gnu-objdump -d -S \
  nanos-lite/build/nanos-lite-riscv32-nemu.elf \
  --start-address=0x80000afc \
  --stop-address=0x80000b80
```

确认 `0x80000afc` 到 `0x80000b74` 属于 `printf()`。

#### 5. 文件长度和字节分析

检查实际被打印的数据长度：

```bash
wc -c nanos-lite/resources/logo.txt
```

结果：

```text
1404 nanos-lite/resources/logo.txt
```

将错误地址按 RISC-V 小端序拆成字节：

```text
0x7c24243a
→ 内存字节 3a 24 24 7c
→ ASCII    :  $  $  |
→ 字符串   ":$$|"
```

这些正是 logo 中大量出现的字符，是输出内容覆盖返回地址的直接证据。

### 证据链与推理过程

1. 程序在 `ret` 后跳到 `0x7c24243a`，说明返回目标异常。
2. Watchpoint 表明 `ra` 在 `lw ra, 0x41c(sp)` 时从栈中恢复为坏值。
3. FTRACE 的 `bt` 表明当前栈顶函数为 `printf()`，预期返回地址为 `0x800000d8`。
4. `printf()` 的栈帧大小为 `0x440`，其局部缓冲区从 `sp + 0x10` 开始，保存的 `ra` 位于 `sp + 0x41c`。
5. 从缓冲区起点到保存的 `ra` 只有 `0x41c - 0x10 = 1036` 字节。
6. `stdio.c` 中缓冲区只有 1024 字节，并使用无边界检查的 `vsprintf()`。
7. `printf("%s", logo)` 需要写出 1404 字节，足以越过缓冲区并覆盖保存的寄存器。
8. 损坏值 `0x7c24243a` 的小端字节就是 logo 字符 `":$$|"`。

因此可以确定：打印 logo 时，`vsprintf()` 写越界，覆盖了 `printf()` 栈帧中保存的 `ra`。

### 栈帧示意

```text
低地址
sp + 0x010   out[1024] 起点
       ...
sp + 0x40f   out[1024] 末尾
sp + 0x414   保存的 s1
sp + 0x418   保存的 s0
sp + 0x41c   保存的 ra
高地址
```

`vsprintf()` 写入 1404 字节时，写入范围会越过 `sp + 0x41c`。

### 根因

`abstract-machine/klib/src/stdio.c` 中：

```c
int printf(const char *fmt, ...) {
  char out[1024];
  // ...
  int ret = vsprintf(out, fmt, ap);
  // ...
}
```

`vsprintf()` 不知道 `out` 的容量，格式化结果超过 1024 字节时必然越界。

### 修复思路

临时验证方案：

```c
char out[2048];
```

这可以验证根因，但不是最终修复，因为更长的输出仍可能溢出。

推荐方案之一：

- 正确实现 `vsnprintf()`，所有写入都检查剩余容量；或者
- 让 `printf()` 流式格式化并通过 `putch()` 输出，不依赖固定大小的栈缓冲区。

如果使用固定缓冲区配合 `vsnprintf()`，必须接受输出被截断，并保证末尾始终写入 `\0`。

### 验证方法

1. 重新构建并运行 nanos-lite。
2. 确认 logo 输出后能够继续执行。
3. 在 `printf()` 返回前执行 `bt`，确认调用层次合理。
4. 监视 `ra`，确认恢复值等于 FTRACE 记录的 `return_addr`。
5. 使用超过缓冲区大小的测试字符串验证不会越界。

### 复盘

- `bt` 只告诉我们故障发生在哪条调用链中，不足以单独判断根因。
- “从哪里加载出坏值”比“在哪条指令崩溃”更重要。
- 返回地址出现可识别 ASCII 时，应优先怀疑字符串写越界。
- 固定长度缓冲区配合 `sprintf/vsprintf/strcpy` 是重点检查对象。
- 调试信息也需要覆盖宿主机断言路径，不能只依赖模拟器状态机的 `NEMU_ABORT`。

---

## BUG-002：无效监视点导致不断输出“表达式求值失败”

### 基本信息

- 模块：NEMU SDB Watchpoint
- 相关文件：`nemu/src/monitor/sdb/watchpoint.c`
- 状态：已修复

### 现象

运行或单步执行时，终端不断出现：

```text
表达式求值失败!
表达式求值失败!
表达式求值失败!
```

输出与客户程序字符交错，因为 `check_watchpoint()` 每执行一条客户指令都会运行一次。

与此同时，一个有效监视点显示为：

```text
Watchpoint 1 triggered!
  expr: $ra
```

编号为 1 暗示编号 0 的监视点已经被占用，但它没有正常工作。

### 本次使用的调试工具

#### 1. SDB 监视点状态

可以使用：

```text
(nemu) info w
```

查看当前活动监视点。出现有效的 `Watchpoint 1` 时，应检查 `Watchpoint 0` 是否是此前创建失败或内容异常的节点。

临时删除可疑节点：

```text
(nemu) d 0
```

#### 2. 根据输出频率定位调用点

“每条指令出现一次”提示应检查主执行循环。`execute()` 在每条指令后调用：

```c
check_watchpoint();
```

而 `check_watchpoint()` 会遍历活动链表并求值：

```c
for (WP *p = head; p != NULL; p = p->next) {
  word_t new_value = expr(p->expr, &success);
  if (!success) {
    printf("表达式求值失败!\n");
    return;
  }
}
```

因此高频输出说明活动链表中长期存在一个无法求值的表达式。

#### 3. 链表生命周期代码审查

原先 `new_wp()` 的顺序为：

```text
从 free_ 取出节点
→ 将节点挂入 head 活动链表
→ 调用 expr() 验证并计算初值
→ 如果失败则直接 return NULL
```

失败路径没有：

- 从 `head` 中移除节点；
- 将节点放回 `free_`；
- 清空无效表达式。

因此创建失败的监视点仍被当成活动监视点，每条指令都会再次求值并失败。

### 根因

监视点节点在表达式验证之前就被提交到活动链表，错误路径缺少回滚。这是典型的“先修改全局状态，后验证输入”导致的资源和状态泄漏。

表达式过长的失败路径也存在相同的空闲节点泄漏风险，因为旧实现先移动 `free_`，再检查长度。

### 修复

将 `new_wp()` 改为先验证、后提交：

```c
WP *new_wp(char *expression) {
  if (expression == NULL) {
    printf("监视点表达式不能为空\n");
    return NULL;
  }

  if (strlen(expression) >= WP_EXPR_MAX) {
    printf("监视点表达式过长\n");
    return NULL;
  }

  bool success = false;
  word_t initial_value = expr(expression, &success);
  if (!success) {
    printf("表达式求值失败!\n");
    return NULL;
  }

  if (free_ == NULL) {
    panic("监视点链表不够用了");
  }

  WP *ret = free_;
  free_ = free_->next;

  strcpy(ret->expr, expression);
  ret->old_value = initial_value;
  ret->next = head;
  head = ret;

  return ret;
}
```

核心原则是：

```text
检查输入
→ 计算初始值
→ 确认资源充足
→ 分配节点
→ 最后提交到活动链表
```

### 验证方法

1. 重启 NEMU，使旧进程中的监视点池重新初始化。
2. 创建非法表达式，确认只报告一次失败：

   ```text
   (nemu) w invalid-expression
   ```

3. 执行若干条指令，确认不再重复输出失败。
4. 使用 `info w`，确认非法表达式没有进入活动链表。
5. 创建有效监视点：

   ```text
   (nemu) w $ra
   ```

6. 确认有效监视点能够正常触发、更新旧值并停止执行。

### 复盘

- 分配资源后存在多个失败出口时，必须逐一检查是否回滚。
- 更稳妥的模式是“先验证，后提交”，让全局状态只在所有前置条件满足后改变。
- 重复输出的频率是重要线索：每条指令一次，通常指向 CPU 主循环中的检查逻辑。
- 编号异常也能暴露隐藏状态：第一个可见监视点编号为 1，意味着编号 0 仍被某个节点占用。

---

## 新 Bug 记录模板

复制本节并将 `BUG-XXX` 替换为下一个编号。尽量先记录证据，再写结论，避免让最初猜测污染后续分析。

````markdown
## BUG-XXX：一句话描述问题

### 基本信息

- 日期：YYYY-MM-DD
- 模块：
- 相关文件：
- 架构/配置：
- 复现提交或版本：
- 状态：调查中 / 已定位 / 已修复 / 待验证

### 现象

- 用户可见现象：
- 首个错误信息：
- 最终崩溃位置：
- 是否稳定复现：

### 最小复现步骤

1. 
2. 
3. 

### 预期行为


### 实际行为


### 使用的调试工具

- ITRACE：
- FTRACE / bt：
- ETRACE：
- Watchpoint：
- GDB：
- addr2line / objdump / readelf / nm：
- 日志或临时断言：

### 已知事实

1. 
2. 

### 排查过的假设

| 假设 | 验证方法 | 结果 | 是否排除 |
| --- | --- | --- | --- |
|  |  |  |  |

### 关键证据

```text
粘贴最小且关键的日志、寄存器或反汇编片段
```

### 证据链与推理过程

1. 
2. 
3. 

### 根因


### 修复方案


### 修改文件

- `path/to/file.c`：

### 验证方法与结果

1. 
2. 

### 未解决问题或后续改进

- 

### 复盘

- 哪个现象最有价值：
- 哪个工具最有效：
- 哪条错误思路浪费了时间：
- 如何让同类问题更早暴露：
````

## 常用调试命令速查

### NEMU SDB

```text
si [N]        单步执行 N 条指令
c             继续执行
info r        查看寄存器
info w        查看监视点
p EXPR        计算表达式
w EXPR        创建监视点
d N           删除编号为 N 的监视点
x N ADDR      从地址 ADDR 开始查看 N 个 word
bt            查看 FTRACE 影子调用栈
```

### ELF 与反汇编

```bash
# 地址映射到函数和源码
riscv64-linux-gnu-addr2line -e path/to/program.elf -f -C 0xADDRESS

# 混合源码反汇编
riscv64-linux-gnu-objdump -d -S path/to/program.elf

# 查看特定地址区间
riscv64-linux-gnu-objdump -d -S path/to/program.elf \
  --start-address=0xSTART --stop-address=0xEND

# 按地址排列符号
riscv64-linux-gnu-nm -n path/to/program.elf

# 查看 ELF 头、段和符号信息
riscv64-linux-gnu-readelf -h -S -s path/to/program.elf
```

### 数据检查

```bash
# 文件字节数
wc -c path/to/file

# 查看末尾字节及 ASCII
tail -c 64 path/to/file | od -An -tx1c

# 搜索定义和调用
rg -n 'symbol_name' .
```

## 通用调试原则

1. 先保存首个异常现场，不要只看最终的连锁报错。
2. 将“崩溃指令”和“状态首次被破坏的指令”区分开。
3. 同时记录 PC、关键寄存器、最近指令和调用栈。
4. 反汇编地址必须用本次构建的 ELF 解析。
5. 对可疑内存值检查字节序和 ASCII，数据形态经常能指出来源。
6. 根据输出频率反推代码执行频率和可能的调用位置。
7. 检查所有错误返回路径是否泄漏资源或留下半完成状态。
8. 修复后不仅复现原场景，还应加入边界值和失败路径测试。

---

## BUG-003：`_execve()` 错误调用 `_exit()`，导致程序重新进入 NTerm

### 基本信息

- 模块：Navy libos 系统调用封装
- 相关文件：
  - `navy-apps/libs/libos/src/syscall.c`
  - `nanos-lite/src/syscall.c`
  - `nanos-lite/src/loader.c`
- 状态：已定位，待修复

### 现象

Bird 单独启动时可以正常运行，但在 NTerm 中输入 `bird` 并按回车后，界面仍停留在 NTerm，光标继续闪烁。加载日志连续两次显示：

```text
Loading /bin/nterm, entry = 0x8300e5e8
Loading /bin/nterm, entry = 0x8300e5e8
```

没有出现预期的 `Loading /bin/bird`，说明 Nanos-lite 收到的不是 `SYS_execve`。

### 根因

`navy-apps/libs/libos/src/syscall.c` 中的 `_execve()` 错误调用了 `_exit()`：

```c
int _execve(const char *fname, char * const argv[], char *const envp[]) {
  _exit(SYS_execve);
  return 0;
}
```

因此实际执行路径为：

```text
execvp("bird")
  -> execve("/bin/bird", ...)
  -> _execve()
  -> _exit(SYS_execve)
  -> 发起 SYS_exit
  -> Nanos-lite 再次加载 /bin/nterm
```

传给 `_exit()` 的 `SYS_execve` 只是退出状态码，并不会把系统调用类型变成 `SYS_execve`。

### 修复方式

让 `_execve()` 通过 `_syscall_()` 发起真正的 `SYS_execve`，并依次传递文件名、参数数组和环境变量：

```c
int _execve(const char *fname, char * const argv[], char *const envp[]) {
  return (int)_syscall_(
    SYS_execve,
    (intptr_t)fname,
    (intptr_t)argv,
    (intptr_t)envp
  );
}
```

修复后的预期日志为：

```text
Loading /bin/nterm, entry = ...
Loading /bin/bird, entry = ...
```
