/*
 * 控制台字符接口层：把字符输入输出转交给 SBI；中断和初始化接口仍为占位。
 */
/* 逐行：引入 SBI 包装接口声明，使 C 代码可以通过函数调用发起固件服务请求。 */
#include <sbi.h>
/* 逐行：引入控制台字符接口声明，把高层格式化与底层 SBI 封装连接起来。 */
#include <console.h>

/* kbd_intr - try to feed input characters from keyboard */
/*
 * kbd_intr：键盘中断处理的预留入口。
 * 参数：无；返回值：无。当前函数体为空，不读取键盘、不处理或确认中断。
 * Lab1 尚未建立内核自己的键盘中断处理流程。
 */
/* 逐行：定义无参数、无返回值的键盘中断占位函数；空函数体表示当前没有处理动作。 */
void kbd_intr(void) {}

/* serial_intr - try to feed input characters from serial port */
/*
 * serial_intr：串口中断处理的预留入口。
 * 参数：无；返回值：无。当前为空实现，不读取串口或清除中断状态。
 * 本实验的打印依赖 SBI 服务，不依赖此函数。
 */
/* 逐行：定义串口中断占位函数；Lab1 的输出通过 SBI 完成，不依赖此空实现。 */
void serial_intr(void) {}

/* cons_init - initializes the console devices */
/*
 * cons_init：控制台初始化的预留接口。
 * 参数：无；返回值：无。Lab1 使用固件提供的控制台服务，当前函数不做初始化。
 */
/* 逐行：定义控制台初始化占位函数；当前不配置硬件，使用已启动的固件服务。 */
void cons_init(void) {}

/* cons_putc - print a single character @c to console devices */
/*
 * cons_putc：向固件控制台输出一个字符。
 * 参数 c：字符值，转换为 unsigned char 后交给 sbi_console_putchar。
 * 返回值：无。这里只负责字符传递，格式化由上层完成，不直接访问设备寄存器。
 */
/* 逐行：接收一个 int 字符，将它转换为无符号字节，再请求 SBI 字符输出；没有返回值。 */
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
/* 逐行：进入控制台输入封装；此路径引用的 sbi_console_getchar 在当前仓库没有定义。 */
int cons_getc(void) {
    /* 逐行：声明输入结果变量并先置零，下一条语句会用固件输入结果覆盖它。 */
    int c = 0;
    /* 逐行：调用已声明的 SBI 输入包装函数，接收原始结果；当前启动不会执行此路径。 */
    c = sbi_console_getchar();
    /* 逐行：将最后获得的字符值或底层状态原样返回给上层输入函数。 */
    return c;
/* 逐行：结束当前函数、分支或循环的作用域；右花括号本身不执行输出或修改数据。 */
}
