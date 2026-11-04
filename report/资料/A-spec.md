# Lab3 A：Trap 入口与恢复约定

日期：2026-11-04。姓名：匡航逸，2412235。

布局由教师框架的 `SAVE_ALL` 和 `struct trapframe` 确定。RV64 下每个槽占 8 字节，32 个通用寄存器槽后依次保存 `sstatus`、`sepc`、`stval` 和 `scause`，总长 288 字节。进入 Trap 前的 sp 由 `sscratch` 暂存，再写入第 2 槽。

`RESTORE_ALL` 以当前 `sp` 为帧基址，先恢复 `sstatus` 和 `sepc`，再恢复 x1、x3～x31，最后从第 2 槽恢复 x2。x0 恒为零；`badvaddr` 和 `cause` 用于分类和诊断。C 处理函数对帧内 `epc` 的修改应成为实际返回地址。

`__alltraps` 执行 `SAVE_ALL` 后，将帧地址通过 a0 传给 `trap(struct trapframe *tf)`。调用时保持 16 字节栈对齐。C 函数返回后进入 `__trapret`，执行恢复宏及 `sret`。已有 `SAVE_ALL`、结构体布局和公共函数签名保持原样。

Lab2 迁移范围为 `page_init()`、Best-Fit 和启动恒等映射，页分配外层使用 Lab3 的关中断保护。正式代码中的 B/C 处理函数保留待填标记；独立验证在临时副本中加入处理桩，完成异常地址更新和时钟重设。

验证分为保存与恢复两部分。保存阶段比较入口处的通用寄存器和帧内值，检查 C 入口 a0、字段偏移与栈对齐；恢复阶段比较 `sret` 前后的寄存器、CSR 和 PC。触发样例覆盖 16/32 位断点、16/32 位非法指令及连续三次时钟中断，并检查返回特权级为 S 模式。日志、反汇编及环境记录写入 `report/evidence/A-20261104/`。
