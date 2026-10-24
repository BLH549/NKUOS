# Lab2：物理内存初始化与页分配

源码在 `code/`，报告入口为 [report.md](../report/report.md)。A 部分由匡航逸（2412235）完成：DTB 内存初始化、First-Fit 分析与修复、Best-Fit、独立测试和运行证据。

## 构建、运行和管理器选择

需要 GNU Make、RISC-V 裸机 GCC/binutils 和 QEMU。进入本目录执行：

```sh
make                         # 默认 Best-Fit
make qemu                    # Ctrl+A 后松开，再按 X 退出
make PMM=first_fit qemu       # 原始 First-Fit 检查
make PMM=best_fit qemu        # 切回 Best-Fit，不需要手工 clean
make PMM=buddy qemu           # Buddy 检查，随后运行 SLUB 检查
make grade                   # 教师检查，默认内存 128 MiB
make debug                   # QEMU 暂停，监听 localhost:1234
make gdb                     # 另一终端运行
```

成功时输出 `memory management: best_fit_pmm_manager` 和 `check_alloc_page() succeeded!`，之后无限循环。First-Fit 的管理器名为 `default_pmm_manager`。`make grade` 在临时副本注入教师测试，串口输出保存在 `.qemu.out`，不改工作区测试或内核源文件。

默认工具前缀为 `riscv64-unknown-elf-`，可用环境变量或 make 参数覆盖 `GCCPREFIX`、`QEMU`，或用 make 参数覆盖 `GDB`。编译明确使用 RV64、`lp64` 和 `-msmall-data-limit=0`；本启动入口没有初始化 `gp`。

## A 的 macOS arm64 环境

本次实测 GNU Make 3.81、xPack GCC 15.2.0 / binutils 2.45、xPack QEMU 9.2.4（自带 OpenSBI 1.5.1）。工具位于仓库外的 `~/.local/share/nkuos-tools/`；通过 [GCC 发布页](https://github.com/xpack-dev-tools/riscv-none-elf-gcc-xpack/releases/tag/v15.2.0-1) 和 [QEMU 发布页](https://github.com/xpack-dev-tools/qemu-riscv-xpack/releases/tag/v9.2.4-1) 下载对应 `darwin-arm64.tar.gz`，对照同页 `.sha` 校验后解压即可。

```sh
export GCCPREFIX="$HOME/.local/share/nkuos-tools/xpack-riscv-none-elf-gcc-15.2.0-1/bin/riscv-none-elf-"
export QEMU="$HOME/.local/share/nkuos-tools/xpack-qemu-riscv-9.2.4-1/bin/qemu-system-riscv64"
make
make qemu
make grade
```

工具目录和下载包不提交 Git。没有修改系统 PATH 或安装包管理器。

## 独立测试与证据重现

```sh
make check-host              # 系统 C 编译器，ASan + UBSan，无需交叉工具链
python3 tools/check_qemu.py   # 真实重编译两个管理器，并运行 5 组 QEMU 启动
```

本机系统 SDK 的默认选择与链接器不匹配，主机检查需指定已装的兼容 SDK：

```sh
SDKROOT=/Library/Developer/CommandLineTools/SDKs/MacOSX26.5.sdk make check-host
```

其他系统按其正常工具链配置运行即可。主机检查直接编译实际分配器和 `page_init()`，只替换 DTB 输入、内存映射缓冲区与注册回调。QEMU 脚本接受 `GCCPREFIX`、`QEMU`、`MAKE` 环境变量，默认将日志和源码/镜像校验值写入 `../report/evidence/A-20261023/`，可用 `--output PATH` 指定新目录。

当前实现管理首段 RAM `[0x80000000, min(DTB_END, 0x88000000))`。256 MiB RAM 的启动验证覆盖截断行为；教师评分脚本的 `npage` 断言只适用于未被截断的默认配置。DTB 解析器沿用实验简化版本，通用多段 RAM 与任意 reserved-memory 解析不在本次实现范围内。C 的 QEMU 源码调试仍按 [任务分工](../report/Lab2-任务分工.md) 接续。

## B 的 WSL Ubuntu 22.04 环境与 Buddy/SLUB

B 由柯云超（2413575）完成；使用本机已有 SiFive GCC 10.2、QEMU 4.1.1、GNU Make 4.3、主机 GCC 12.3。旧版交叉工具链必须使用 `ARCH=rv64imac`：

```sh
source ~/.local/opt/riscv/env.sh
export ARCH=rv64imac
make PMM=buddy qemu
make check-b
python3 tools/check_b_qemu.py --arch rv64imac
```

Buddy 将请求向上取整为 2 的幂；释放时传原始请求，例如 `alloc_pages(3)` 对应 `free_pages(p,3)`，实际占用和归还 4 页。实现使用绝对物理页号计算伙伴，不改变 Page 布局。

SLUB 在三种页分配器上均可运行，提供 `slub_alloc/slub_free`，支持 1～2048 字节、16 字节对齐；空 slab 立即归还页面。仅支持单 hart 顺序调用，未实现 Linux 完整 SLUB 的每 CPU 缓存等机制。

`make check-b` 使用真实源码进行 ASan/UBSan 检查：Buddy 20,000 次独立位图操作，SLUB 在三种 PMM 上各 5,000 次随机对象操作，另含异常释放与耗尽测试。`check_b_qemu.py` 保存五次真实启动、教师评分、工具版本与源码/镜像哈希，结束时重建默认 Best-Fit；可用 `--output PATH` 指定新证据目录。教师 20/20 没有覆盖 Buddy/SLUB，新模块须看独立测试与内核自检。

见 [B 报告](../report/B-Buddy与SLUB.md)、[截图指南](../report/B-截图操作指南.md)、[截图核对清单](../report/B-截图核对清单.md)。2026-10-24 已补入六张本人运行截图，覆盖七项内容（B-04/B-05 合并），并关联五份本人运行日志。

起始代码来源：`lab2.zip` 的 `lab2_new/`，压缩包 SHA-256 为 `48402e9b2dba7cf4726060a0bda3fa0c57e4f7be66f76ecf11851e181f546d6c`。Lab1 的材料保存在 `lab1` 分支。
