/*
 * 内核自己的输入输出与格式化接口，不是宿主系统的标准 stdio.h。
 * 控制台输出见 kern/libs/stdio.c，格式化见 printfmt.c，行输入见 readline.c。
 */
#ifndef __LIBS_STDIO_H__
#define __LIBS_STDIO_H__

#include <defs.h>
#include <stdarg.h>

/* kern/libs/stdio.c */
/*
 * cprintf：内核的 printf 风格控制台输出入口。
 * 参数 fmt：格式字符串；其余参数：按格式说明提供的值，类型必须匹配。
 * 通过 va_start 取得参数列表，交给 vcprintf，再调用 va_end。
 * 返回提交输出的字符数。Lab1 的启动消息由 kern_init 调用本函数打印。
 */
int cprintf(const char *fmt, ...);
/*
 * vcprintf：用已有的可变参数列表向控制台打印格式化文本。
 * 参数 fmt：格式字符串；ap：与格式匹配的参数列表。
 * 以局部 cnt 作为回调上下文，调用 vprintfmt 完成解析和字符输出。
 * 返回提交输出的字符数；本函数不负责为调用者初始化或结束 ap。
 */
int vcprintf(const char *fmt, va_list ap);
/*
 * cputchar：直接输出单个字符的便捷接口。
 * 参数 c：字符值；返回值：无。直接调用 cons_putc，不解析格式、不增加换行。
 */
void cputchar(int c);
/*
 * cputs：输出以 NUL 结尾的字符串，并额外输出一个换行。
 * 参数 str：有效的字符串地址，不可为 NULL。
 * 逐字符调用 cputch，遇到字符串结束符后输出换行。
 * 返回字符数，包含追加的换行，不包含 NUL 结束符。
 */
int cputs(const char *str);
/*
 * getchar：反复轮询控制台输入，直到取得非零结果。
 * 参数：无；返回值：cons_getc 返回的第一个非零整数。
 * 零值会使函数继续忙等；负值也会直接返回，交由 readline 判断。
 * 本函数没有调度、休眠或超时机制；当前启动流程不使用它。
 */
int getchar(void);

/* libs/readline.c */
/*
 * readline：显示提示并读取一行控制台文本。
 * 参数 prompt：可为 NULL 的提示字符串；返回静态 buf 地址，输入错误返回 NULL。
 * 通过 getchar 忙等读取，可打印字符在容量允许时保存并回显；退格减少长度，
 * 回车或换行结束输入并添加 NUL。最多保存 BUFSIZE-1 个字符，超出部分丢弃。
 * 返回的缓冲区由后续调用覆盖，不可释放；需要保留时应自行复制。
 * 本函数不支持并发调用，当前 Lab1 启动没有调用输入功能。
 */
char *readline(const char *prompt);

/* libs/printfmt.c */
/*
 * printfmt：为任意字符输出回调提供可变参数格式化入口。
 * 参数 putch：每次接受一个字符和上下文的回调；putdat：回调上下文；
 * fmt：格式字符串；其余参数：格式匹配的值。返回值：无。
 * 初始化 ap，转交 vprintfmt，最后结束参数列表；不直接依赖控制台设备。
 */
void printfmt(void (*putch)(int, void *), void *putdat, const char *fmt, ...);
/*
 * vprintfmt：解析格式字符串，并通过 putch 逐字符输出。
 * 参数 putch：有效字符回调；putdat：回调上下文；fmt：格式串；ap：参数列表。
 * 返回值：无。普通文本直接输出；遇到 % 后解析标志、宽度、精度及 l 修饰符。
 * 支持 c、s、d、u、o、x、p、%、以及内核扩展 e（错误描述）。
 * width 表示字段宽度，precision 限制字符串长度，lflag 决定整数取参类型。
 * 这里的 # 对字符串把不可打印字符替换成 ?，不等同于标准 printf 的全部语义；
 * 数字统一交给 printnum，未知格式按字面输出。本实现是内核精简格式化器。
 */
void vprintfmt(void (*putch)(int, void *), void *putdat, const char *fmt, va_list ap);
/*
 * snprintf：把可变参数格式化结果写入有限容量字符串缓冲区。
 * 参数 str：目标缓冲区；size：容量，包含末尾 NUL；fmt：格式串；其余为参数。
 * 初始化参数列表后调用 vsnprintf，返回其结果：本应生成的字符数（不含 NUL），
 * 或参数检查失败时的负错误码；容量不足时结果会截断。
 */
int snprintf(char *str, size_t size, const char *fmt, ...);
/*
 * vsnprintf：使用已有参数列表，把格式化文本写入缓冲区。
 * 参数 str：目标缓冲区；size：包含 NUL 的容量，正常使用需至少为 1；
 * fmt：格式串；ap：与格式匹配的参数列表。
 * 保留最后一个字节给 NUL，通过 sprintputch 限制写入，再追加结束符。
 * 返回本应产生的字符数，即使截断也计入未写入部分；检查失败返回 -E_INVAL。
 * 注意原实现先计算 str+size-1 再检查参数，不能把它当作标准库那样
 * 安全支持 NULL/零容量的接口；本次保留既有代码，仅解释其使用前提。
 */
int vsnprintf(char *str, size_t size, const char *fmt, va_list ap);

#endif /* !__LIBS_STDIO_H__ */

