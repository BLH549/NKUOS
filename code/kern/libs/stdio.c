/*
 * 内核高层输入输出：将格式化层与字符设备接口连接起来。
 * 启动输出链：cprintf → vcprintf → vprintfmt → cputch → cons_putc。
 */
#include <console.h>
#include <defs.h>
#include <stdio.h>

/* HIGH level console I/O */

/* *
 * cputch - writes a single character @c to stdout, and it will
 * increace the value of counter pointed by @cnt.
 * */
/*
 * cputch：格式化器使用的单字符输出回调。
 * 参数 c：要输出的字符；cnt：有效的计数器地址，调用前由 vcprintf 清零。
 * 先调用 cons_putc，再增加计数；返回值：无。
 * 计数表示向控制台接口提交的字符数，底层输出失败不会在这里单独报告。
 */
static void cputch(int c, int *cnt) {
    cons_putc(c);
    (*cnt)++;
}

/* *
 * vcprintf - format a string and writes it to stdout
 *
 * The return value is the number of characters which would be
 * written to stdout.
 *
 * Call this function if you are already dealing with a va_list.
 * Or you probably want cprintf() instead.
 * */
/*
 * vcprintf：用已有的可变参数列表向控制台打印格式化文本。
 * 参数 fmt：格式字符串；ap：与格式匹配的参数列表。
 * 以局部 cnt 作为回调上下文，调用 vprintfmt 完成解析和字符输出。
 * 返回提交输出的字符数；本函数不负责为调用者初始化或结束 ap。
 */
int vcprintf(const char *fmt, va_list ap) {
    int cnt = 0;
    vprintfmt((void *)cputch, &cnt, fmt, ap);
    return cnt;
}

/* *
 * cprintf - formats a string and writes it to stdout
 *
 * The return value is the number of characters which would be
 * written to stdout.
 * */
/*
 * cprintf：内核的 printf 风格控制台输出入口。
 * 参数 fmt：格式字符串；其余参数：按格式说明提供的值，类型必须匹配。
 * 通过 va_start 取得参数列表，交给 vcprintf，再调用 va_end。
 * 返回提交输出的字符数。Lab1 的启动消息由 kern_init 调用本函数打印。
 */
int cprintf(const char *fmt, ...) {
    va_list ap;
    int cnt;
    va_start(ap, fmt);
    cnt = vcprintf(fmt, ap);
    va_end(ap);
    return cnt;
}

/* cputchar - writes a single character to stdout */
/*
 * cputchar：直接输出单个字符的便捷接口。
 * 参数 c：字符值；返回值：无。直接调用 cons_putc，不解析格式、不增加换行。
 */
void cputchar(int c) { cons_putc(c); }

/* *
 * cputs- writes the string pointed by @str to stdout and
 * appends a newline character.
 * */
/*
 * cputs：输出以 NUL 结尾的字符串，并额外输出一个换行。
 * 参数 str：有效的字符串地址，不可为 NULL。
 * 逐字符调用 cputch，遇到字符串结束符后输出换行。
 * 返回字符数，包含追加的换行，不包含 NUL 结束符。
 */
int cputs(const char *str) {
    int cnt = 0;
    char c;
    while ((c = *str++) != '\0') {
        cputch(c, &cnt);
    }
    cputch('\n', &cnt);
    return cnt;
}

/* getchar - reads a single non-zero character from stdin */
/*
 * getchar：反复轮询控制台输入，直到取得非零结果。
 * 参数：无；返回值：cons_getc 返回的第一个非零整数。
 * 零值会使函数继续忙等；负值也会直接返回，交由 readline 判断。
 * 本函数没有调度、休眠或超时机制；当前启动流程不使用它。
 */
int getchar(void) {
    int c;
    while ((c = cons_getc()) == 0) /* do nothing */;
    return c;
}
