# Lab4 三人任务分工（抽签用 ABC）

依据：2026-11-18 下载的课程指导书与 `lab4.zip`。ABC 仅代表任务包，不绑定姓名、学号或此前 Lab 的分工。此文件是待实施计划。

## 起点与分支

应以当次网站的 Lab4 代码包为代码起点，将以前完成的实现按本次接口重新生成或适配到 `lab2`/`lab3` 标记处。Git 分支可以保留 Lab3 的提交历史，但当前代码树要采用新的 Lab4 框架。这两个“起点”并不矛盾。

已建立本地 `lab4` 分支，独立工作区为 `../../`，其中 `code/` 对应新压缩包的 `lab4_new/`，`report/` 为本次报告。原 `../../../labs/` 仍在 Lab3，保留未提交修改。新分支保存原始教学框架，Lab2/3 空缺和 Lab4 作业均待实现；这不是已经可以运行或已经通过评分的版本。

本次新增 `kmalloc`、VMA、进程结构、`forkrets`、调度和评分注入工具，不能直接复制旧代码树，也不能整文件覆盖本次 `pmm.c`、`trap.c` 或 `trapentry.S`。例如新的 `idt_init()` 设置 `SSTATUS_SUM`，`trapentry.S` 末尾有新线程启动所需的 `forkrets`，都需要保留。

## 总览与工作量

| 任务包 | 实现边界 | 配套任务与验收 | 目标占比 |
| --- | --- | --- | --- |
| A：页映射与内存基线 | `page_insert()`、`page_remove()`；本框架的 Lab2 空缺 | 引用计数/失败路径/权限/TLB 测试；分页思考题；最终回归与报告组装 | 约 1/3 |
| B：线程创建与初始化 | `alloc_proc()`、`proc_init()`、`kernel_thread()`、`do_fork()`、`copy_thread()`、`copy_mm()` | PCB、内核栈与首次运行现场检查；失败清理测试；创建部分报告 | 约 1/3 |
| C：调度与上下文切换 | `cpu_idle()`、`schedule()`、`proc_run()`、`switch_to()`；本框架的 Lab3 空缺 | Sv39 页表切换、重复切换测试；开关中断 Challenge；调度部分报告 | 约 1/3 |

B 的六个函数构成一个完整的资源生命周期，代码与回滚逻辑较多，因此公共收尾主要由 A 承担；C 包含汇编切换和 Trap 适配；A 的两个新函数较少，增加内存基线、边界验证、分页题和整合工作。按实现、调试、分析和收尾一起衡量，不能只按函数数量分。

每人都负责自己的 spec、实际 prompt、迭代记录、测试日志、截图和报告正文。A 只做最终报告的链接和格式组装，不代写 B/C 的内容。

## A：页映射与内存基线

负责文件：`code/kern/mm/pmm.c`、`default_pmm.c` 及确有必要的对应内存头文件；个人测试和报告独立存放。

1. 根据以前的 spec/prompt 补齐 `page_init()` 和分配器等 Lab2 空缺，核对新 `struct Page`、DTB、地址偏移与页表接口。当前包的 `default_pmm.c` 引用了包内不存在的 `best_fit_pmm.h`，并保留 `basic_check()`/`best_fit_check()`：需要补齐匹配的声明/管理器，不能仅粘贴旧 First-Fit 文件或删除教师测试。
2. 实现 `page_insert()`：首次映射、同页重映射、替换其他页、权限更新、引用计数与 TLB 失效；正确处理 `get_pte(..., 1)` 失败，返回 `-E_NO_MEM`。核对已有 `get_pte()` 的分配行为，区分失败后无目标映射与中间页表资源的处理范围，记录实际边界。
3. 实现幂等的 `page_remove()`：无页表/无有效映射时返回；有映射时清除、减少引用计数，必要时释放页面并失效 TLB。
4. 完成分页 Challenge：Sv32/Sv39/Sv48 与 `get_pte()` 两段相似遍历的关系；讨论“查找”和“分配”是否应拆分。
5. 负责最终代码集成验证、证据目录和报告入口组装。

验收：

- [ ] 内存基线可用于其他两人的内核栈与 PCB 分配；保留新框架的 `kmalloc`、页表和 VMA 逻辑。
- [ ] 首次映射、同页改权限、换页、多地址引用同页、重复删除均有验证，引用计数与空闲页变化正确。
- [ ] 分配失败的返回值和状态变化有验证；说明教师测试覆盖之外的边界。
- [ ] 通过框架的页表检查和 `[Test PMM] PASS`。
- [ ] 集成后保存 `make`、`make qemu`、`make grade` 的实际结果及源码版本；未通过时按事实记录。

## B：线程创建与初始化

主要修改 `code/kern/process/proc.c` 的 `//LAB4 function2 YOUR CODE` 区域。六个函数保持在同一任务包，避免跨人协商每一步资源回收。

