# Lab 1：A 模块交接

负责人：刘华彬，2411238。验证日期：2026-10-08（北京时间）。

## 交接结果

构建、链接、镜像模块已独立完成分析和验证。`code/` 的所有受版本管理文件 SHA-256 前后相同，内核源码、Makefile 和链接脚本均未改动。当前生成目录来自本轮 `make clean` 后的完整构建，正常入口和内核输出已实测，下一位可以直接运行或调试。

内层仓库分支为 `lab1`，验证基准提交为 `55fd23080d86f0612db463a1ac417ea2dbadfc09`。本轮新增报告、记录和截图尚未提交；没有配置远程或执行 push。外层原有修改不属于本轮交付。

## 直接使用

```bash
source /home/liu/projects/nku-os/scripts/env.sh
cd /home/liu/projects/nku-os/labs/code
make
make qemu
```

内核会显示 `(THU.CST) os is loading ...` 并进入死循环。退出 QEMU：`Ctrl+a`，松开后按 `x`。

调试时，第一个终端：

```bash
source /home/liu/projects/nku-os/scripts/env.sh
cd /home/liu/projects/nku-os/labs/code
make debug
```

第二个终端同样加载环境、进入 `labs/code`，执行 `make gdb`。默认调试端口为 1234，需要空闲。

```gdb
break *0x80200000
continue
x/3i $pc
```

当前 Makefile 已使用 `-kernel bin/ucore.img`，沿用它即可。不要根据教材旧示例改回 loader 参数。`bin/kernel` 用于 GDB 符号，`bin/ucore.img` 用于运行，两者不能混淆。

## 本轮基准产物

| 项目 | 本轮值 |
|---|---|
| ELF 大小 | 44904 字节 |
| 镜像大小 | 12296 字节，`0x3008` |
| ELF 入口、镜像首指令地址 | `0x80200000` |
| `kern_init` | `0x8020000a` |
| 内核栈范围 | `[0x80201000, 0x80203000)`，8192 字节 |
| `edata`、`end` | 均为 `0x80203008`，当前 BSS 为空 |

```text
SHA-256 bin/kernel
1f1a202ccea24f60828fa714d8ec4f0ccc7d2b4a9106141b6f6c58f2265093eb

SHA-256 bin/ucore.img
ac486354d148e98079ffeca22efe89c7b3cbcd4c9fa05a9ee2a4039d8080a8bf
```

地址、大小和哈希只对应本轮源码、工具和构建目录；修改代码、编译选项或工具后应重新验证。`bin/`、`obj/` 是被忽略的生成目录，不需要提交。

## 已完成的验证

- 清理后完整构建成功，退出码 0；再次 make 不重复构建。
- ELF64 小端、RISC-V、入口正确，最终无未解析符号和剩余重定位。
- map 验证 `entry.o` 的 `.text` 位于最前；诊断链接与正常 ELF 全字节相同。
- 根据 ELF 可加载段内容和间隙重建的裸镜像与实际镜像全字节相同；重复 objcopy 转换也一致。
- 在 `make debug` 的初始暂停处，PC 为 `0x1000`，镜像范围内存已与文件一致；继续运行后命中 `0x80200000` 入口断点。
- 普通启动显示 OpenSBI 1.0、Next Address `0x80200000`、S-mode 和内核信息。
- 本轮创建的 QEMU 和取证 xterm 已关闭，调试端口已释放；未终止其他进程。

证据目录：[evidence/build-link-20261008](evidence/build-link-20261008/summary.json)。保留了原始构建、ELF、对象重定位、链接 map、段删除、入口 GDB、启动及失败尝试日志。

真实截图：[编译截图](images/A-build-20261008.png)、[运行截图](images/A-boot-20261008.png)。报告片段：[A-构建链接与镜像](fragments/A-构建链接与镜像.md)。实际提示词：[prompt.md](prompt.md#记录-1a1-构建链接与镜像)。

## 给 B / C 的事实与边界

| 下一成员 | 可以使用的工程事实 | 应自行完成的部分 |
|---|---|---|
| B：入口与初始化 | 栈范围、入口符号、反汇编、`edata=end` | 单步验证 `la` 实际展开、SP 变化、跳转与 C 初始化，回答练习 1 |
| C：固件与输出 | 复位暂停时镜像已加载、入口可命中、输出可运行 | 分析复位首批指令，跟踪 OpenSBI 阶段，完整回答练习 2，核对 SBI 输出链 |

当前入口实际指令为 `auipc sp,0x3`、`mv sp,sp`、跳转至 `kern_init`。这只是当前构建结果，不应当作所有工具链下的固定展开。

`ENTRY(kern_entry)` 指定 ELF 元数据中的入口，但不自动重排函数；当前镜像第一条指令正确还依赖实际段布局与对象顺序。若修改链接输入，必须复核。`cons_getc` 对未实现输入服务的引用被未使用段删除，本轮只证明当前启动和输出路径可用。

## 验证辅助脚本与真实问题

`evidence/build-link-20261008/` 下保存了本轮 `build-terminal.sh`、`verify.py` 和 `boot-terminal.sh`。它们是证据辅助文件，不是内核实现或正式评分脚本。脚本路径固定到当前工作区，重跑会覆盖相同名字的日志；保存新一轮证据时先另设目录并更新路径，不覆盖本轮记录。

首次取证脚本使用 `timeout ... make qemu | tee ...`，只有超时终止消息，没有启动信息，判为失败并保留。随后加 `< /dev/null` 明确非交互输入，成功取得输出和截图。不能只凭 124 说测试通过，也不能把终端输入问题写成内核修复。本轮未获取首次停止信号，具体终端原因仍为推断。

日常交互运行用普通 `make qemu`。需要自动保存输出时可用：

```bash
set -o pipefail
timeout -k 2s 5s make qemu < /dev/null 2>&1 | tee /tmp/lab1-boot.log
```

该命令预期超时码为 124；仍须检查内核输出。它没有执行评分测试。

## 待完成

A 的技术模块、真实日志和两张截图已完成，并经负责人审阅后整合到 [主报告](report.md)。主报告保留 B/C 模块、正式练习、验证与总结的待填写位置；环境、构建和调试方法见 [代码 README](../code/README.md)。B/C 仍需取得自己的证据并完成报告，最后共同整合。原始片段的 `../images/...`、`../evidence/...` 链接保留，主报告中的链接已经按所在目录调整。

当前 `tools/grade.sh` 缺失，不执行 `make grade`：该目标会先清理生成目录，再调用缺失脚本。没有评分通过截图。公开远程仓库和最终提交、推送仍待小组交付阶段处理。

本次整合只修改文档，未重新执行实验。`evidence/build-link-20261008/SHA256SUMS` 保留 A 取证完成时的历史快照；其中 `prompt.md` 和本交接文档的哈希属于更新前版本，原始证据和图片不改写。
