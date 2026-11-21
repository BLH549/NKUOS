# Lab4 C 部分提示词与迭代记录

AI 工具为 Codex，模型系列为 GPT-6.2。

## 最终提示词

> 阅读本地 `_book/lab4/` 和 `labs/report/资料/Lab4-任务分工.md`，在 `labs/code` 的 `lab4` 分支完成 C 部分。先检查 A 的页映射、B 的线程创建和已经迁入的 Lab3 Trap 路径。保留 PCB、context、Trap frame 的布局及教师评分文件。
>
> 实现 `cpu_idle`、`schedule`、`proc_run` 和 RV64 `switch_to`。关中断后清除重调度标记，从当前线程的后继或 idle 的链表头查找 runnable，正确跨过哨兵、环绕并处理当前线程重选和 idle 回退。给选中线程增加 `runs`。在 `proc_run` 的临界区里将 `current`、Sv39 SATP 和 context 切到目标，使用物理根页号与全量 `sfence.vma`，避免调用旧的 Sv32 `lsatp`。汇编按 `ra, sp, s0–s11` 的布局保存与恢复。通过本线程栈上的标记恢复中断状态。
>
> 核对 Lab3 的 Trap 入口、恢复和处理函数，保留 SUM、`forkrets` 和正常的时钟计数。验证新线程首次经 `forkret → forkrets → __trapret → kernel_thread_entry` 运行。跑正常构建、启动和教师评分，在临时副本中验证空就绪集合、自选、重复让出、线程栈和寄存器、不同页表对同一虚拟地址的翻译、SIE 嵌套恢复以及异常和定时器返回。记录真实命令、失败与修复，并完成开关中断 Challenge。

## 实际迭代

| 步骤 | 现象与处理 |
| --- | --- |
| 检查基线和约束 | A/B 已合入，C 的三个标记处为空。将每个接口的中断边界、链表终止条件和 RV64 槽位写成规格。用剩余组员的学号标注 C。 |
| 接通调度链 | 编译链接通过。普通 `make qemu` 在本机 QEMU 7 只显示 OpenSBI，`Domain0 Next Address` 为 0。通过外部适配脚本将内核镜像交给 QEMU 的 `-kernel`，入口变为 `0x80200000`，普通演示成功运行到 `do_exit()`。 |
| 运行教师评分 | GCC 15 将教师测试里 `void (*)(void *)` 传给 `kernel_thread(int (*)(void *), ...)` 报为编译错误。保留教师源码及线程接口，通过命令行 `DEFS=-Wno-error=incompatible-pointer-types` 将该诊断降为警告，评分得到 30/30。 |
| 修复补充测试首次启动 | AI 生成的测试驱动最初将探针放在 `kern/init/`。原始镜像以物理基址启动，而按文件名链接时探针排在 `entry.S` 之前，QEMU 给出正确的 OpenSBI 下一级地址后仍无内核输出。`nm` 对比确认启动符号移位。把探针移到临时副本的 `kern/schedule/`，`kern_entry` 恢复为链接基址，16 次轮换与 Trap 测试通过。 |

[默认 QEMU 启动](../evidence/C-lab4/C-qemu.log) · [GCC 15 初次评分](../evidence/C-lab4/C-grade-gcc15-default.log) · [独立用例首次启动](../evidence/C-lab4/C-fixture-first-run.log) · [探针位置与符号对照](../evidence/C-lab4/C-fixture-placement.log)
