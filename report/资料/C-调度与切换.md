# Lab4 C 部分 · 调度与上下文切换

2026-11-21，`lab4` 分支。C 部分以 A 的页映射和 B 的内核线程创建为基础，实现 idle 调度、全局进程链表轮换、Sv39 页表切换和 RV64 context 恢复。教师评分三项通过，得分 **30/30**。独立用例又验证了两个内核线程的 16 次有序让出、不同根页表的实际访存和中断状态恢复。代码中的 2411238 由小组名单及本次 A、B 的标注推定。[规格](C-spec.md) · [提示词与真实迭代](C-prompts.md)

## 调度执行链

内核初始化内存、Trap、VMA 和进程管理后启用时钟中断并进入 `cpu_idle()`。B 部分建立的 idle 已置位 `need_resched`，于是 idle 主动调用 `schedule()`。调度器先清除这个标记。当前为 idle 时从链表头之后开始，普通线程从自己之后开始，跨过哨兵继续查找。环绕时若只有当前线程可运行，就增加它的 `runs` 并继续执行。全部候选都无法运行时，选择 idle。由于 B 的 `list_add()` 在表头插入，创建顺序和首次扫描顺序可能相反，后续按链表顺序轮换。

发生实际切换时，`proc_run()` 先保存当前 PCB 指针，然后设置 `current`。目标 PCB 的 `pgdir` 已是物理地址，将它右移 12 位得到根页号，再用 `((uintptr_t)SATP_MODE_SV39 << 60)` 设置模式位。Lab4 使用 ASID 0，写入 SATP 后用 `flush_tlb()` 执行全量 `sfence.vma`。切换过程中两套页表都需要能访问高地址的内核指令、PCB 和栈，因为 SATP 在 SP 切换前就生效。

`switch_to()` 按 8 字节槽保存和恢复 `ra`、`sp` 和 `s0–s11`，共 112 字节。调用者保存寄存器由编译器按照函数调用约定处理。普通恢复从旧线程先前的 `switch_to()` 调用点返回。新线程的初始 `ra` 指向 `forkret`，初始 `sp` 指向栈顶 Trap frame。第一次 `ret` 后经 `forkret → forkrets → __trapret → sret` 到达 `kernel_thread_entry`，把 B 放在 Trap frame 中的 `s1` 作为参数传给 `s0` 指定的线程函数。

Lab3 迁移代码已经实现 `__alltraps` 和 `RESTORE_ALL`。普通 Trap 把被打断的通用寄存器及 CSR 存入 36 个 8 字节槽位的帧，将帧地址传给 `trap()`。恢复时从帧中写回 `sstatus` 和 `sepc`，最后载入 `sp` 再执行 `sret`。保留这条路径以及 `idt_init()` 的 SUM 设置，移除 `DEBUG_GRADE` 下第 100 个 tick 进入 panic 的 Lab3 遗留分支。时钟例程继续预约下一次事件。

## 开关中断 Challenge

`local_intr_save(flag)` 调用 `__intr_save()`，读取当前 hart 的 `sstatus.SIE`。若该位原来为 1，就调用 `intr_disable()` 清零并返回 true。已经关闭时直接返回 false。`local_intr_restore(flag)` 在标记为 true 时用 `intr_enable()` 置回 SIE，其余情况保持当前的屏蔽状态。SIE 的保存依赖每层有自己的标记，且临界区内部遵守关闭状态。

嵌套时若外层进入时开中断，外层保存 true 并关闭，内层保存 false。内层退出时保持关闭，外层之后才重新开启。若最外层进入时已经关闭，两层退出后仍关闭。实际调度中，`schedule()` 的外层标记属于发起调度的线程，内层 `proc_run()` 看到的 SIE 已经关闭。线程被切出时，这两层函数的调用现场一起挂起，等原线程回到自己的栈上，再按相反顺序恢复。新线程没有对应的旧调用栈，它的初始 SIE 由 `sret` 从初始帧的 SPIE 恢复。

SIE 屏蔽的是本 hart 在 S 态响应的可屏蔽中断。单 hart 中可借此保证选择、`current`、SATP 和 context 切换之间的状态一致。同步异常仍可发生，因此切换依赖正确的内核映射。其他 hart 可以并行访问共享对象，多核版本还需要锁、每 hart 的当前线程状态及跨 hart 的 TLB 协调。

## 测试与结果

