# Lab3 提示词记录

日期：2026-11-04（北京时间）。姓名：匡航逸，2412235。AI 工具：Codex，GPT-6。

## 记录 1：切换分支并完成 A

本次输入：

```text
切换到lab3分支，然后完成模块a，文档一定要参考lab2的语言，不要用第三人称或者防御性的语言，理工科语气
```

按 Lab2 的记录格式整理如下：

```text
[PROMPT]
在 lab3 分支完成 Trap 入口与上下文恢复，导入教师 Lab3 框架，
迁移 Lab2 必填实现，保存真实运行和 GDB 记录，撰写 A 的报告与运行说明。
技术说明参照 lab2，按机制、实现过程和观察结果展开。

[RELY]
工作区：E:/NKUOS；源码 code/，报告 report/。
阅读 report/资料/Lab3-任务分工.md、_book/lab3/ 的练习与报告要求。
参考 origin/lab2 中的报告、A 的 spec、page_init、Best-Fit 和启动映射。
教师代码：http://oslab.mobisys.top/lab2026/lab3.zip。
核心接口：SAVE_ALL、struct trapframe、trap(tf)、__alltraps、__trapret、sret。

[SPEC]
保持 SAVE_ALL、Trap frame 布局、trap() 签名及分发函数。
RESTORE_ALL 按实际槽位恢复整数寄存器、sstatus 和 sepc，最后恢复原 sp。
__alltraps 通过 a0 传递当前帧地址，按调用约定进入 C 函数并衔接统一返回。
使用 Lab3 页管理外层接口，保留其中的中断保护。
B/C 的处理函数保留待填标记，A 的处理桩仅加入临时验证副本。

[CHECK]
构建并启动正式基线，检查物理页自检。
用真实 Trap 比较入口、C 调用、恢复入口、sret 和返回目标处的状态。
核对帧大小、偏移、a0、栈对齐、全部整数寄存器、CSR、PC 和特权级。
覆盖 16/32 位断点、16/32 位非法指令及连续三次时钟中断。
保存日志、反汇编、环境、源码校验值和报告中的日志图片。
```

### 实现与验证

检查工作区后，从 `origin/lab3` 建立本地跟踪分支。参考材料包括 Lab3 分工和指导书，以及 `origin/lab2` 中的报告、提示词和已有实现。

起始代码采用教师 `lab3.zip` 中的框架，迁移 `page_init()`、Best-Fit 和临时恒等映射，补齐 `RESTORE_ALL` 与 `__alltraps`。页分配外层使用 Lab3 的中断保护，B/C 处理函数保留待填标记，暂以空函数建立编译基线。

初次编译报告两个内存边界常量缺失，通过新框架的虚拟地址边界和映射偏移补齐。加入汇编触发样例后，测试函数排到裸镜像开头，导致启动未到达 `kern_init`；将内核入口放入 `.text.kern_entry` 后，启动恢复正常。

GDB 在复位阶段读取高地址反汇编时报告内存不可访问，改为先运行到 `kern_init` 再检查返回代码。执行 `sret` 的单步操作停在下一次 Trap 入口后，改用返回地址处的硬件断点检查 PC、整数寄存器和特权级。

临时副本中触发四次指令异常和三次时钟中断，逐次比较全部整数寄存器、帧布局和返回位置，七次返回均通过。最终 `ticks=3`，执行到后续标记，QEMU 正常关机。正式基线的 Best-Fit 自检和时钟初始化通过。

实现约定见 [A-spec.md](资料/A-spec.md)，分析与实验结果见 [report.md](report.md)，连续输出与逐次比较见 [GDB 日志](evidence/A-20261104/trap-gdb.log)和[验证汇总](evidence/A-20261104/trap-summary.json)。

## B：中断与时钟（柯云超，2413575）

本轮用户输入、四段式实现约束和测试约束见 [B-prompts.md](资料/B-prompts.md)，实现前的接口与验收约定见 [B-spec.md](资料/B-spec.md)。正式完成 `interrupt_handler()`，C 的 `exception_handler()` 保留待填。环境兼容问题、测试桩声明修复及真实验证结果见 [B 报告](资料/B-中断与时钟.md)。本轮属于同一个 Codex 会话，结构化提示词为输入和实现约束的整理，不冒充额外的独立模型调用。
