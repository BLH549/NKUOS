#include <assert.h>
#include <clock.h>
#include <console.h>
#include <defs.h>
#include <kdebug.h>
#include <memlayout.h>
#include <mmu.h>
#include <riscv.h>
#include <stdio.h>
#include <trap.h>

/* 每累计 100 次时钟中断输出一次统计。 */
#define TICK_NUM 100

/* 输出一轮时钟统计；评分构建可能在此提前结束。 */
static void print_ticks() {
    cprintf("%d ticks\n", TICK_NUM);
#ifdef DEBUG_GRADE
    cprintf("End of Test.\n");
    panic("EOT: kernel seems ok.");
#endif
}

/* 初始化 S 态统一 Trap 入口；这里使用 stvec 直接模式，并非 x86 IDT。 */
void idt_init(void) {

    extern void __alltraps(void);
    /* Set sup0 scratch register to 0, indicating to exception vector
       that we are presently executing in the kernel */
    write_csr(sscratch, 0); /* 本阶段约定内核态下 scratch 初值为零。 */
    /* Set the exception vector address */
    write_csr(stvec, &__alltraps); /* 对齐地址低两位为 0，选择直接模式。 */
}

/* trap_in_kernel - test if trap happened in kernel */
/* 根据硬件保存的 SPP 判断发生 Trap 前是否处于 S 态。 */
bool trap_in_kernel(struct trapframe *tf) {
    return (tf->status & SSTATUS_SPP) != 0;
}

/* 输出 Trap 帧及关键 CSR，便于定位未处理的 Trap。 */
void print_trapframe(struct trapframe *tf) {
    cprintf("trapframe at %p\n", tf);
    print_regs(&tf->gpr);
    cprintf("  status   0x%08x\n", tf->status);
    cprintf("  epc      0x%08x\n", tf->epc);
    cprintf("  badvaddr 0x%08x\n", tf->badvaddr);
    cprintf("  cause    0x%08x\n", tf->cause);
}

/* 按 ABI 名称输出全部已保存的通用寄存器。 */
void print_regs(struct pushregs *gpr) {
    cprintf("  zero     0x%08x\n", gpr->zero);
    cprintf("  ra       0x%08x\n", gpr->ra);
    cprintf("  sp       0x%08x\n", gpr->sp);
    cprintf("  gp       0x%08x\n", gpr->gp);
    cprintf("  tp       0x%08x\n", gpr->tp);
    cprintf("  t0       0x%08x\n", gpr->t0);
    cprintf("  t1       0x%08x\n", gpr->t1);
    cprintf("  t2       0x%08x\n", gpr->t2);
    cprintf("  s0       0x%08x\n", gpr->s0);
    cprintf("  s1       0x%08x\n", gpr->s1);
    cprintf("  a0       0x%08x\n", gpr->a0);
    cprintf("  a1       0x%08x\n", gpr->a1);
    cprintf("  a2       0x%08x\n", gpr->a2);
    cprintf("  a3       0x%08x\n", gpr->a3);
    cprintf("  a4       0x%08x\n", gpr->a4);
    cprintf("  a5       0x%08x\n", gpr->a5);
    cprintf("  a6       0x%08x\n", gpr->a6);
    cprintf("  a7       0x%08x\n", gpr->a7);
    cprintf("  s2       0x%08x\n", gpr->s2);
    cprintf("  s3       0x%08x\n", gpr->s3);
    cprintf("  s4       0x%08x\n", gpr->s4);
    cprintf("  s5       0x%08x\n", gpr->s5);
    cprintf("  s6       0x%08x\n", gpr->s6);
    cprintf("  s7       0x%08x\n", gpr->s7);
    cprintf("  s8       0x%08x\n", gpr->s8);
    cprintf("  s9       0x%08x\n", gpr->s9);
    cprintf("  s10      0x%08x\n", gpr->s10);
    cprintf("  s11      0x%08x\n", gpr->s11);
    cprintf("  t3       0x%08x\n", gpr->t3);
    cprintf("  t4       0x%08x\n", gpr->t4);
    cprintf("  t5       0x%08x\n", gpr->t5);
    cprintf("  t6       0x%08x\n", gpr->t6);
}