环境为 WSL2 Ubuntu 22.04.5、RISC-V GCC 15.1.0、QEMU 7.0.0 / OpenSBI 1.0。测试时的分支基线、内核源码和教师文件校验值见记录。[源码记录](../evidence/C-lab4/C-source.txt)

从 `labs` 运行独立用例，随后进入 `code` 执行构建与评分。适配脚本将 QEMU 7 的 loader 参数转换成 `-kernel`，从而让 OpenSBI 取得正确的下一级地址。GCC 15 的参数仅将教师回调签名的诊断降为警告。教师脚本和用例按原样执行。

```bash
report/evidence/C-lab4/verify.sh
cd code
make
adapter=$(realpath ../report/evidence/C-lab4/qemu-kernel.sh)
timeout 5s make QEMU="$adapter" qemu
make QEMU="$adapter" DEFS=-Wno-error=incompatible-pointer-types grade
```

| 检查 | 实测结果 |
| --- | --- |
| 正式内核 | `make` 成功。兼容参数下普通启动打印 `this initproc, pid = 1, name = "init"` 和两行演示消息，随后到达框架 `do_exit()` 的 panic 和调试监视器。 |
| 教师评分 | `[Test PMM] PASS`、`[Test Process Create] PASS`、`[Test Schedule] PASS`，总分 30/30。 |
| 选择分支 | 空链表和全睡眠时回退到 idle。仅自己 runnable 时维持当前 context、增加 `runs` 并清除 `need_resched`。验证了直接调用 `proc_run()` 的 SIE 恢复。 |
| 多线程和地址翻译 | PID 4 与 PID 3 轮流让出共 16 次。每次恢复比较全部 `s0–s11`、栈上变量、当前 PCB、SP、SATP 和此前设置的 SIE。两线程访问同一 VA `0x400000`，各自都读到对应物理页的值。最后两个 worker 睡眠并切回 idle。 |
| Trap | 两个断点的 EPC 间隔 2 字节，随后非法指令的 EPC 再前进 4 字节。返回后 `t0/a0` 仍为探针预设值。以 `-DDEBUG_GRADE` 编译的独立内核经过 101 次定时器中断后继续执行并输出 PASS。 |

[正式构建](../evidence/C-lab4/C-build.log) · [普通启动](../evidence/C-lab4/C-qemu-kernel.log) · [评分输出](../evidence/C-lab4/C-grade.log) · [教师注入版串口](../evidence/C-lab4/C-grade-serial.log) · [独立用例串口](../evidence/C-lab4/C-fixture.log)

| 线程 | 根页表物理地址 | SATP | 恢复后的实际 SP |
| --- | --- | --- | --- |
| PID 4 | `0x8041b000` | `0x800000000008041b` | `0xffffffffc0420f80` |
| PID 3 | `0x80415000` | `0x8000000000080415` | `0xffffffffc041af80` |
| PID 0（idle） | `0x80209000` | `0x8000000000080209` | `0xffffffffc0208fa0` |

独立用例记录的两个 worker 首次运行时，`context.ra` 等于本次 ELF 的 `forkret`，`context.sp` 比各自的栈顶低 `0x120`。反复恢复后，`context.ra` 改为切换调用续点，`s0` 和 `s11` 分别等于当轮探针种子和种子加 11。串口日志还记录了结果标记、符号地址及 `runs`。[调度和 Trap 日志摘录](../evidence/C-lab4/C-verification.png)

## OS 原理与答辩

- PCB 提供调度状态，独立内核栈与 context 保存执行进度，MMU 用 SATP 选择地址翻译。调度决策、执行现场与地址翻译必须指向同一个运行对象。
- 主动切换发生在已知的函数调用边界，context 依据 ABI 保存恢复所需的寄存器。Trap 可以打断任意一条指令，恢复路径需要保留更多寄存器和 CSR。首次线程运行复用了 Trap 返回机制来设置起始 EPC、参数和 SIE。
- 当前轮换只在显式调度点发生。时间片抢占、优先级、用户地址空间和 U/S 栈切换、多核同步及完整退出回收由后续机制承担。普通演示结束后的 `process exit!!.` 是框架行为。
- 答辩时可以沿 `switch_to` 的第一个 `ret` 讲到启动 Trap frame，再用两个 worker 的 SATP、SP 和 SIE 日志说明后续恢复。将第二个断点的半字对齐 EPC 与日志中的地址差对应，可以说明为什么不能固定把异常 EPC 加 4。
