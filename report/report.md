# Lab 1 实验报告：最小可执行内核与启动流程

本实验围绕最小可执行内核的启动展开：先理解程序的内存布局和入口点，再通过 OpenSBI 实现输出，最后用 QEMU 与 GDB 观察从加电复位到内核执行的过程。

## 1. 实验概况与分工

| 项目 | 内容 |
|---|---|
| 小组成员 | 刘华彬，2411238（A）；柯云超，2413575（B）；匡航逸，2412235（C） |
| 练习 2 负责人 | 匡航逸，2412235（C） |
| 完成日期 | 2026/10/09 |
| AI 工具与底层模型 | Codex，GPT-6 |
| 补充调试日期 | 2026-10-11 |
| 实验目标 | 理解最小内核的启动过程，解释入口操作，使用 QEMU 与 GDB 验证从复位到内核入口的执行过程 |

小组成员共同学习“构建 → 启动 → 复位跟踪 → 入口与栈观察 → 内核输出”，报告按下表分工整理。

| 成员 | 共同实践 | 报告主笔与整理职责 |
|---|---|---|
| A：刘华彬 | 完整流程及两道练习 | 实验概况、环境、整体流程、验证汇总、总结与最终整合 |
| B：柯云超，2413575 | 完整流程及两道练习 | 练习 1：入口指令的操作、目的及相关观察 |
| C：匡航逸，2412235 | 完整流程及两道练习 | 练习 2：启动跟踪、最初指令的位置和功能 |

Lab1 中的内核完成格式化输出后进入无限循环。两道练习分别从入口代码和实际执行过程出发，解释这个最小内核如何开始运行。

## 2. 环境与整体流程

### 实验环境

前期于 2026-10-08 使用 Ubuntu 22.04.5 LTS（WSL）、Make 4.3、RISC-V GCC 15.1.0、Binutils 2.45、GDB 16.3 开发构建、QEMU 7.0.0 和 OpenSBI 1.0，完成构建与启动观察，工具版本见 [前期记录](evidence/build-link-20261008/summary.json)。本轮练习 2 在当前 WSL 中重新编译并调试，环境和结果在第 4 节记录。

| 成员 | AI 工具 | 底层模型 | 本次实践环境/差异 |
|---|---|---|---|
| A | Codex | GPT-6（按已有会话记录） | 上述前期环境；本人已确认完整体验，记录待补 |
| B：柯云超 | Codex | GPT-6（本次会话声明） | Ubuntu 22.04（WSL）、Make 4.3、SiFive RISC-V GCC 10.2.0、GDB 10.1、QEMU 4.1.1、OpenSBI 0.4；见 [本人环境记录](evidence/B-20261011/environment.log) 和 [启动日志](evidence/B-20261011/boot.log) |
| C：匡航逸 | Codex | GPT-6 | Ubuntu 24.04.4 LTS（WSL）、RISC-V GCC 13.2.0、QEMU 8.2.2、OpenSBI 1.3、GDB 15.1 |