//lab3 function2 (B: 2413575; C: 2411238)
#include <sbi.h>

/* 输出 10 轮统计后请求固件关机。 */
#define SHUTDOWN_AFTER 10

/* 按 scause 的原因码分发异步中断；高位中断标志先被清除。 */
void interrupt_handler(struct trapframe *tf) {
    //lab3 interrupt_handler 2413575
    uintptr_t cause = tf->cause & ~((uintptr_t)1 << (sizeof(uintptr_t) * 8 - 1));
    /* 各非时钟分支仅打印类型，并不确认或清除设备中断。 */
    switch (cause) {
    case IRQ_U_SOFT:
        cprintf("User software interrupt\n");
        break;
    case IRQ_S_SOFT:
        cprintf("Supervisor software interrupt\n");
        break;
    case IRQ_H_SOFT:
        cprintf("Hypervisor software interrupt\n");
        break;
    case IRQ_M_SOFT:
        cprintf("Machine software interrupt\n");
        break;
    case IRQ_U_TIMER:
        cprintf("User timer interrupt\n");
        break;
    case IRQ_S_TIMER:
        clock_set_next_event(); /* SBI 定时事件是一次性的，须预约下一次。 */
        ++ticks; /* 每收到一次 S 态时钟中断计数一次。 */
        if (ticks % TICK_NUM == 0) { /* 达到一个统计周期。 */
            print_ticks();
            if (ticks / TICK_NUM >= SHUTDOWN_AFTER) { /* 达到要求的输出次数。 */
                sbi_shutdown(); /* 通过 OpenSBI 请求关机。 */
            }
        }
        break;
    case IRQ_H_TIMER:
        cprintf("Hypervisor timer interrupt\n");
        break;
    case IRQ_M_TIMER:
        cprintf("Machine timer interrupt\n");
        break;
    case IRQ_U_EXT:
        cprintf("User external interrupt\n");
        break;
    case IRQ_S_EXT:
        cprintf("Supervisor external interrupt\n");
        break;
    case IRQ_H_EXT:
        cprintf("Hypervisor external interrupt\n");
        break;
    case IRQ_M_EXT:
        cprintf("Machine external interrupt\n");
        break;
    case IRQ_COP:
        cprintf("Coprocessor interrupt\n");
        break;
    case IRQ_HOST:
        cprintf("Host interrupt\n");
        break;
    default:
        cprintf("Unknown interrupt: cause %lu\n", (unsigned long)cause);
        print_trapframe(tf);
        break;
    }
}

/* 处理同步异常；其他异常只打印帧，并未提供通用恢复策略。 */
void exception_handler(struct trapframe *tf) {
    //lab3 exception_handler 2411238
    switch (tf->cause) {
    case CAUSE_ILLEGAL_INSTRUCTION:
    case CAUSE_BREAKPOINT: {
        if (tf->cause == CAUSE_ILLEGAL_INSTRUCTION) {
            cprintf("Illegal instruction at 0x%016lx\n", (unsigned long)tf->epc);
        } else {
            cprintf("Breakpoint at 0x%016lx\n", (unsigned long)tf->epc);
        }
        /* The RV64IMAC lab triggers contain 16- or 32-bit instructions.
         * Read only the first halfword, even at a two-byte-aligned EPC. */
        /* 低两位非 11 表示 16 位压缩指令；11 在本实验中按 32 位处理。 */
        unsigned short insn = *(const unsigned short *)tf->epc;
        /* 修改帧中的 sepc，防止 sret 后再次执行同一异常指令。 */
        tf->epc += (insn & 3) == 3 ? 4 : 2;
        break;
    }
    default:
        print_trapframe(tf);
        break;
    }
}



/* scause 最高位为 1 表示中断；否则是同步异常。 */
static inline void trap_dispatch(struct trapframe *tf) {
    if ((intptr_t)tf->cause < 0) {
        // interrupts
        interrupt_handler(tf);
    } else {
        // exceptions
        exception_handler(tf);
    }
}

/* 汇编入口传入当前帧；分发完成后由汇编恢复现场并执行 sret。 */
void trap(struct trapframe *tf) {
    // dispatch based on what type of trap occurred
    trap_dispatch(tf);
}