1. `alloc_proc()`：安全初始化 PCB 的状态、PID、父进程、栈、mm、context、trapframe、标志、名字与链表字段。
2. `copy_mm()`：按本 Lab 的内核线程范围实现接口，不扩展用户空间复制。
3. `kernel_thread()`、`copy_thread()`：组织函数和参数、子线程返回值、栈顶 Trap frame、`context.ra/sp`；与框架 `kernel_thread_entry`、`forkret`、`forkrets` 对接。
4. `do_fork()`：检查进程数量限制，分配 PCB/内核栈，复制现场，分配 PID，设置 parent，登记 hash/list，计数并唤醒。失败时逆序回收，避免部分挂接。
5. `proc_init()`：初始化全局结构，以当前现场建立 idleproc，设置 current，再创建 initproc。保留 idle 的特殊 PID 和 bootstack 约定。

验收：

- [ ] idle/init 的 PID、状态、父子关系、mm、栈和 current 符合框架测试。
- [ ] context 与 Trap frame 的地址、对齐和字段可独立检查；首次入口为 `forkret → forkrets → __trapret → kernel_thread_entry`。
- [ ] PCB/内核栈分配失败时，资源、链表、进程计数不残留错误；可在独立测试版本中注入失败。
- [ ] 可创建多个线程，PID 可查找且参数互不串扰。
- [ ] 通过 `[Test Process Create] PASS`；与 C 联调后线程函数实际收到正确参数。

## C：调度与上下文切换

负责 `proc.c` 的 `//LAB4 function3 YOUR CODE` 区域、`kern/schedule/sched.c`、实际文件名 `kern/process/switch.S`，以及 `kern/trap/` 的 Lab3 待填区域。

1. 参考以前的提示词适配 `RESTORE_ALL`、`__alltraps` 和处理函数，保留本框架的 `forkrets`、SUM 设置及已有公共代码。Lab3 的定时关机属于旧实验验收策略，不应原样搬来提前结束 Lab4 线程实验；中断/异常行为要服从本次执行链路。
2. `cpu_idle()`：根据 `need_resched` 发起调度。
3. `schedule()`：清除重调度标记，按当前链表位置寻找 runnable 线程，无候选时选择 idle；维护 runs，并正确保存和恢复中断状态。
4. `proc_run()`：切换 current、目标页表与 context；使用 Sv39 的 64 位 SATP 写法及 TLB 刷新。本次指导书明确指出旧 `lsatp()` 为 Sv32 遗留，不能直接使用。
5. `switch_to()`：按 `struct context` 实际布局保存/恢复调用约定要求的寄存器，不混同完整 Trap frame。
6. 完成开关中断 Challenge，解释 `local_intr_save/restore` 的状态保存、嵌套及单 hart 适用边界。

验收：

- [ ] Trap 返回和 `forkrets` 新线程首次启动均能使用同一恢复路径。
- [ ] 无其他 runnable、自身再次被选中、多线程轮换均能处理。
- [ ] 记录 `current`、`satp`、栈及关键 context 字段，核对切换一致性。
- [ ] 真实线程反复让出并恢复，局部变量/保存寄存器保持正确，中断原有状态可恢复。
- [ ] 通过 `[Test Schedule] PASS`，说明普通演示的 `do_exit()` 仍可能按框架设计 panic，不把完整进程退出列为本次新增任务。

## 解耦方式与顺序

- 三人从同一个 Lab4 基线开展；A/C 优先交付 Lab2/Lab3 依赖适配，不等报告完成后才共享。
- A 拥有 `pmm.c`；B/C 虽共享 `proc.c`，但按 function2/function3 标记分区，禁止全文件格式化或互相覆盖。
- 固定 `proc.h` 中的 context/PCB 布局、`trap.h` 的 Trap frame 和各函数签名。B 建立新线程现场，C 按该布局恢复；不另行设计私有布局。
- C 沿用框架已有 `wakeup_proc()`，B 直接调用；调度策略与创建机制分开提交。
- 各人使用独立测试文件、测试补丁和报告文件。需要临时构造 PCB/Trap frame 或替代外部接口时只用于局部验证，不提交到教师评分实现中。
- A 可先验证页表；B 可先检查 PCB/栈/失败路径；C 可先检查上下文布局和构造 runnable 队列。完整内核运行仍有必要依赖，最终共同联调，不能宣称三个模块完全独立。
- A 汇总测试与报告；B/C 分别提交自己的正文、截图、日志与问题分析，并各自复现完整流程。

建议正文：`report/资料/A-页映射与内存.md`、`B-线程创建.md`、`C-调度与切换.md`；对应图片、日志使用 ABC 前缀。任务包内的 `YOUR CODE` 在抽签确定成员后替换为实际学号。

## 共同提交要求与评分依据

每人交付实现、spec、最终 prompt、真实迭代和 bug 记录、测试命令/输出、OS 原理对应关系、未覆盖知识点和答辩说明。报告遵循课程模板，AI 工具及模型按真实使用情况填写。

新版 `make grade` 会将源码复制到 `/tmp/lab4_grade`，运行 `tools/inject.py` 注入 `tools/test_lab4.c`，再执行 `tools/grade.sh`。三组测试为 PMM、Process Create、Schedule。保留这些教师文件；不要通过修改测试得到通过结果。同一台机器并行评分会共用该临时路径，应串行运行。

参考：[练习](../../../_book/lab4/lab4_2_1_exercises.html)、[报告要求](../../../_book/lab4/lab4_5_labs_requirement.html)、[报告模板](../../../materials/实验报告模板.md)、[代码包来源](../../../materials/upstream/2026-11-18/manifest.json)。以上任务尚未标记为实现完成。
