# Lab1 运行与调试

本工程来自课程发放的 `lab1.zip`。原有内核源码、链接脚本和 Makefile 保持原样；新增 `tools/boot.gdb` 和 `tools/verify_lab1.py`，用于记录和复现启动验证。报告位于同一仓库的 `report/report.md`。

## 环境

已验证的环境为 WSL2 / Ubuntu 22.04，SiFive RISC-V GCC 10.2.0、GDB 10.1、QEMU 4.1.1、GNU Make 4.3、Python 3.10.12。使用其他版本时，以实际反汇编和寄存器值为准。

确认以下命令可以运行：

```bash
riscv64-unknown-elf-gcc --version
riscv64-unknown-elf-gdb --version
qemu-system-riscv64 --version
```

## 编译运行

在本目录执行：

```bash
make
make qemu
```

预期显示 OpenSBI 信息及 `(THU.CST) os is loading ...`。内核随后进入无限循环，这是原代码的行为。退出 QEMU 时先按 `Ctrl+A`，松开后按 `X`。

## 交互式 GDB

终端一在本目录执行：

```bash
make debug
```

终端二在同一目录执行：

```bash
make gdb
```

常用观察命令：

```gdb
info registers pc sp
x/6i $pc
hbreak *0x80000000
continue
delete breakpoints
hbreak *0x80200000
continue
info registers pc sp ra a0 a1
p/x &bootstacktop
si
si
info registers pc sp ra
```

也可以在终端一执行 `make debug` 后，在终端二使用完整验证命令：

```bash
riscv64-unknown-elf-gdb --batch -x tools/boot.gdb
```

`boot.gdb` 要求连接时 CPU 尚停在复位地址 `0x1000`。一次验证结束后，应结束这次 QEMU 实例，重新 `make debug` 再验证。

## 自动记录验证

先退出占用 1234 端口的调试实例，再执行：

```bash
python3 tools/verify_lab1.py
```

脚本按顺序清理构建产物、编译、检查 ELF/符号、运行 QEMU 和执行 GDB 跟踪，将真实命令输出保存到 `../report/logs/`。它会主动结束自己启动的 QEMU 实例。运行成功表示编译、启动和五项 GDB 观察通过。它不替代成员的手工调试与截图。

Lab1 的练习页要求入口分析和 GDB 启动验证，没有提供本实验的评分脚本。按当前实验安排不执行 `make grade`；原始 Makefile 中的遗留目标保留原样。

## 成员实测截图

报告目前使用明确标注的占位图。按 [操作与截图指南](../report/操作与截图指南.md) 在两个终端逐步执行，再用成员自己截取的终端图片替换占位图。
