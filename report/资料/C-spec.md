# C：异常处理实现约定

负责人：刘华彬 2411238。日期：2026-11-05。

## 输入与边界

`exception_handler(struct trapframe *tf)` 接收统一入口保存的现场。只填充该函数，沿用 A 的入口/恢复和 B 的中断处理，保留公共结构体、函数签名和分发逻辑。

## 行为

- `CAUSE_ILLEGAL_INSTRUCTION` 输出 `Illegal instruction at 0x%016lx`；`CAUSE_BREAKPOINT` 输出 `Breakpoint at 0x%016lx`，地址取更新前的 `tf->epc`。
- 对本实验 RV64IMAC 的 16/32 位触发指令，从可读内核代码地址读取首个 16 位半字；最低两位不为 `11` 时前进 2 字节，否则前进 4 字节。只更新 `epc`，不修改其他现场。
- 其他异常调用 `print_trapframe(tf)`，保持返回地址及现场不变，不承诺恢复执行。
- 范围为本实验可读的内核触发点，不扩展为用户地址安全取指、任意非法地址恢复或大于 32 位指令解码。

编码依据：[RISC-V 非特权规范：指令长度编码](https://docs.riscv.org/reference/isa/v20240411/_attachments/riscv-unprivileged.pdf)。

## 验收

真实 QEMU/GDB 覆盖 16/32 位断点和非法指令，核对原因、原始指令、epc 增量、全部整数寄存器、sret 后 PC 和 S 特权级；之后执行结束标记并验证连续三次时钟返回。未知异常通过构造现场验证兜底且不访问 epc。教师评分保持原样，最终补充正式启动和 B 回归。
