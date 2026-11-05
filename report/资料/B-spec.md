# B：中断与时钟 spec

负责人：柯云超，2413575。基线：`origin/lab3` 的 `7df2517`；日期：2026-11-04。

## 实现约定

1. 输入是汇编入口保存的 `struct trapframe *tf`。`trap_dispatch()` 已根据 cause 最高位分流；处理函数清除该位后，以低位原因编号分类，不能把整个 cause 直接与 `IRQ_S_TIMER` 比较。
2. 仅在 `trap.c` 的 B 区域增加所需声明、常量和 `interrupt_handler()` 实现，不修改 A 的汇编、公共结构体、`trap()`、`trap_dispatch()` 或 C 的异常函数。
3. 框架 `riscv.h` 提供 IRQ 0～13。分别输出用户、监督者、旧版 Hypervisor、机器级的软件/定时器/外部中断提示，以及旧版 coprocessor/host 提示。它们是原因分类分支，不等于本实验已实现对应设备驱动。未知编号输出 `Unknown interrupt: cause N`，并调用 `print_trapframe(tf)`。
4. `IRQ_S_TIMER` 每次先调用 `clock_set_next_event()`，再将 `ticks` 加一。仅在 `ticks % TICK_NUM == 0` 时调用已有 `print_ticks()`，保持输出 `100 ticks`。
5. 教师框架只有 `TICK_NUM=100`，未给出 `SHUTDOWN_AFTER` 的数值。本实现补充 `SHUTDOWN_AFTER=10`（周期输出次数）：第 1000 次中断先输出第十行，再调用 `sbi_shutdown()`。用 `ticks / TICK_NUM` 推导累计输出数，不引入独立静态计数状态。
6. 中断处理不修改 `tf->epc` 或其他帧字段。中断后继续原执行位置。不在处理函数内重新开启全局中断，不实现嵌套中断。
7. 保留 `lab3` 标记，B 的实现标记使用 `2413575`。不把尚未完成的 C 标记替换为 B 的学号。

## 验收方法

| 层级 | 方法 | 可证明的内容 |
| --- | --- | --- |
| 函数级 | 从真实源码提取 B 函数，在宿主 C 测试中替代打印、时钟和关机接口 | 全部分类、未知兜底、99/100/101 和 999/1000 边界、调用顺序、完整帧不变 |
| 正式 QEMU | 正式内核 `make qemu` 自然运行至关机 | A 入口/恢复 + B 真实时钟、连续重设、十次输出和 SBI 关机 |
| GDB | 在真实时钟处理的入口与 `__trapret` 比较帧和 ticks | cause、原 epc 保持、99→100、100→101 等边界；不以截图代替自动断言 |
| 教师评分 | 原样运行 `make grade` | 记录全部组分结果；C 未实现时不得报告 100/100 |
| A 回归 | 原样运行 `check_a.py` 的临时处理桩验证 | A 保存/恢复未退化；此项使用测试桩，不代表 C 正式实现完成 |

验证脚本新增在 `code/tools/check_b.py`，日志保存到 `report/evidence/B-20261104/`。截图由本人按操作指南采集，本轮仅放文字占位符。
