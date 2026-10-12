/*
 * Lab1 的旧式 SBI 服务封装。编号 0～8 对应模板保留的固件服务。
 * 仅实现通用调用、字符输出与定时器设置；编号存在不代表对应包装函数已实现。
 */
// libs/sbi.c
#include <sbi.h>
#include <defs.h>


uint64_t SBI_SET_TIMER = 0;
uint64_t SBI_CONSOLE_PUTCHAR = 1;
uint64_t SBI_CONSOLE_GETCHAR = 2;
uint64_t SBI_CLEAR_IPI = 3;
uint64_t SBI_SEND_IPI = 4;
uint64_t SBI_REMOTE_FENCE_I = 5;
uint64_t SBI_REMOTE_SFENCE_VMA = 6;
uint64_t SBI_REMOTE_SFENCE_VMA_ASID = 7;
uint64_t SBI_SHUTDOWN = 8;

/*
 * sbi_call：按本代码采用的旧式 SBI 约定发起固件服务请求。
 * 参数 sbi_type：服务编号；arg0、arg1、arg2：前三个服务参数。
 * 汇编依次把编号放入 x17/a7，把参数放入 x10/a0、x11/a1、x12/a2，
 * 通过 ecall 请求 M 模式的 OpenSBI，再读取 a0 作为 uint64_t 返回值。
 * volatile 保留汇编操作，memory 声明阻止编译器把内存访问随意跨越此块。
 * 这是现有内联汇编的约定说明，memory 不等同于硬件内存屏障。
 */
uint64_t sbi_call(uint64_t sbi_type, uint64_t arg0, uint64_t arg1, uint64_t arg2) {
    uint64_t ret_val;
    __asm__ volatile (
        "mv x17, %[sbi_type]\n"
        "mv x10, %[arg0]\n"
        "mv x11, %[arg1]\n"
        "mv x12, %[arg2]\n"
        "ecall\n"
        "mv %[ret_val], x10"
        : [ret_val] "=r" (ret_val)
        : [sbi_type] "r" (sbi_type), [arg0] "r" (arg0), [arg1] "r" (arg1), [arg2] "r" (arg2)
        : "memory"
    );
    return ret_val;
}

/*
 * sbi_console_putchar：调用编号为 1 的旧式 SBI 控制台输出服务。
 * 参数 ch：要输出的字节；返回值：无，忽略 sbi_call 的返回值。
 * 字符放在第一个参数中，另外两个参数置零。真正的设备输出由固件完成。
 */
void sbi_console_putchar(unsigned char ch) {
    sbi_call(SBI_CONSOLE_PUTCHAR, ch, 0, 0);
}

/*
 * sbi_set_timer：调用编号为 0 的旧式 SBI 定时器服务。
 * 参数 stime_value：下一次定时器事件的绝对计时值，使用平台计时单位。
 * RV64 下通过 a0 传递 64 位值；其余参数为零。返回值：无。
 * 本函数只请求设置定时器，Lab1 没有调用它，也没有建立定时中断处理流程。
 */
void sbi_set_timer(unsigned long long stime_value) {
    sbi_call(SBI_SET_TIMER, stime_value, 0, 0);
}
