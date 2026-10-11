# Lab 1 交接：操作、QEMU 问题与报告分工

更新：2026-10-11。本页集中维护当前实验所需的环境、操作、问题说明和交接安排。

## 1. 当前状态与分工

Lab1 以两道练习为主体：解释入口指令，以及用 GDB 跟踪复位到内核入口。全员完整体验流程并理解两题，按章节主笔。

| 成员 | 状态 | 报告职责 |
|---|---|---|
| A：刘华彬，2411238 | 本人已确认完整阅读手册和体验操作，已有前期验证材料 | 公共章节、验证汇总、总结和整合 |
| B：姓名学号待补 | 尚未开始 | 练习 1：两条入口指令的操作、目的和观察 |
| C：姓名学号待补 | 尚未开始 | 练习 2：启动跟踪、最初指令的位置和功能 |

B/C 可分别开始，无须顺序等待。报告见 [report.md](report.md)，主要提示词按 Lab0.5 四段式整理到 [prompt.md](prompt.md)，补充追问可用自然语言。本人操作记录与 AI 自动验证分别注明，完成后再填写实践状态。

## 2. 环境与准备

已验证环境：Ubuntu 22.04.5 LTS（WSL）、Make 4.3、RISC-V GCC 15.1.0、Binutils 2.45、GDB 16.3 开发构建、QEMU 7.0.0、OpenSBI 1.0。精确版本和工具路径见 [历史验证汇总](evidence/build-link-20261008/summary.json)。

本机终端执行：

```bash
source /home/liu/projects/nku-os/scripts/env.sh
cd /home/liu/projects/nku-os/labs/code
```

`env.sh` 将已有工具链和 QEMU 加入 PATH，本机 `.bashrc` 已配置加载它。其他成员按指导书准备工具，在自己的 Linux/WSL 终端设置实际工具路径，并进入实验仓库的 `code/`：

```bash
export RISCV="/实际安装路径/riscv-elf-toolchains"
export PATH="$RISCV/bin:$PATH"
command -v make riscv64-unknown-elf-gcc riscv64-unknown-elf-gdb qemu-system-riscv64
```

如 QEMU 安装在自选目录，也将其可执行文件目录加入 PATH。外层 `_book` 和环境脚本不随内层 `labs` 仓库克隆，资料需另行取得。

`tools/kernel.ld` 按手册示例使用；本实验没有新增内核功能。当前缺少 `tools/grade.sh`，不执行 `make grade`，也不声称评分通过。

## 3. 每个人完整操作一遍

### 构建与运行

在 `code/` 执行：

```bash
make
make qemu
```

构建生成 `bin/kernel`（带调试信息的 ELF）和 `bin/ucore.img`（运行用裸镜像）。应看到 `(THU.CST) os is loading ...`，然后无限循环。退出 QEMU：按 Ctrl+A，松开后按 X。

如需观察完整重建，可先 `make clean` 再 `make`。调试时 ELF 和加载镜像必须来自同一轮构建；重新编译后重新启动 QEMU。

### 启动跟踪与入口单步

两个终端均进入同一份 `code/`，终端 1 执行 `make debug`，终端 2 执行 `make gdb`。初始无输出是 `-S` 暂停 CPU 的结果；`-s` 开放默认调试端口 1234。

在 GDB 中：

```gdb
set pagination off
info registers pc
x/6i $pc
x/3i 0x80200000
```

本机预期初始 PC 为 `0x1000`。阅读六条复位指令，解释它们如何准备参数和跳转地址，再逐条执行：

```gdb
si
si
si
si
si
si
info registers pc a0 a1 a2
```

本机六条指令执行后预期 PC 为 `0x80000000`，进入 OpenSBI。不同环境以实际指令和结果为准。

```gdb
break *0x80200000
continue
info registers pc sp
x/3i $pc
p/x &bootstack
p/x &bootstacktop
```

应停在 `kern_entry` 第一条指令前。继续逐条 `si`，每次用 `info registers pc sp` 观察 SP 何时成为 `bootstacktop`，以及何时跳到 `kern_init`。伪指令不一定只对应一条机器指令。

```gdb
p/x &edata
p/x &end
break cprintf
continue
x/s $a0
x/s $a1
continue
```

本机预期在 `cprintf` 看到格式字符串与启动消息参数，继续后在 QEMU 终端看到输出。需要结束时先在 GDB 终端按 Ctrl+C，再执行 `detach`、`quit`；QEMU 终端用 Ctrl+A、松开后 X 退出。

### 保存必要记录

