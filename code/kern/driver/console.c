/*
 * 控制台字符接口层：把字符输入输出转交给 SBI；中断和初始化接口仍为占位。
 */
#include <sbi.h>
#include <console.h>

/* kbd_intr - try to feed input characters from keyboard */
/*
 * kbd_intr：键盘中断处理的预留入口。
 * 参数：无；返回值：无。当前函数体为空，不读取键盘、不处理或确认中断。
 * Lab1 尚未建立内核自己的键盘中断处理流程。
 */
void kbd_intr(void) {}

/* serial_intr - try to feed input characters from serial port */
/*
 * serial_intr：串口中断处理的预留入口。
 * 参数：无；返回值：无。当前为空实现，不读取串口或清除中断状态。
 * 本实验的打印依赖 SBI 服务，不依赖此函数。
 */
void serial_intr(void) {}

/* cons_init - initializes the console devices */
/*
 * cons_init：控制台初始化的预留接口。
 * 参数：无；返回值：无。Lab1 使用固件提供的控制台服务，当前函数不做初始化。
 */
void cons_init(void) {}

/* cons_putc - print a single character @c to console devices */
/*
 * cons_putc：向固件控制台输出一个字符。
 * 参数 c：字符值，转换为 unsigned char 后交给 sbi_console_putchar。
 * 返回值：无。这里只负责字符传递，格式化由上层完成，不直接访问设备寄存器。
 */
void cons_putc(int c) { sbi_console_putchar((unsigned char)c); }

/* *
 * cons_getc - return the next input character from console,
 * or 0 if none waiting.
 * */
/*
 * cons_getc：调用 sbi_console_getchar 获取输入结果并原样返回。
 * 参数：无；返回值：底层 SBI 输入接口的整数结果。
 * 本仓库仅声明 sbi_console_getchar，没有提供其实现；当前启动不调用输入路径。
 * 不要据此认为本实验已实现完整的键盘驱动或输入缓冲。
 */
int cons_getc(void) {
    int c = 0;
    c = sbi_console_getchar();
    return c;
}