操作方法见 [代码 README](../code/README.md)。本机运行与调试使用 `-kernel bin/ucore.img`，使 QEMU 为默认 OpenSBI 提供正确的内核入口；相关对照结果见 [交接中的 QEMU 问题说明](A-构建链接交接.md#qemu)。

### 整体流程与核心代码

```text
构建：.c/.S → 编译、汇编为 .o → 按 kernel.ld 链接为 bin/kernel
      → objcopy 转成 bin/ucore.img

运行：QEMU 预加载固件和内核 → 复位代码 → OpenSBI
      → kern_entry → 设置 SP → kern_init → 清零、输出 → 无限循环
```

`bin/kernel` 是 ELF 文件，保留符号和调试信息，供 GDB 对照源代码；`bin/ucore.img` 是由 `objcopy` 转换得到的 bin 文件，按链接后的布局保存需要加载的内容。`kernel.ld` 以 `0x80200000` 为起始地址，安排 `.text`、`.rodata`、`.data` 和 `.bss` 等段，并通过 `ENTRY(kern_entry)` 指定入口点。本次构建中，`kern_entry` 正好位于 `.text` 段开头，对应镜像的第一条指令。

| 核心文件 | 在流程中的作用 |
|---|---|
| `Makefile`、`tools/kernel.ld` | 构建镜像并安排入口和内存布局 |
| `kern/init/entry.S` | 设置内核栈指针并转入 C 函数 |
| `kern/init/init.c` | `kern_init` 清理 `.bss`，打印启动消息，进入无限循环 |
| `kern/libs/stdio.c`、`libs/printfmt.c` | 处理格式字符串并逐字符输出 |
| `kern/driver/console.c`、`libs/sbi.c` | 将字符输出请求交给 OpenSBI |

教材先解决“代码放在哪里、从哪里开始执行”，再解决“内核如何输出”。`entry.S` 设置内核栈后进入 `kern_init`；`kern_init` 通过 `memset` 清零 `.bss`，调用 `cprintf` 输出启动信息，随后进入无限循环。清零范围由链接脚本中的 `edata` 和 `end` 给出，把内存布局与初始化代码联系起来。

输出模块则从 SBI 服务逐层封装。`sbi_call` 将功能编号放入 `a7`，参数放入 `a0`～`a2`，执行 `ecall`，由 M 模式下的 OpenSBI 处理请求。`sbi_console_putchar` 和 `cons_putc` 提供字符输出接口；`cputch` 在输出字符时累计字符数；`vprintfmt` 解析格式字符串；`vcprintf` 和 `cprintf` 组织可变参数并完成格式化输出。源码中的关系为：

```text
cprintf → vcprintf → vprintfmt → cputch → cons_putc
        → sbi_console_putchar → sbi_call → ecall → OpenSBI
```

项目中的 `stdio.h` 对应这套内核输出函数。格式化处理由内核完成，最终的字符输出交给 OpenSBI，构成从 SBI 服务到 `cprintf` 的完整输出过程。

## 3. 练习 1：理解内核启动中的程序入口操作

**报告主笔：柯云超，2413575（B）。**

2026-10-11 完成入口与 C 初始化的源码分析，按本人运行的日志和 6 张终端截图记录构建、入口、栈、清零调用及启动输出。具体命令见 [练习一取证指南](B-练习一取证指南.md)。

### 题目

阅读 `kern/init/entry.S`，结合内核启动流程，说明 `la sp, bootstacktop` 和 `tail kern_init` 分别完成什么操作，目的是什么。

### 分析与回答

```asm
kern_entry:
    la sp, bootstacktop
    tail kern_init
```

#### la sp, bootstacktop：建立内核启动栈

`la` 是 load address 伪指令，将标号 `bootstacktop` 的地址写入 SP（`x2`），使内核使用自己的启动栈。它取得的是地址，不会读取该地址处的数据，也不负责在运行时申请栈空间。栈空间已由 `entry.S` 中的数据段定义预留：

```asm
.section .data
    .align PGSHIFT
    .global bootstack
bootstack:
    .space KSTACKSIZE
    .global bootstacktop
bootstacktop:
```

`mmu.h` 定义 `PGSHIFT=12`、`PGSIZE=4096`，`memlayout.h` 定义 `KSTACKPAGE=2`、`KSTACKSIZE=KSTACKPAGE * PGSIZE`，所以启动栈大小为 8192 字节。在当前 RISC-V GNU 汇编器中，`.align PGSHIFT` 将栈起点按 `2^12=4096` 字节对齐。汇编指示符安排空间和符号，CPU 在入口执行时通过设置 SP 开始使用这段空间。

栈区为 `[bootstack, bootstacktop)`。栈向低地址增长，初始 SP 指向预留区的高地址边界，后续建立栈帧时减小 SP，把保存寄存器、返回地址等所需的空间放在栈区内。页对齐的起点加上两页的大小，也保证栈顶满足标准 RISC-V ABI 在函数调用边界的 16 字节对齐要求。

C 编译器生成的函数可能立即调整 SP，保存寄存器并调用其他函数，因此进入 C 之前需要合法的栈。OpenSBI 交接时 SP 即使非零，也仍可能是固件使用的栈；内核通过这条指令显式建立自己的启动栈。

#### tail kern_init：交接给 C 初始化函数

`tail kern_init` 以 tail call 的方式跳转到 C 语言编写的内核初始化函数，不向 RA（`x1`）写入新的返回地址。普通 `call` 会建立返回到调用位置之后的地址，这里的入口在完成栈设置后直接交出执行权。

`kern_init` 声明为 `noreturn`，完成初始化和输出后进入无限循环，因此不需要返回到 `kern_entry`。`noreturn` 告知编译器函数不会正常返回；当前实现中，末尾的 `while (1) ;` 实际保证了这一点。

`la` 和 `tail` 都是伪指令，最终展开取决于汇编、重定位和链接松弛。已有反汇编中，`la` 对应 `auipc sp,0x3`、`mv sp,sp`，`tail` 对应跳到 `kern_init` 的 `j`；反汇编中的 `mv`、`j` 也可能是基础指令的别名。一次 GDB `si` 执行一条机器指令，不一定执行完一整条源码伪指令。

先设置 SP、再进入 C 函数的顺序体现了两者的依赖关系：汇编入口准备栈，C 初始化函数才能使用栈并进行后续调用。

#### kern_init：清零、输出与无限循环

```c
extern char edata[], end[];
memset(edata, 0, end - edata);
const char *message = "(THU.CST) os is loading ...\n";
cprintf("%s\n\n", message);
while (1)
    ;
```

`edata` 和 `end` 来自链接脚本的 `PROVIDE`，分别标记初始化数据结束处和 BSS 结束处。C 中使用 `extern char ...[]` 引用边界地址，不会额外分配两段数组。`end - edata` 以字节为单位，`memset` 将 `[edata,end)` 写成零，满足未显式初始化或零初始化的静态存储对象从零开始的 C 语义。

启动栈位于 `.data`，不在这里的 BSS 清零区间内；执行 `memset` 时已经在使用栈。若实际构建得到 `edata == end`，清零长度就是 0，调用不会写入任何 BSS 字节。第 4 节已有记录属于这一情况，本人的结果以重新运行时的边界和参数为准。

随后 `cprintf` 输出启动信息，程序停在无限循环。终端不再增加输出符合当前代码的控制流；字符格式化及 SBI 输出过程见第 2 节。

### 入口观察

入口调试主要观察栈空间、伪指令的实际展开，以及设置 SP 前后的变化。下表沿用第 4 节已有的小组调试记录，便于对照代码；B 的个人实践另列于后，可比较不同工具环境下的实际数值。

| 观察项 | 本次结果 | 日志或截图 |
|---|---|---|
| `bootstack`、`bootstacktop` 与栈大小 | `0x80201000`、`0x80203000`，相差 8192 字节 | [符号记录](evidence/gdb-boot-20261011/symbols.log) |
| `kern_entry` 的实际反汇编 | `auipc sp,0x3`、`mv sp,sp`、`j 0x8020000a` | [入口反汇编](evidence/gdb-boot-20261011/entry-disassembly.log) |
| 设置 SP 前后的变化 | 从 `0x80046eb0` 变为 `0x80203000`，等于 `bootstacktop` | [单步记录](evidence/gdb-boot-20261011/gdb.log) |
| 入口跳转后的 PC | `0x8020000a`，进入 `kern_init` | [单步记录](evidence/gdb-boot-20261011/gdb.log) |

第 4 节记录了本轮调试中执行入口指令后 PC 和 SP 的变化，可以与这两条伪指令的含义对应起来。

### B 本人运行记录与截图

2026-10-11 本人按 [练习一取证指南](B-练习一取证指南.md) 执行构建和 GDB 命令，记录保存在 `evidence/B-20261011/`。环境见 [environment.log](evidence/B-20261011/environment.log)，入口、栈和 C 初始化的完整输出见 [gdb.log](evidence/B-20261011/gdb.log)，补充普通启动的输出见 [boot.log](evidence/B-20261011/boot.log)。下表数值来自这些记录。

| 观察项 | 本人实际结果 | 截图文件名 |
|---|---|---|
| 构建、入口反汇编与关键符号 | make 完成链接及镜像转换；入口为 `0x80200000`；反汇编为 auipc、mv、j；栈边界为 `0x80201000`、`0x80203000` | `B-01-build-symbols.png` |
| 入口执行前的 PC、SP、RA 与栈边界 | `pc=0x80200000`、`sp=0x8001bd80`、`ra=0x80000a02`；栈大小 8192 字节 | `B-02-entry-before.png`；完整边界见 GDB 日志 |
| 执行完整 la 展开后的 SP | `pc=0x80200008`、`sp=0x80203000`；SP 与栈顶相等的比较为 1，低四位为 `0x0` | `B-03-stack-after.png` |
| tail 跳转到 kern_init 首地址 | `pc=0x8020000a`、`sp=0x80203000`；RA 保持 `0x80000a02`，比较为 1；`edata=end=0x80203008`，长度 0 | `B-04-enter-c.png` |
| memset 的真实调用参数与返回 | `a0=0x80203008`、`a1=0`、`a2=0`；三项参数比较均为 1；返回后 `pc=0x80200026`、`sp=0x80202ff0` | `B-05-bss-init.png` |
| 内核启动输出 | OpenSBI 0.4 启动后出现 `(THU.CST) os is loading ...`；普通启动使用 5 秒 timeout 结束观察 | `B-06-boot.png` |

构建的 [build.log](evidence/B-20261011/build.log) 记录了链接和 objcopy，并未显示本次逐个编译所有源文件，因此本轮不记为干净重建。入口反汇编和关键符号分别见 [entry-disassembly.log](evidence/B-20261011/entry-disassembly.log)、[symbols.log](evidence/B-20261011/symbols.log)。

以下截图由本人操作终端后保存。第 2 张底部未显示全部栈边界，缺少的边界和大小可从同次 GDB 日志核对。

![B：显示本人构建日志、入口反汇编和关键符号](images/B-01-build-symbols.png)

![B：入口执行前的 PC、SP、RA 和入口反汇编](images/B-02-entry-before.png)

![B：执行完整 la 后 SP 等于栈顶并满足 16 字节对齐](images/B-03-stack-after.png)

![B：尾调用到 C 函数首地址，RA 不变，BSS 长度为零](images/B-04-enter-c.png)

![B：memset 参数比较及返回 kern_init 的位置](images/B-05-bss-init.png)

![B：普通启动中的 OpenSBI 信息与内核消息](images/B-06-boot.png)

截图 1 是本人将已有原始日志重新显示到终端后截取的结果。截图 6 对应调试结束后另做的一次普通启动，命令为 `timeout -k 2s 5s make qemu < /dev/null`，并通过 tee 保存 boot.log。末尾的 Terminated 对应超时终止；判断启动成功的依据是实际内核消息，不以超时命令作为内核正常退出。

这些观察说明，入口先切换到预留的内核栈，再通过不改写 RA 的跳转进入 C 函数。进入 C 后，函数序言将 SP 减小 16 字节，符合向低地址建立栈帧的行为。本次 BSS 长度为零，截图 5 证明的是零长度 memset 调用及返回，未观察到非空 BSS 内容改变。

本次实际请求和处理过程见 [提示词记录 8](prompt.md#记录-8练习-1-分析与本人取证准备)。练习一的分析、本人调试记录及 6 张截图已齐。`[待本人补充或确认：个人学习收获。]`

## 4. 练习 2：使用 GDB 验证启动流程

**报告主笔：匡航逸，2412235（C）。本轮由 Codex 执行命令并保存调试记录。**

### 题目

使用 GDB 跟踪 QEMU 模拟的 RISC-V 从加电开始到执行内核第一条指令的过程，回答：最初执行的几条指令位于什么地址，主要完成哪些功能？记录调试过程与观察结果。

### 调试环境与方法

本轮使用 Ubuntu 24.04.4 LTS（WSL）、Make 4.3、RISC-V GCC 13.2.0、Binutils 2.42、QEMU 8.2.2、OpenSBI 1.3 和 GDB 15.1。当前环境安装的是 `gdb-multiarch`，它可以通过 `set architecture riscv:rv64` 调试 RISC-V 64 位程序。工具路径与版本见 [本轮汇总](evidence/gdb-boot-20261011/summary.json)。

在 `code/` 中执行 `make -B`，重新编译全部源文件，生成同一轮的 `bin/kernel` 和 `bin/ucore.img`。随后启动 QEMU，使用与 `make debug` 相同的参数：

```bash
qemu-system-riscv64 -machine virt -nographic -bios default \
    -kernel bin/ucore.img -s -S
```

`-S` 使 CPU 在开始执行前暂停，`-s` 开放默认的 GDB 调试端口 1234。GDB 加载 ELF 文件以取得内核符号，并连接 QEMU：

```gdb
file bin/kernel
set architecture riscv:rv64
target remote localhost:1234
set pagination off
info registers pc sp t0 a0 a1 a2
x/6i $pc
x/3i 0x80200000
watch *(unsigned int *)0x80200000
```

先逐条执行复位代码，观察寄存器；进入 OpenSBI 后，在 `0x80200000` 设置断点并继续运行，观察固件完成初始化后的跳转。最后单步执行内核第一条指令，确认内核已经开始运行。完整命令和输出见 [GDB 日志](evidence/gdb-boot-20261011/gdb.log)，本轮命令由 [实验脚本](evidence/gdb-boot-20261011/capture.py) 执行。

### 初始 PC 与最初六条指令

GDB 连接后，初始 PC 为 `0x1000`，SP 为 `0`。`x/6i $pc` 显示：

```asm
0x1000: auipc t0,0x0
0x1004: addi  a2,t0,40
0x1008: csrr  a0,mhartid
0x100c: ld    a1,32(t0)
0x1010: ld    t0,24(t0)
0x1014: jr    t0
```

这些指令位于 MROM（Machine ROM），是本次 QEMU `virt` 平台的复位代码。对每条指令执行一次 `si`，再查看相关寄存器，得到以下结果：

| 指令地址 | 指令 | 操作与执行后的观察 |
|---|---|---|
| `0x1000` | `auipc t0,0x0` | 将当前指令地址与立即数左移 12 位后的值相加。这里立即数为 0，得到 `t0 = 0x1000`，供后续计算地址 |
| `0x1004` | `addi a2,t0,40` | 得到 `a2 = 0x1028`，指向传给 OpenSBI 的启动信息 |
| `0x1008` | `csrr a0,mhartid` | 从 `mhartid` 读取当前 hart ID，得到 `a0 = 0` |
| `0x100c` | `ld a1,32(t0)` | 从 `0x1020` 读取 64 位地址，得到 `a1 = 0x87e00000`，即设备树的地址 |
| `0x1010` | `ld t0,24(t0)` | 从 `0x1018` 读取 OpenSBI 入口，得到 `t0 = 0x80000000` |
| `0x1014` | `jr t0` | 跳转到 `t0` 指向的地址，执行后 PC 为 `0x80000000`，进入 OpenSBI |

这里两条 `ld` 的作用可以通过内存检查直接看出：

```text
(gdb) x/2gx 0x1018
0x1018: 0x0000000080000000  0x0000000087e00000
```

`0x1018` 起的八个字节保存 OpenSBI 入口，`0x1020` 起的八个字节保存设备树地址。指令先从内存取出地址，再将地址用于传参或跳转。

为理解 `a2` 的作用，又检查了 `0x1028` 起的六个 64 位值，依次为 `0x4942534f`、`2`、`0x80200000`、`1`、`0`、`0`。它们对应 OpenSBI 的 `fw_dynamic_info` 中的 `magic`、`version`、`next_addr`、`next_mode`、`options` 和 `boot_hart`。其中 `next_addr` 给出内核入口，`next_mode = 1` 表示下一阶段使用 S 模式。这一对应关系可由 [OpenSBI 1.3 的结构定义](https://github.com/riscv-software-src/opensbi/blob/v1.3/include/sbi/fw_dynamic.h) 和 [QEMU 8.2.2 的复位代码](https://github.com/qemu/qemu/blob/v8.2.2/hw/riscv/boot.c) 核对。

因此，最初六条指令主要完成的是地址计算、参数准备和跳转。执行完后，PC 指向 OpenSBI，`a0`、`a1`、`a2` 分别保存 hart ID、设备树地址和启动信息地址，固件可以据此继续初始化。

### 从 OpenSBI 到内核第一条指令

第六次 `si` 后，在 `0x80000000` 处观察到 OpenSBI 的入口指令：

```asm
0x80000000: add s0,a0,zero
0x80000004: add s1,a1,zero
0x80000008: add s2,a2,zero
```

这三条指令先把复位代码传来的参数保存到 `s0`、`s1`、`s2`。随后设置内核入口断点：

```gdb
break *0x80200000
continue
info registers pc sp a0 a1
x/3i $pc
```

GDB 停在 `kern_entry` 的第一条指令前，PC 为 `0x80200000`，SP 为 `0x80046eb0`，`a0 = 0`，`a1 = 0x87e00000`。QEMU 终端中的 OpenSBI 输出也给出了 `Next Address = 0x80200000`、`Next Arg1 = 0x87e00000` 和 `Next Mode = S-mode`，与入口处的地址和参数对应。

内核入口的反汇编为：

```asm
0x80200000 <kern_entry>:   auipc sp,0x3
0x80200004 <kern_entry+4>: mv    sp,sp
0x80200008 <kern_entry+8>: j     0x8020000a <kern_init>
```

此时已经到达内核入口。再执行一次 `si` 后，PC 变为 `0x80200004`，SP 变为 `0x80203000`，说明 `0x80200000` 处的第一条内核指令已经执行。后续两次单步的结果如下：

| 停止位置 | PC | SP | 含义 |
|---|---|---|---|
| 入口断点 | `0x80200000` | `0x80046eb0` | 尚未执行入口指令，SP 仍指向固件使用的栈 |
| 第一次 `si` 后 | `0x80200004` | `0x80203000` | `auipc` 将 SP 设置为当前 PC 加 `0x3000` |
| 第二次 `si` 后 | `0x80200008` | `0x80203000` | `mv sp,sp` 对应的地址偏移为 0，SP 保持原值，`la` 执行完毕 |
| 第三次 `si` 后 | `0x8020000a` | `0x80203000` | `j` 跳转到 `kern_init`，开始执行 C 初始化函数 |

本次符号地址为 `bootstack = 0x80201000`、`bootstacktop = 0x80203000`，两者相差 `0x2000`，即 8192 字节。SP 的变化说明内核开始执行后会使用自己预留的栈；OpenSBI 之前使用的栈与内核栈分别属于两个启动阶段。

进一步在 `cprintf` 处设置断点，观察到格式字符串为 `"%s\n\n"`，字符串参数为 `"(THU.CST) os is loading ...\n"`。继续执行后，QEMU 输出：

```text
(THU.CST) os is loading ...
```

这与 `kern_init` 的代码一致：清零 `.bss` 范围，输出启动信息，再进入无限循环。本次 `edata` 与 `end` 均为 `0x80203008`，所以 `memset` 的长度为 0。完整输出见 [QEMU 日志](evidence/gdb-boot-20261011/qemu.log)。

### watchpoint 与镜像加载时机

按照练习提示，使用 `watch` 在 `0x80200000` 设置了 watchpoint。继续运行后，GDB 命中了内核入口断点，watchpoint 没有触发。回看初始 PC 仍为 `0x1000` 时的内存，`x/3i 0x80200000` 已经能够反汇编出 `kern_entry` 的三条指令。

随后将初始状态下 `0x80200000` 起的 12296 字节与 `bin/ucore.img` 比较，两者完全一致，SHA-256 均为 `36bef3842e20e33667dd8075563b890317fb18f9654ca961dcef4dc145eab84c`。这说明本轮启动中，QEMU 已在 CPU 开始执行前将内核镜像加载到物理内存。教材 [“OpenSBI, bin, elf”](../_book/lab1/lab1_3_1_layout.html) 一节也说明了这一加载方式。

watchpoint 记录设置后被监视内容的变化，而入口断点记录 CPU 到达指定地址的时刻。本轮镜像加载发生在设置 watchpoint 之前，所以调试中观察到的是复位代码跳转到 OpenSBI，再由 OpenSBI 将控制权交给已经位于内存中的内核。

### 问题的答案

在本次 QEMU 模拟的 RISC-V `virt` 平台上，加电复位后最初执行的六条指令位于 MROM 的 `0x1000`～`0x1014`。它们以当前 PC 为基址，准备 hart ID、设备树地址和 OpenSBI 所需的启动信息，读取 `0x80000000` 作为固件入口，再通过 `jr t0` 跳转。OpenSBI 在 M 模式下完成初始化后，将控制权交给 S 模式下位于 `0x80200000` 的内核；单步执行该地址处的指令后，SP 变为 `bootstacktop`，内核开始使用自己的栈。

## 5. 验证结果与全员实践记录

2026-10-08 的构建、入口断点和启动输出分别见 [构建记录](evidence/build-link-20261008/build.log)、[入口记录](evidence/build-link-20261008/entry-gdb.log) 和 [启动记录](evidence/build-link-20261008/boot.log)。2026-10-11 重新进行的启动跟踪见第 4 节。

前期的 [编译截图](images/A-build-20261008.png) 和 [启动截图](images/A-boot-20261008.png) 展示了构建结果及内核输出，本轮的指令和寄存器观察保存在 GDB 文本日志中。

| 本轮观察项 | 结果 | 记录 |
|---|---|---|
| 重新编译 | `make -B` 退出码为 0，生成 ELF 与内核镜像 | [构建日志](evidence/gdb-boot-20261011/build.log) |
| 启动执行流 | `0x1000 → 0x80000000 → 0x80200000` | [GDB 日志](evidence/gdb-boot-20261011/gdb.log) |
| 内核第一条指令 | 执行后 PC 为 `0x80200004`，SP 为 `0x80203000` | [GDB 日志](evidence/gdb-boot-20261011/gdb.log) |
| 初始内存与镜像 | 12296 字节完全一致，watchpoint 未触发 | [实验汇总](evidence/gdb-boot-20261011/summary.json)、[GDB 日志](evidence/gdb-boot-20261011/gdb.log) |
| 内核输出 | 出现 `(THU.CST) os is loading ...` | [QEMU 日志](evidence/gdb-boot-20261011/qemu.log) |

本轮调试使用现有内核代码，源码内容与编译前一致。实验脚本和 GDB 命令一并保存在 `evidence/gdb-boot-20261011/` 中。在仓库根目录的 WSL 终端执行以下命令，可以重新取得构建与调试记录：

```bash
python3 report/evidence/gdb-boot-20261011/capture.py
```

| 成员 | 完成/实践日期 | 构建与输出 | 复位到入口跟踪 | 栈与 C 入口观察 | 本人记录 |
|---|---|---|---|---|---|
| A | 2026-10-08；完整实践日期待补 | 本人已确认完成 | 本人已确认完成 | 本人已确认完成 | 前期记录见本节 |
| B：柯云超，2413575 | 2026-10-11 | 链接、镜像转换和内核输出已有日志及截图 | 已查看初始复位指令；完整单步待补 | 已记录 SP、RA、C 入口和 memset 参数 | 第 3 节已补练习一实测值及全部 6 张截图 |
| C：匡航逸，2412235 | 2026/10/09 | 见第 4 节 | 见第 4 节 | 见第 4 节 | 2026-10-11 的补充调试记录见第 4 节 |

个人实践记录包括构建与输出、复位指令单步、内核入口以及栈指针的观察。

## 6. 总结与答辩准备

### 与操作系统原理的联系

| 本实验知识 | 含义及与原理的联系、差异 |
|---|---|
| 链接与加载 | 链接脚本规定各段和符号的位置，QEMU 将固件和内核镜像放入物理内存。它们共同使运行时地址与代码中的地址一致；相比一般用户程序的加载，本实验使用固定的物理地址 |
| 启动与特权级 | 复位代码将执行权交给 M 模式下的 OpenSBI，固件初始化后启动 S 模式下的内核。OS 原理中的特权级用于限制不同软件的操作权限；这里首先观察的是固件与内核之间的分工 |
| 栈与函数调用 | 栈用于保存局部变量和函数调用中的状态。入口汇编先设置 SP，使 C 函数能够使用内核栈；本实验只有一份静态预留的内核栈，进程各自的内核栈将在后续实验中涉及 |
| SBI 与异常 | 内核执行 `ecall` 后，由 M 模式下的 OpenSBI 提供字符输出服务。这体现了通过异常进入更高特权级的机制；用户程序通过系统调用进入内核的机制与之相关，但调用双方和接口不同 |
| 调试与执行流 | PC、反汇编和断点把源码中的启动顺序对应到 CPU 的实际执行。单步观察地址计算和跳转，比仅看到启动字符串更能说明内核是如何获得执行权的 |

OS 原理中还有几组重要内容将在后续实验中展开：物理内存分配与虚拟内存管理负责管理和隔离地址空间；进程、线程和调度负责组织计算任务；中断处理、同步互斥与进程间通信负责处理异步事件和并发；文件系统负责持久化数据。Lab1 的最小内核尚处在启动和输出阶段。

### 实验思考与 AI 协作

启动过程中，复位地址、固件地址和内核入口分别对应不同阶段。只观察最终的输出，很容易把这些阶段合在一起；把 PC 的变化与跳转指令对应起来后，才能看清每一阶段如何为下一阶段准备入口和参数。

Lab0.5 强调将描述转化为可观察的结果。本次让 AI 阅读教材和实际代码、整理 GDB 命令，再用寄存器与内存记录核对解释。完整提示词见 [prompt.md 的记录 7](prompt.md#记录-7练习-2-的-gdb-跟踪与报告)。个人收获由各成员在实践后补充。

这次调试中最有帮助的观察是入口前后的 SP：固件已经有自己的栈，但内核仍要设置 `bootstacktop`。它使“为 C 代码准备运行环境”从一句概括变成了可以通过寄存器变化理解的过程。