每人新建 `report/evidence/<成员>-<日期>/`，记录环境、关键命令、观察与疑问；真实截图放 `report/images/`。在 GDB 开始观察前可执行以下命令，路径替换为已创建的个人目录：

```gdb
set logging file ../report/evidence/<成员>-<日期>/gdb.log
set logging enabled on
```

结束时 `set logging enabled off`。报告使用代表性截图即可，不必重复粘贴三套相同输出。

<a id="qemu"></a>

## 4. QEMU 为什么需要调整启动参数

### 现象与修改

手册使用 `-bios default` 配合：

```make
-device loader,file=$(UCOREIMG),addr=0x80200000
```

本机能够看到 OpenSBI，但 Next Address 为 0，没有进入内核。因此 Makefile 的 `qemu`、`debug` 两个目标统一使用：

```make
-kernel $(UCOREIMG)
```

这属于本机启动环境适配。普通运行与调试保持同一加载方式，`debug` 额外保留 `-s -S`。

### 根本原因：默认固件不同

启动需要同时满足“镜像已放入内存”和“固件知道下一阶段入口”。`-device loader` 中的 `addr` 只指定本例裸镜像放置位置，不能自动成为动态固件的交接地址。

| 配置 | 默认 RV64 固件 | 内核入口来源 |
|---|---|---|
| 手册安装示例对应的上游 QEMU 4.1.1 | `fw_jump` | 构建时预设；旧 qemu/virt 的 RV64 配置为 `0x80200000` |
| 本机 QEMU 7.0.0 | `fw_dynamic` | 启动时接收前一阶段提供的信息 |

旧组合的固件已知道跳转位置；本机则需要 QEMU 传入入口。QEMU 7.0 的 virt 启动流程在没有 `-kernel` 时将 `kernel_entry` 设为 0，随后把该值写入固件动态信息；使用 `-kernel` 后会加载镜像并提供正确入口。相同的 `-bios default` 在不同版本下不一定选择相同固件。

依据：[QEMU 4.1.1 默认固件](https://github.com/qemu/qemu/blob/v4.1.1/hw/riscv/virt.c#L43-L47)、[旧 RV64 跳转地址](https://github.com/riscv-software-src/opensbi/blob/v0.4/platform/qemu/virt/config.mk#L23-L32)、[QEMU 7.0 默认固件](https://github.com/qemu/qemu/blob/v7.0.0/include/hw/riscv/boot.h#L24-L25)、[内核入口选择](https://github.com/qemu/qemu/blob/v7.0.0/hw/riscv/virt.c#L1224-L1246)、[固件入口传递](https://github.com/qemu/qemu/blob/v7.0.0/hw/riscv/boot.c#L228-L259)。

Lab0 展示 OpenSBI 0.4，Lab1 展示 0.6，示例并未锁定一套完整配置。以上旧版结论来自源码核对，未在本机重装实跑旧 QEMU；不能把本机适配理解成所有环境都必须修改。保留本机现有配置时，继续使用已经验证的 `-kernel`。

### 同镜像验证结果

2026-10-11 重新构建后，使用同一镜像、相同平台和默认固件，仅替换加载参数：

| 参数 | Next Address | 内核输出 |
|---|---|---|
| loader | `0x0000000000000000` | 无 |
| `-kernel` | `0x0000000080200000` | 有 |

证据：[构建](evidence/boot-parameters-20261011/build.log)、[loader 运行](evidence/boot-parameters-20261011/loader.log)、[kernel 运行](evidence/boot-parameters-20261011/kernel.log)、[汇总](evidence/boot-parameters-20261011/summary.json)。两次均由 `timeout` 结束，124 只是超时码，判断依据是入口信息和内核输出。

练习 2 中也要区分加载与执行：本机初始 PC 为 `0x1000` 时镜像已在内存，写监视点可能不触发，应以初始内存检查和执行断点验证启动过程。

## 5. 剩余交付

B 在主报告第 3 节补练习 1 的实际观察与结论；C 在第 4 节补启动记录及最初指令解释。A 汇总环境与证据，全组补齐姓名学号、AI 工具模型、实践日期和个人收获，核对两题并准备答辩。

已有证据仅保留关键构建、布局、入口、运行和失败记录，以及 QEMU 参数对照；旧模块分析、独立启动说明和环境流水记录已合并清理。历史提示词中的旧路径说明当时任务，当前操作以本页为准。

提交前核对：两题答全、成员状态真实、图片日志可访问，所有成员能解释全流程。Git 提交与推送尚需按小组最终交付安排执行。
