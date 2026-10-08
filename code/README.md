# Lab 1：最小可执行内核

本仓库使用 `lab1` 分支，交付结构为 `code/` 和 `report/`。A（刘华彬，2411238）已完成构建、链接与镜像验证并合入 [主报告](../report/report.md)；B/C 接续入口初始化和固件启动/输出分析。

## 环境配置

在 Linux 或 WSL 的 Linux 终端中准备 GNU Make、RISC-V 裸机工具链和 `qemu-system-riscv64`。工具链应包含 `riscv64-unknown-elf-gcc`、`ld`、`objcopy`、`objdump`、`readelf`、`nm` 和支持 RV64 的 GDB；QEMU 应支持 `virt` 平台与默认 OpenSBI。

A 已验证的环境为 Ubuntu 22.04.5 LTS（WSL）、Make 4.3、GCC 15.1.0、Binutils 2.45、GDB 16.3.90.20250610-git、QEMU 7.0.0、OpenSBI 1.0。版本和路径见 [A 原始记录](../report/evidence/build-link-20261008/summary.json)。这是已验证组合，其他组合由使用者实际构建和运行确认。

### 在朋友自己的机器上配置

按课程环境手册安装或解压适合 Linux 宿主架构的裸机工具链，将下面的示例路径换成实际安装根目录：

```bash
export RISCV="/path/to/riscv-elf-toolchains"
export PATH="$RISCV/bin:$PATH"
```

如 QEMU 安装在自选目录，再将它的 `bin/` 加入 PATH。这些设置可以放入 `~/.bashrc`，然后在交互 Bash 中执行 `source ~/.bashrc`。自动化脚本应明确设置 PATH，不能假定它会读取交互终端配置。

在两个调试终端中分别检查：

```bash
command -v make riscv64-unknown-elf-gcc riscv64-unknown-elf-gdb qemu-system-riscv64
make --version
riscv64-unknown-elf-gcc --version
riscv64-unknown-elf-ld --version
riscv64-unknown-elf-gdb --version
qemu-system-riscv64 --version
```

Makefile 默认调用 `riscv64-unknown-elf-` 工具链，不需要将另一套编译器重命名。代码 README 和内层仓库不依赖原机器的个人环境脚本。

### 在原工作区配置

若继续使用 `/home/liu/projects/nku-os` 中的现有环境，可以直接执行：

```bash
source /home/liu/projects/nku-os/scripts/env.sh
cd /home/liu/projects/nku-os/labs/code
```

该脚本位于仓库外，不会随朋友克隆 `labs` 仓库取得。外层 `_book` 和报告模板也属于课程资料，朋友需要自行取得；报告和 A 证据的链接均留在本仓库内。

## 进入分支与构建

从小组仓库根目录开始：

```bash
git switch lab1
cd code
make
```

构建关系为：

```text
.c / .S → obj/ 下的 .o → ld + tools/kernel.ld → bin/kernel
        → objcopy --strip-all -O binary → bin/ucore.img
```

| 产物 | 用途 |
|---|---|
| `bin/kernel` | ELF，可供 GDB 读取符号和调试信息 |
| `bin/ucore.img` | 裸镜像，供 QEMU 加载 |
| `obj/**/*.o`、`obj/**/*.d` | 编译对象和头文件依赖 |
| `obj/kernel.asm`、`obj/kernel.sym` | 反汇编和符号表 |

`bin/`、`obj/` 由 [.gitignore](.gitignore) 忽略，朋友克隆后自行构建。A 的入口为 `0x80200000`；[链接脚本](tools/kernel.ld)和实际对象布局共同决定入口位置。

要查看完整编译命令或全量重建，可执行：

```bash
make clean
make -j2 V=
```

`make clean` 删除生成目录。调试过程中保持 ELF 与 QEMU 内存来自同一份构建；如重新编译，先退出当前调试会话，再用新产物重新启动 QEMU。

## 启动与保存输出

在 `code/` 目录执行：

```bash
make qemu
```

当前 [Makefile](Makefile) 使用 `-machine virt -nographic -bios default -kernel bin/ucore.img`。沿用当前启动方式；教材旧示例的 loader 参数与 A 的验证环境存在差异。

预期先出现 OpenSBI 信息，再出现：

```text
(THU.CST) os is loading ...
```

