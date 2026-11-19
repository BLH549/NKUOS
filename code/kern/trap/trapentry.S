#include <riscv.h>

    .macro SAVE_ALL
    .align 2

    csrw sscratch, sp

    addi sp, sp, -36 * REGBYTES
    # save gprs
    STORE  x1,1*REGBYTES(sp)
    STORE  x3,3*REGBYTES(sp)
    STORE  x4,4*REGBYTES(sp)
    STORE  x5,5*REGBYTES(sp)
    STORE  x6,6*REGBYTES(sp)
    STORE  x7,7*REGBYTES(sp)
    STORE  x8,8*REGBYTES(sp)
    STORE  x9,9*REGBYTES(sp)
    STORE  x10,10*REGBYTES(sp)
    STORE  x11,11*REGBYTES(sp)
    STORE  x12,12*REGBYTES(sp)
    STORE  x13,13*REGBYTES(sp)
    STORE  x14,14*REGBYTES(sp)
    STORE  x15,15*REGBYTES(sp)
    STORE  x16,16*REGBYTES(sp)
    STORE  x17,17*REGBYTES(sp)
    STORE  x18,18*REGBYTES(sp)
    STORE  x19,19*REGBYTES(sp)
    STORE  x20,20*REGBYTES(sp)
    STORE  x21,21*REGBYTES(sp)
    STORE  x22,22*REGBYTES(sp)
    STORE  x23,23*REGBYTES(sp)
    STORE  x24,24*REGBYTES(sp)
    STORE  x25,25*REGBYTES(sp)
    STORE  x26,26*REGBYTES(sp)
    STORE  x27,27*REGBYTES(sp)
    STORE  x28,28*REGBYTES(sp)
    STORE  x29,29*REGBYTES(sp)
    STORE  x30,30*REGBYTES(sp)
    STORE  x31,31*REGBYTES(sp)

    # get sr, epc, badvaddr, cause
    csrr s0, sscratch
    csrr s1, sstatus
    csrr s2, sepc
    csrr s3, 0x143
    csrr s4, scause

    STORE s0, 2*REGBYTES(sp)
    STORE s1, 32*REGBYTES(sp)
    STORE s2, 33*REGBYTES(sp)
    STORE s3, 34*REGBYTES(sp)
    STORE s4, 35*REGBYTES(sp)
    .endm

    .macro RESTORE_ALL
 # 定义与保存布局对应的现场恢复宏。

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


    .endm

    .globl __alltraps
    .align 2
__alltraps:
    SAVE_ALL

    # lab3: migrated entry call; keep Lab4 forkrets below.
    mv a0, sp
    jal trap
    # sp should be the same as before "jal trap"

    .globl __trapret
__trapret:
    RESTORE_ALL
    # go back from supervisor call
    sret
 
    .globl forkrets
forkrets:
    # set stack to this new process's trapframe
    move sp, a0
    j __trapret
