#include <clock.h>
#include <defs.h>
#include <sbi.h>
#include <stdio.h>
#include <riscv.h>

/* 在中断处理程序中更新的时钟计数；volatile 本身不提供原子性。 */
volatile size_t ticks;

/* 读取 time CSR（实际是平台时间计数，不是 CPU cycle）。 */
static inline uint64_t get_cycles(void) {
#if __riscv_xlen == 64
    uint64_t n;
    /* RV64 可以一次读取完整的 64 位时间值。 */
    __asm__ __volatile__("rdtime %0" : "=r"(n));
    return n;
#else
    /* RV32 使用高-低-高读取；若高位变化则重试，避免低位溢出撕裂。 */
    uint32_t lo, hi, tmp;
    __asm__ __volatile__(
        "1:\n"
        "rdtimeh %0\n"
        "rdtime %1\n"
        "rdtimeh %2\n"
        "bne %0, %2, 1b"
        : "=&r"(hi), "=&r"(lo), "=&r"(tmp));
    return ((uint64_t)hi << 32) | lo;
#endif
}

/* 固定时间计数间隔；QEMU 常见 10 MHz timebase 下约为 10 ms。 */
static uint64_t timebase = 100000;

/* 允许 S 态时钟中断并安排首次事件；全局 SIE 由调用者开启。 */
void clock_init(void) {
    // enable timer interrupt in sie
    set_csr(sie, MIP_STIP); /* sie.STIE 与 MIP_STIP 使用相同的位掩码。 */
    // divided by 500 when using Spike(2MHz)
    // divided by 100 when using QEMU(10MHz)
    // timebase = sbi_timebase() / 500;
    clock_set_next_event();

    // initialize time counter 'ticks' to zero
    ticks = 0;

    cprintf("++ setup timer interrupts\n");
}

/* SBI 定时器接受绝对时间，因此用当前 time 加固定间隔。 */
void clock_set_next_event(void) { sbi_set_timer(get_cycles() + timebase); }
