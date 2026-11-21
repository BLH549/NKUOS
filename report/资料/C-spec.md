# Lab4 C 部分规格

对应指导书的功能模块三与开关中断 Challenge。以 `lab4` 的 A/B 集成版本 `690308b` 为基线。

## 依赖和接口

| 接口 | 约定 |
| --- | --- |
| `cpu_idle()` | 启动完成后在 idle 的 bootstack 上进入空闲循环。`current->need_resched` 置位时调用 `schedule()`。 |
| `schedule()` | 保存并屏蔽本 hart 的中断，清除调用者的重调度标记。idle 从链表头之后查找，普通线程从自己的后继查找，跨过哨兵节点并环绕一轮。在环绕时可重新选中仍为 RUNNABLE 的当前线程。无候选时回退到 idle。递增选中线程的 `runs`，需要切换时调用 `proc_run()`，调用者继续执行时恢复其原中断状态。 |
| `proc_run(proc)` | 当前线程与目标相同时直接返回。否则在本 hart 关中断期间切换 `current`，以 `proc->pgdir` 的物理页号组成 RV64 Sv39 SATP，以 ASID 0 执行全量 `sfence.vma`，再调用 `switch_to()`。旧线程被再次选中后恢复自己的中断状态。目标页表须保留内核映射，使切换代码、PCB 和栈在两套映射下均可访问。 |
| `switch_to(from, to)` | 以 `a0/a1` 接收两个 context 指针。依照 PCB 中的布局保存和读取 14 个 RV64 槽位，顺序为 `ra, sp, s0–s11`。恢复后的 `ret` 跳往目标的首次入口或此前的调用续点。 |
| Lab3 Trap 路径 | 按现有 `struct trapframe` 核对保存、传参和恢复。同步异常可修正帧中的 EPC，时钟中断重设定时器并计数。普通 Trap 和 `forkrets` 共用 `__trapret`，由 `sret` 恢复特权级及中断使能。Lab4 的时钟中断可持续返回。 |

生产内核的调度点为 `cpu_idle()`。教师评分及补充用例中的线程直接调用 `schedule()` 来主动让出 CPU。Lab4 的内核线程默认共用启动根页表。补充用例单独构造不同的根页表，并保留相同的高地址内核映射。

## 验收

- `make` 链接成功，普通启动输出 PID 1、问候语和告别语，之后进入框架 `do_exit()` 的调试监视器。
- 教师注入的 PMM、Process Create 和 Schedule 三项全部通过。
- 独立用例覆盖空链表、链表中无 runnable、当前线程自选、直接调用 `proc_run()`、两个线程有序轮换及回到 idle。
- 对照 `current`、SATP、实际 SP、目标栈区间及保存的 context。两个根页表对同一虚拟地址映射不同值，切换后逐次进行真实读取。
- 两线程反复让出后逐项核对 `s0–s11`、栈上局部变量、`need_resched` 和 SIE。验证嵌套关中断、16/32 位异常返回及超过 100 次定时器中断。
