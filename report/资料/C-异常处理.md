# C：异常处理与执行恢复

负责人：刘华彬 2411238。完成日期：2026-11-05。

## 1. 异常如何返回原执行流

异常由当前执行的指令触发，`scause` 给出原因，`sepc` 指向触发指令。A 的入口将这两个 CSR 保存为 `tf->cause` 和 `tf->epc`，`trap_dispatch()` 再把同步异常交给 `exception_handler()`。因此，异常函数既要判断发生了什么，也要决定返回之后从哪里执行。

本实验对非法指令和断点采取跳过触发指令、继续执行的策略。若只打印提示而不修改 `tf->epc`，`RESTORE_ALL` 会把旧地址重新写回 `sepc`，`sret` 后便再次执行相同指令，形成异常循环。处理函数修改的是内存中的 Trap frame；随后 A 的恢复代码将新值写回处理器，最终 PC 才会改变。

中断则与当前指令无关。B 的时钟处理保持 `epc` 不变，重设下一次事件并更新计数。本次异常和时钟共用同一个入口及返回路径，但对返回地址的处理不同。

## 2. 实现

只在 `code/kern/trap/trap.c` 中补齐 `exception_handler()`，保留 `lab3` 注释并填入学号 `2411238`。

| 原因 | 处理 |
| --- | --- |
| `CAUSE_ILLEGAL_INSTRUCTION` | 输出 `Illegal instruction at` 和原 epc，按触发指令长度前进 |
| `CAUSE_BREAKPOINT` | 输出 `Breakpoint at` 和原 epc，按触发指令长度前进 |
| 其他异常 | 调用已有 `print_trapframe(tf)`，保留现场 |

