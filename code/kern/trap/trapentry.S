/* 引入 REGBYTES、STORE 和 LOAD 等架构相关定义。 */
#include <riscv.h>

    .macro SAVE_ALL # 定义保存现场的汇编宏；展开后在当前栈构造 trapframe。

    csrw sscratch, sp # 暂存进入 Trap 时的原始 sp；本实现直接使用当前栈，并未切换用户栈。

    addi sp, sp, -36 * REGBYTES # 向低地址分配 36 个寄存器槽位（RV64 下 288 字节）。
    # save x registers
    STORE x0, 0*REGBYTES(sp) # 保存恒为零的 x0，对应 trapframe 的第 0 个槽位。
    STORE x1, 1*REGBYTES(sp) # 保存返回地址 ra，对应 trapframe 的第 1 个槽位。
    STORE x3, 3*REGBYTES(sp) # 保存全局指针 gp，对应 trapframe 的第 3 个槽位。
    STORE x4, 4*REGBYTES(sp) # 保存线程指针 tp，对应 trapframe 的第 4 个槽位。
    STORE x5, 5*REGBYTES(sp) # 保存临时寄存器 t0，对应 trapframe 的第 5 个槽位。
    STORE x6, 6*REGBYTES(sp) # 保存临时寄存器 t1，对应 trapframe 的第 6 个槽位。
    STORE x7, 7*REGBYTES(sp) # 保存临时寄存器 t2，对应 trapframe 的第 7 个槽位。
    STORE x8, 8*REGBYTES(sp) # 保存保存寄存器 s0/fp，对应 trapframe 的第 8 个槽位。
    STORE x9, 9*REGBYTES(sp) # 保存保存寄存器 s1，对应 trapframe 的第 9 个槽位。
    STORE x10, 10*REGBYTES(sp) # 保存参数及返回值 a0，对应 trapframe 的第 10 个槽位。
    STORE x11, 11*REGBYTES(sp) # 保存参数及返回值 a1，对应 trapframe 的第 11 个槽位。
    STORE x12, 12*REGBYTES(sp) # 保存参数 a2，对应 trapframe 的第 12 个槽位。
    STORE x13, 13*REGBYTES(sp) # 保存参数 a3，对应 trapframe 的第 13 个槽位。
    STORE x14, 14*REGBYTES(sp) # 保存参数 a4，对应 trapframe 的第 14 个槽位。
    STORE x15, 15*REGBYTES(sp) # 保存参数 a5，对应 trapframe 的第 15 个槽位。
    STORE x16, 16*REGBYTES(sp) # 保存参数 a6，对应 trapframe 的第 16 个槽位。
    STORE x17, 17*REGBYTES(sp) # 保存参数 a7，对应 trapframe 的第 17 个槽位。
    STORE x18, 18*REGBYTES(sp) # 保存保存寄存器 s2，对应 trapframe 的第 18 个槽位。
    STORE x19, 19*REGBYTES(sp) # 保存保存寄存器 s3，对应 trapframe 的第 19 个槽位。
    STORE x20, 20*REGBYTES(sp) # 保存保存寄存器 s4，对应 trapframe 的第 20 个槽位。
    STORE x21, 21*REGBYTES(sp) # 保存保存寄存器 s5，对应 trapframe 的第 21 个槽位。
    STORE x22, 22*REGBYTES(sp) # 保存保存寄存器 s6，对应 trapframe 的第 22 个槽位。
    STORE x23, 23*REGBYTES(sp) # 保存保存寄存器 s7，对应 trapframe 的第 23 个槽位。
    STORE x24, 24*REGBYTES(sp) # 保存保存寄存器 s8，对应 trapframe 的第 24 个槽位。
    STORE x25, 25*REGBYTES(sp) # 保存保存寄存器 s9，对应 trapframe 的第 25 个槽位。
    STORE x26, 26*REGBYTES(sp) # 保存保存寄存器 s10，对应 trapframe 的第 26 个槽位。
    STORE x27, 27*REGBYTES(sp) # 保存保存寄存器 s11，对应 trapframe 的第 27 个槽位。
    STORE x28, 28*REGBYTES(sp) # 保存临时寄存器 t3，对应 trapframe 的第 28 个槽位。
    STORE x29, 29*REGBYTES(sp) # 保存临时寄存器 t4，对应 trapframe 的第 29 个槽位。
    STORE x30, 30*REGBYTES(sp) # 保存临时寄存器 t5，对应 trapframe 的第 30 个槽位。
    STORE x31, 31*REGBYTES(sp) # 保存临时寄存器 t6，对应 trapframe 的第 31 个槽位。

    # get sr, epc, badvaddr, cause
    # Set sscratch register to 0, so that if a recursive exception
    # occurs, the exception vector knows it came from the kernel
    csrrw s0, sscratch, x0 # 取回原始 sp 到 s0，同时将 sscratch 清零；原 s0 已保存。
    csrr s1, sstatus # 读取 Trap 进入后的 sstatus，供返回时恢复。
    csrr s2, sepc # 读取硬件保存的异常或中断返回地址 sepc。
    csrr s3, sbadaddr # 读取异常相关地址 stval（sbadaddr 是旧别名）。
    csrr s4, scause # 读取 Trap 原因及中断标志 scause。

    STORE s0, 2*REGBYTES(sp) # 将原始 sp 放入 x2 对应槽位。
    STORE s1, 32*REGBYTES(sp) # 保存 sstatus，供 sret 前恢复。
    STORE s2, 33*REGBYTES(sp) # 保存 sepc；C 处理函数可以修改此返回地址。
    STORE s3, 34*REGBYTES(sp) # 保存 stval，供异常诊断。
    STORE s4, 35*REGBYTES(sp) # 保存 scause，供 C 代码分发。
    .endm # 结束当前汇编宏定义。

    .macro RESTORE_ALL # 定义与保存布局对应的现场恢复宏。

    #lab3 function1 2412235
    # Restore CSR values before reloading the saved temporaries.
    LOAD t0, 32*REGBYTES(sp) # 先借用 t0 取出保存的 sstatus；t0 原值稍后恢复。
    LOAD t1, 33*REGBYTES(sp) # 借用 t1 取出可能已由异常处理修改的 sepc。
    csrw sstatus, t0 # 恢复返回所需的状态位，包括 SPP 和 SPIE。
    csrw sepc, t1 # 恢复返回地址；sret 将从该地址继续执行。

    LOAD x1, 1*REGBYTES(sp) # 恢复返回地址 ra，对应 trapframe 的第 1 个槽位。
    LOAD x3, 3*REGBYTES(sp) # 恢复全局指针 gp，对应 trapframe 的第 3 个槽位。
    LOAD x4, 4*REGBYTES(sp) # 恢复线程指针 tp，对应 trapframe 的第 4 个槽位。
    LOAD x5, 5*REGBYTES(sp) # 恢复临时寄存器 t0，对应 trapframe 的第 5 个槽位。
    LOAD x6, 6*REGBYTES(sp) # 恢复临时寄存器 t1，对应 trapframe 的第 6 个槽位。
    LOAD x7, 7*REGBYTES(sp) # 恢复临时寄存器 t2，对应 trapframe 的第 7 个槽位。
    LOAD x8, 8*REGBYTES(sp) # 恢复保存寄存器 s0/fp，对应 trapframe 的第 8 个槽位。
    LOAD x9, 9*REGBYTES(sp) # 恢复保存寄存器 s1，对应 trapframe 的第 9 个槽位。
    LOAD x10, 10*REGBYTES(sp) # 恢复参数及返回值 a0，对应 trapframe 的第 10 个槽位。
    LOAD x11, 11*REGBYTES(sp) # 恢复参数及返回值 a1，对应 trapframe 的第 11 个槽位。
    LOAD x12, 12*REGBYTES(sp) # 恢复参数 a2，对应 trapframe 的第 12 个槽位。
    LOAD x13, 13*REGBYTES(sp) # 恢复参数 a3，对应 trapframe 的第 13 个槽位。
    LOAD x14, 14*REGBYTES(sp) # 恢复参数 a4，对应 trapframe 的第 14 个槽位。
    LOAD x15, 15*REGBYTES(sp) # 恢复参数 a5，对应 trapframe 的第 15 个槽位。
    LOAD x16, 16*REGBYTES(sp) # 恢复参数 a6，对应 trapframe 的第 16 个槽位。
    LOAD x17, 17*REGBYTES(sp) # 恢复参数 a7，对应 trapframe 的第 17 个槽位。
    LOAD x18, 18*REGBYTES(sp) # 恢复保存寄存器 s2，对应 trapframe 的第 18 个槽位。
    LOAD x19, 19*REGBYTES(sp) # 恢复保存寄存器 s3，对应 trapframe 的第 19 个槽位。
    LOAD x20, 20*REGBYTES(sp) # 恢复保存寄存器 s4，对应 trapframe 的第 20 个槽位。
    LOAD x21, 21*REGBYTES(sp) # 恢复保存寄存器 s5，对应 trapframe 的第 21 个槽位。
    LOAD x22, 22*REGBYTES(sp) # 恢复保存寄存器 s6，对应 trapframe 的第 22 个槽位。
    LOAD x23, 23*REGBYTES(sp) # 恢复保存寄存器 s7，对应 trapframe 的第 23 个槽位。
    LOAD x24, 24*REGBYTES(sp) # 恢复保存寄存器 s8，对应 trapframe 的第 24 个槽位。
    LOAD x25, 25*REGBYTES(sp) # 恢复保存寄存器 s9，对应 trapframe 的第 25 个槽位。
    LOAD x26, 26*REGBYTES(sp) # 恢复保存寄存器 s10，对应 trapframe 的第 26 个槽位。
    LOAD x27, 27*REGBYTES(sp) # 恢复保存寄存器 s11，对应 trapframe 的第 27 个槽位。
    LOAD x28, 28*REGBYTES(sp) # 恢复临时寄存器 t3，对应 trapframe 的第 28 个槽位。
    LOAD x29, 29*REGBYTES(sp) # 恢复临时寄存器 t4，对应 trapframe 的第 29 个槽位。
    LOAD x30, 30*REGBYTES(sp) # 恢复临时寄存器 t5，对应 trapframe 的第 30 个槽位。
    LOAD x31, 31*REGBYTES(sp) # 恢复临时寄存器 t6，对应 trapframe 的第 31 个槽位。
    # The frame base stays in sp until every other load is complete.
    LOAD x2, 2*REGBYTES(sp) # 恢复栈指针 sp，对应 trapframe 的第 2 个槽位。 最后恢复 sp，因为此前所有加载都依赖帧基址。

    .endm # 结束当前汇编宏定义。

    .globl __alltraps # 导出统一 Trap 入口，供 C 初始化 stvec 引用。
    .align(2) # 按 2 的 2 次方字节对齐，满足 stvec 直接模式要求。
__alltraps: # 硬件发生委托给 S 态的 Trap 后从此处进入。
    SAVE_ALL # 展开宏，在当前栈保存完整的 Trap 帧。

    #lab3 function1 2412235
    mv a0, sp # 按 RISC-V 调用约定，将 trapframe 地址作为第一个参数。
    jal trap # 调用 C 层 trap(tf)；返回后继续执行下面的恢复路径。
    # sp should be the same as before "jal trap"

    .globl __trapret # 导出 Trap 返回入口，供其他代码复用。
__trapret: # C 处理完成后顺序落入该标签。
    RESTORE_ALL # 按帧中保存的值恢复状态和通用寄存器。
    # return from supervisor call
    sret # 依据 sepc 和 sstatus 返回原特权级及执行位置；不一定返回 U 态。
