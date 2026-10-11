# Lab 1：最小可执行内核

实验仓库使用 `lab1` 分支，代码在 `code/`，报告在 `report/`。

先配置 GNU Make、RISC-V 裸机工具链和 QEMU，然后在本目录运行：

```bash
make
make qemu
```

看到 `(THU.CST) os is loading ...` 后进入无限循环是预期行为。退出 QEMU：Ctrl+A，松开后 X。

调试时两个终端分别执行 `make debug`、`make gdb`。`bin/`、`obj/` 为生成目录，可用 `make clean` 清理，再用 `make` 重建。

2026-10-11 的练习 2 在 Ubuntu 24.04.4 LTS（WSL）中使用 QEMU 8.2.2、OpenSBI 1.3 和 `gdb-multiarch` 15.1 完成。该环境的工具安装命令为：

```bash
sudo apt-get install --no-install-recommends \
    gcc-riscv64-unknown-elf binutils-riscv64-unknown-elf \
    qemu-system-misc gdb-multiarch
```

使用 `gdb-multiarch` 时，在另一个终端进入 `code/` 并执行：

```bash
gdb-multiarch -q bin/kernel \
    -ex 'set architecture riscv:rv64' \
    -ex 'target remote localhost:1234'
```

完整的复位单步、入口断点和寄存器记录见 [练习 2](../report/report.md#4-练习-2使用-gdb-验证启动流程)。在仓库根目录的 WSL 终端执行 `python3 report/evidence/gdb-boot-20261011/capture.py`，可以重新编译并保存同样的调试记录。

环境配置、完整操作、QEMU 固件差异与启动参数原因、成员分工统一见 [实验交接](../report/A-构建链接交接.md)。

- [实验报告](../report/report.md)：围绕两道练习填写。
- [提示词记录](../report/prompt.md)：主要任务按 Lab0.5 四段式整理。

当前缺少 `tools/grade.sh`，不运行 `make grade`。
