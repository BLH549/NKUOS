/*
 * 内核高层输入输出：将格式化层与字符设备接口连接起来。
 * 启动输出链：cprintf → vcprintf → vprintfmt → cputch → cons_putc。
 */
/* 逐行：引入控制台字符接口声明，把高层格式化与底层 SBI 封装连接起来。 */
#include <console.h>
/* 逐行：引入 uint64_t、size_t、NULL 等内核基础类型和宏。 */
#include <defs.h>
/* 逐行：引入本项目的 stdio 接口声明；头文件由 Makefile 的 -Ilibs 找到，不是宿主 glibc。 */
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
/* 逐行：定义仅在本文件可见的输出回调；c 是字符，cnt 指向调用者的计数器。 */
static void cputch(int c, int *cnt) {
    /* 逐行：把一个字符交给控制台层；随后经 sbi_console_putchar 调用固件。 */
    cons_putc(c);
    /* 逐行：先解引用 cnt 再递增实际计数值，不是让指针移动；一次回调计一个字符。 */
    (*cnt)++;
/* 逐行：结束当前函数、分支或循环的作用域；右花括号本身不执行输出或修改数据。 */
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
/* 逐行：接收格式串及已建立的参数列表，提供面向控制台的格式化输出。 */
int vcprintf(const char *fmt, va_list ap) {
    /* 逐行：创建并清零本次调用的字符计数器，后续传地址给输出回调累计字符数。 */
    int cnt = 0;
    /* 逐行：把字符回调、计数器地址、格式串和参数交给解析器；保留模板现有回调转换写法。 */
    vprintfmt((void *)cputch, &cnt, fmt, ap);
    /* 逐行：返回本次累计字符数；cputs 中包含追加换行，格式化接口中不包含 NUL。 */
    return cnt;
/* 逐行：结束当前函数、分支或循环的作用域；右花括号本身不执行输出或修改数据。 */
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
/* 逐行：定义可变参数输出接口；fmt 是最后一个固定参数，省略号表示后续值按格式解释。 */
int cprintf(const char *fmt, ...) {
    /* 逐行：声明由编译器维护的参数遍历状态，必须通过 va_start 初始化后才能取参。 */
    va_list ap;
    /* 逐行：声明结果计数变量；后续调用先赋值，再由 return 返回，当前不读取未初始化值。 */
    int cnt;
    /* 逐行：以最后一个固定参数 fmt 初始化 ap，使后续格式化代码可以读取省略号参数。 */
    va_start(ap, fmt);
    /* 逐行：将参数列表交给 vcprintf 执行字符输出，并保存其返回的总字符数。 */
    cnt = vcprintf(fmt, ap);
    /* 逐行：结束本层可变参数遍历生命周期；本项目 stdarg.h 将该宏定义为空操作。 */
    va_end(ap);
    /* 逐行：返回本次累计字符数；cputs 中包含追加换行，格式化接口中不包含 NUL。 */
    return cnt;
/* 逐行：结束当前函数、分支或循环的作用域；右花括号本身不执行输出或修改数据。 */
}

/* cputchar - writes a single character to stdout */
/*
 * cputchar：直接输出单个字符的便捷接口。
 * 参数 c：字符值；返回值：无。直接调用 cons_putc，不解析格式、不增加换行。
 */
/* 逐行：提供单字符输出快捷接口，直接调用 cons_putc，不解析格式，也不额外输出换行。 */
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
/* 逐行：进入字符串输出函数；str 必须指向有效的 NUL 结尾文本，返回值为输出字符数。 */
int cputs(const char *str) {
    /* 逐行：创建并清零本次调用的字符计数器，后续传地址给输出回调累计字符数。 */
    int cnt = 0;
    /* 逐行：声明当前字符变量；循环条件在每次使用前读取并赋值。 */
    char c;
    /* 逐行：先读取 str 指向的字符到 c，再推进 str；遇到 NUL 停止，结束符本身不输出。 */
    while ((c = *str++) != '\0') {
        /* 逐行：把当前字符和计数器地址交给输出回调，同时完成字符输出与计数增加。 */
        cputch(c, &cnt);
    /* 逐行：结束当前函数、分支或循环的作用域；右花括号本身不执行输出或修改数据。 */
    }
    /* 逐行：字符串输出完后额外发送一个换行，回调同样把它计入返回字符数。 */
    cputch('\n', &cnt);
    /* 逐行：返回本次累计字符数；cputs 中包含追加换行，格式化接口中不包含 NUL。 */
    return cnt;
/* 逐行：结束当前函数、分支或循环的作用域；右花括号本身不执行输出或修改数据。 */
}

/* getchar - reads a single non-zero character from stdin */
/*
 * getchar：反复轮询控制台输入，直到取得非零结果。
 * 参数：无；返回值：cons_getc 返回的第一个非零整数。
 * 零值会使函数继续忙等；负值也会直接返回，交由 readline 判断。
 * 本函数没有调度、休眠或超时机制；当前启动流程不使用它。
 */
/* 逐行：进入无参数输入函数；它通过轮询等待第一个非零控制台结果。 */
int getchar(void) {
    /* 逐行：声明保存读取结果的变量；while 条件会在判断之前赋值。 */
    int c;
    /* 逐行：反复调用 cons_getc；结果为零时执行空循环体继续忙等，非零时退出且保留结果。 */
    while ((c = cons_getc()) == 0) /* do nothing */;
    /* 逐行：将最后获得的字符值或底层状态原样返回给上层输入函数。 */
    return c;
/* 逐行：结束当前函数、分支或循环的作用域；右花括号本身不执行输出或修改数据。 */
}