RISC-V 的 C 扩展允许 16 位指令与 32 位指令混排。本实验从 `tf->epc` 读取一个 `unsigned short`，检查最低两位：不为 `11` 时跳过 2 字节，否则对本实验的 32 位指令跳过 4 字节。读取半字可以处理只按两字节对齐的 32 位触发点，避免为了判断长度直接读取整字。编码依据见 [RISC-V 非特权规范](https://docs.riscv.org/reference/isa/v20240411/_attachments/riscv-unprivileged.pdf)。

长度判断放在两个需要恢复的异常分支中，未知异常不会读取 `epc`。因此，即使诊断现场中的 `epc` 不可读，也能进入帧输出。兜底输出只用于诊断；对于持续触发的其他异常，不额外承诺恢复执行。

本次范围是 RV64IMAC 下可读的内核代码和 16/32 位触发指令，不包含用户地址安全取指、大于 32 位指令解码、缺页修复、非法指令模拟或系统调用。

## 3. spec 与提示词

先确定输入现场、允许修改的字段、提示输出和验收方式，再实现代码。[C-spec.md](C-spec.md) 规定仅更新 `epc`，其他现场字段不变；提示词要求根据实际编码判断长度，并用正式处理函数验证恢复结果。原始任务输入和本次组织的四段式约束见 [prompt.md](../prompt.md)。

实现后首次构建失败，原因是本机 GCC 15.1 配套汇编器不接受框架使用的旧 CSR 名称 `sbadaddr`。在 Makefile 增加 `-Dsbadaddr=stval`，让预处理器映射到 CSR 0x143 的现用名称。该改动保留 A 的 `SAVE_ALL` 正文、保存布局和寄存器行为。原始错误保存在 [initial-build-failure.log](../evidence/C-20261105/initial-build-failure.log)。异常函数本身未在本轮验证中出现失败。

## 4. 验证方法

复现命令（本机工作区）：

```bash
source /home/liu/projects/nku-os/scripts/env.sh
cd /home/liu/projects/nku-os/labs/code
make check-c
python3 tools/check_b.py --output ../report/evidence/C-20261105/b-regression
make grade
make -j4
make qemu
```

### 函数级检查

`check_c.py` 从正式源码提取异常函数，使用真实 `trap.h` 编译宿主机测试。六组已处理场景检查两种异常原因、两类 16 位首半字和两字节对齐的 32 位编码，逐字节比较整个帧，确认仅 `epc` 改变；同时核对完整提示字符串及原地址。

另用十三种非本实验恢复的原因构造现场，故意将 `epc` 设为不可读地址 1，验证仅调用一次帧输出、完全保留现场、不会取指。这些是函数级输入检查；真实异常原因与恢复行为由下一组 QEMU/GDB 测试验证。结果见 [host.log](../evidence/C-20261105/host.log)。

### 正式函数的真实异常

复用 A 的汇编触发样例与 GDB 逐项检查，新增 `--production-handlers` 模式，保留正式 B/C 函数，临时副本仅插入触发代码和结束标记。A 默认使用最小处理桩的验证方式继续保留。`make check-c` 使用正式模式。

四个触发点分别为 `0x9002`、`0x00100073`、`0x0000` 和 `0x0000007b`。GDB 对照触发符号读取实际编码，记录硬件原因、入口现场和处理后的帧；在真正的 `sret` 返回目标设置断点，比较全部 32 个整数寄存器槽与返回 PC、S 特权级。随后验证三次连续时钟返回，执行结束标记后关机。

| 样例 | 原 epc | 更新后 epc | 结果 |
| --- | --- | --- | --- |
| 16 位断点 | `0xffffffffc02000f0` | `0xffffffffc02000f2` | 通过 |
| 32 位断点 | `0xffffffffc02000f4` | `0xffffffffc02000f8` | 通过 |
| 16 位非法指令 | `0xffffffffc02000fa` | `0xffffffffc02000fc` | 通过 |
| 32 位非法指令 | `0xffffffffc02000fe` | `0xffffffffc0200102` | 通过 |

串口中四条提示均只出现一次，输出地址与 GDB 记录的原 epc 一致，随后到达结束标记。三次时钟返回保持原 epc，七次返回的全部整数寄存器均通过比较。地址对应本次测试镜像，重新构建后以新日志为准。

证据：[GDB 日志](../evidence/C-20261105/context/trap-gdb.log)、[逐次结果](../evidence/C-20261105/context/trap-summary.json)、[反汇编](../evidence/C-20261105/context/fixture-disassembly.log)、[串口输出](../evidence/C-20261105/context/fixture-qemu.log)、[源码 SHA-256](../evidence/C-20261105/tested-source.json)。

### 集成回归

B 的既有检查脚本在独立的 C 证据目录运行。中断分类、计数边界、十次输出后关机、真实 GDB 中的帧和 epc 保持均通过，结果见 [B 回归汇总](../evidence/C-20261105/b-regression/summary.json)。教师评分和最终正式构建结果统一记录在 [报告入口](../report.md)。

## 5. 与 OS 原理的联系

| 知识点 | 原理与实验的关系 |
| --- | --- |
| 同步异常与异步中断 | 异常绑定当前指令，时钟中断与当前指令无关；分类决定是否改变返回地址 |
| 现场与控制流恢复 | 帧内 epc 是软件能修改的返回状态，经恢复汇编写入 sepc，sret 后形成真实 PC |
| 指令集与操作系统接口 | 软件必须结合实际指令编码计算返回位置，不能统一使用固定偏移 |
| 特权级与保护 | 实测为内核 S → S；用户态陷入还需要栈切换和用户地址检查 |
| 异常恢复策略 | 跳过非法指令是实验约定；一般系统还会模拟指令、终止进程或通知用户态 |

本实验没有实现进程调度、用户态信号、需求分页、嵌套异常处理和多核同步。关中断保护适用于本实验单核临界区，不能替代多核锁。

## 6. 证据使用

上述记录由本轮实际构建、QEMU 和 GDB 运行产生。日志可以直接复现和核对；本人操作截图、完整演示和答辩准备由本人确认。本轮没有将程序生成的图片作为本人截图，也没有把原 A 的最小处理桩结果当作正式异常函数通过的证据。