之后无限循环是当前代码设计。退出 QEMU：先按 `Ctrl+a`，松开后按 `x`。

需要自动保存输出时，可在仓库的 `report/evidence/` 下为自己的验证创建新目录，例如 B 从 `code/` 执行：

```bash
mkdir -p ../report/evidence/B
set -o pipefail
timeout -k 2s 5s make qemu < /dev/null 2>&1 | tee ../report/evidence/B/boot.log
```

这个自动化命令在观察后以超时码 124 结束；须实际检查内核输出，不能只凭退出码判断成功。A 首次终端管道取证没有启动输出，随后给验证命令增加 `< /dev/null` 才成功；失败记录已保留，内核未因此修改。日常交互运行使用普通 `make qemu`。

## 双终端调试

两个终端都配置工具 PATH，并进入同一份仓库的 `code/`。

终端 1：

```bash
make debug
```

`-S` 使 CPU 在复位后暂停，`-s` 开放 GDB 的本机 1234 端口。此时没有内核输出是正常的；若端口被占用，先确认是否是自己的上一次调试会话。

终端 2：

```bash
make gdb
```

该目标加载 `bin/kernel`，设置 `riscv:rv64` 架构并连接 `localhost:1234`。连接后先观察初始状态，再执行入口断点：

```gdb
set pagination off
info registers pc
x/10i $pc
x/3i 0x80200000
break *0x80200000
continue
info registers pc sp
x/3i $pc
```

B 可在入口处用 `si` 按真实机器指令单步，并对照栈顶符号：

```gdb
p/x &bootstacktop
si
info registers pc sp
```

一次 `si` 不一定执行完整一条伪指令。C 应在自己的新会话中，从初始暂停状态观察复位指令，按阶段在 `0x80000000`（OpenSBI）和 `0x80200000`（内核入口）设断点；实际观察以自己的工具和产物为准。

A 已观察到初始 PC 为 `0x1000` 时镜像就已在内存中。因此，入口写监视点不触发不等于启动失败，应分别检查镜像是否已加载和 CPU 是否执行到入口。上面的命令是操作指南，不替代 B/C 的实际观察和练习答案。

保存 GDB 文本记录时，连接后、开始观察前执行（B/C 用自己的目录）：

```gdb
set logging file ../report/evidence/B/gdb.log
set logging overwrite off
set logging enabled on
```

先在 shell 中创建该目录；使用其他版本 GDB 时按其命令支持情况调整。结束记录可用 `set logging enabled off`。退出 GDB 后也关闭本次 QEMU，释放调试端口。截图应捕获真实终端，放入 `report/images/`；文本日志不能渲染成图片冒充截图。

## B/C 接续与报告

| 成员 | 接续任务 | 主报告填写位置 |
|---|---|---|
| B | 入口、栈、C 初始化的独立验证；回答 `la sp, bootstacktop` 和 `tail kern_init` 的操作与目的 | 模块 B、练习 1、验证与总结 |
| C | 从复位到内核首条指令的跟踪；解释最初指令的地址和功能；分析 SBI 输出链 | 模块 C、练习 2、验证与总结 |

开始前阅读 [A 交接](../report/A-构建链接交接.md)与 [A 片段](../report/fragments/A-构建链接与镜像.md)。本轮基准地址、哈希和日志见 [A 证据汇总](../report/evidence/build-link-20261008/summary.json)，用于定位和对照；B/C 仍须取得自己的证据。

各成员保留真实任务提示词、追问和纠错，按发生顺序追加到 [prompt.md](../report/prompt.md)。主 [report.md](../report/report.md) 已合入 A 成果并保留 B/C 待完成位置，新增日志和截图用相对路径链接。源码或工具改变后另存新证据，A 的原始日志、图片和校验清单作为历史快照保留；其中脚本包含原机器路径，直接重跑会覆盖旧记录。

最终共同补齐 B/C 学号姓名、实际工具模型、全组完成日期与总结。文档整合阶段没有替朋友完成 B/C 实验。

## 当前限制

目前未提供 `tools/grade.sh`，不能宣称自动评分通过。`make grade` 会先清理产物，再调用缺失脚本，本阶段不运行该目标。Lab 1 文档没有列正式 Challenge。

远程地址按小组实际仓库使用，不在 README 中虚构 URL。提交与推送由全组完成并核对后按课程要求处理；本次文档整理不执行 Git 提交或推送。
