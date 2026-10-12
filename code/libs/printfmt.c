/*
 * 设备无关的精简格式化器：通过回调输出到控制台或有限容量缓冲区。
 * 错误码文字表与 error.h 一致；同一解析器由 cprintf、snprintf 等接口复用。
 */
#include <defs.h>
#include <riscv.h>
#include <error.h>
#include <stdio.h>
#include <string.h>

/* *
 * Space or zero padding and a field width are supported for the numeric
 * formats only.
 *
 * The special format %e takes an integer error code
 * and prints a string describing the error.
 * The integer may be positive or negative,
 * so that -E_NO_MEM and E_NO_MEM are equivalent.
 * */

/*
 * 错误编号到英文描述的只读映射。0 项为空，未知编号由 %e 输出 error 加编号。
 * 指定下标初始化保证表项与 error.h 中的编号一致。
 */
static const char * const error_string[MAXERROR + 1] = {
    [0]                        NULL,
    [E_UNSPECIFIED]            "unspecified error",
    [E_BAD_PROC]            "bad process",
    [E_INVAL]                "invalid parameter",
    [E_NO_MEM]                "out of memory",
    [E_NO_FREE_PROC]        "out of processes",
    [E_FAULT]                "segmentation fault",
};

/* *
 * printnum - print a number (base <= 16) in reverse order
 * @putch:        specified putch function, print a single character
 * @putdat:        used by @putch function
 * @num:        the number will be printed
 * @base:        base for print, must be in [1, 16]
 * @width:         maximum number of digits, if the actual width is less than @width, use @padc instead
 * @padc:        character that padded on the left if the actual width is less than @width
 * */
/*
 * printnum：按指定进制输出无符号整数，并在左侧补齐最小字段宽度。
 * 参数 putch：字符回调；putdat：原样交给回调的上下文；num：数值；
 * base：此实现适用 2～16；width：最小宽度；padc：补齐字符。返回值：无。
 * do_div 把 result 改成商并返回余数；递归先输出高位，回到当前层再输出低位。
 * 数值比宽度长时不会截断；递归终点在最高位前输出所需补齐字符。
 */
static void
printnum(void (*putch)(int, void*), void *putdat,
        unsigned long long num, unsigned base, int width, int padc) {
    unsigned long long result = num;
    unsigned mod = do_div(result, base);

    // 先递归输出商中的高位，使最终显示仍然从最高位到最低位。
    if (num >= base) {
        printnum(putch, putdat, result, base, width - 1, padc);
    } else {
        // 到达最高位后，在它之前补足剩余最小宽度。
        while (-- width > 0)
            putch(padc, putdat);
    }
    // 当前层的余数对应一个数位，使用小写十六进制字符表。
    putch("0123456789abcdef"[mod], putdat);
}

/* *
 * getuint - get an unsigned int of various possible sizes from a varargs list
 * @ap:            a varargs list pointer
 * @lflag:        determines the size of the vararg that @ap points to
 * */
/*
 * getuint：从可变参数列表取出一个无符号整数并统一成 unsigned long long。
 * 参数 ap：参数列表地址，会推进取参位置；lflag：累计的 l 修饰符数量。
 * 0 读取 unsigned int，1 读取 unsigned long，至少 2 读取 unsigned long long。
 * 返回读取值；调用者传入的实际类型必须与格式修饰符一致。
 */
static unsigned long long
getuint(va_list *ap, int lflag) {
    if (lflag >= 2) {
        return va_arg(*ap, unsigned long long);
    }
    else if (lflag) {
        return va_arg(*ap, unsigned long);
    }
    else {
        return va_arg(*ap, unsigned int);
    }
}

/* *
 * getint - same as getuint but signed, we can't use getuint because of sign extension
 * @ap:            a varargs list pointer
 * @lflag:        determines the size of the vararg that @ap points to
 * */
/*
 * getint：从可变参数列表取出带符号整数，返回 long long。
 * 参数 ap：参数列表地址，会推进取参位置；lflag：l 修饰符数量。
 * 0、1、至少 2 分别按 int、long、long long 读取，保留符号扩展。
 * 不能简单复用 getuint，否则窄类型负数的扩展方式会出错。
 */
static long long
getint(va_list *ap, int lflag) {
    if (lflag >= 2) {
        return va_arg(*ap, long long);
    }
    else if (lflag) {
        return va_arg(*ap, long);
    }
    else {
        return va_arg(*ap, int);
    }
}

/* *
 * printfmt - format a string and print it by using putch
 * @putch:        specified putch function, print a single character
 * @putdat:        used by @putch function
 * @fmt:        the format string to use
 * */
/*
 * printfmt：为任意字符输出回调提供可变参数格式化入口。
 * 参数 putch：每次接受一个字符和上下文的回调；putdat：回调上下文；
 * fmt：格式字符串；其余参数：格式匹配的值。返回值：无。
 * 初始化 ap，转交 vprintfmt，最后结束参数列表；不直接依赖控制台设备。
 */
void
printfmt(void (*putch)(int, void*), void *putdat, const char *fmt, ...) {
    va_list ap;

    va_start(ap, fmt);
    vprintfmt(putch, putdat, fmt, ap);
    va_end(ap);
}

/* *
 * vprintfmt - format a string and print it by using putch, it's called with a va_list
 * instead of a variable number of arguments
 * @putch:        specified putch function, print a single character
 * @putdat:        used by @putch function
 * @fmt:        the format string to use
 * @ap:            arguments for the format string
 *
 * Call this function if you are already dealing with a va_list.
 * Or you probably want printfmt() instead.
 * */
/*
 * vprintfmt：解析格式字符串，并通过 putch 逐字符输出。
 * 参数 putch：有效字符回调；putdat：回调上下文；fmt：格式串；ap：参数列表。
 * 返回值：无。普通文本直接输出；遇到 % 后解析标志、宽度、精度及 l 修饰符。
 * 支持 c、s、d、u、o、x、p、%、以及内核扩展 e（错误描述）。
 * width 表示字段宽度，precision 限制字符串长度，lflag 决定整数取参类型。
 * 这里的 # 对字符串把不可打印字符替换成 ?，不等同于标准 printf 的全部语义；
 * 数字统一交给 printnum，未知格式按字面输出。本实现是内核精简格式化器。
 */
void
vprintfmt(void (*putch)(int, void*), void *putdat, const char *fmt, va_list ap) {
    register const char *p;
    register int ch, err;
    unsigned long long num;
    int base, width, precision, lflag, altflag;

    while (1) {
        while ((ch = *(unsigned char *)fmt ++) != '%') {
            if (ch == '\0') {
                return;
            }
            putch(ch, putdat);
        }

        // 每个新的 % 格式都重新设置状态，避免沿用前一个格式的宽度和标志。
        char padc = ' ';
        width = precision = -1;
        lflag = altflag = 0;

    reswitch:
        switch (ch = *(unsigned char *)fmt ++) {

        // 字符串使用 - 标记左对齐；数字分支并未实现完整标准左对齐语义。
        case '-':
            padc = '-';
            goto reswitch;

        // 将补齐字符设为 0，替代默认空格。
        case '0':
            padc = '0';
            goto reswitch;

        // 累积十进制宽度/精度，随后由 process_precision 分配给对应状态。
        case '1' ... '9':
            for (precision = 0; ; ++ fmt) {
                precision = precision * 10 + ch - '0';
                ch = *fmt;
                if (ch < '0' || ch > '9') {
                    break;
                }
            }
            goto process_precision;

        /* 星号从可变参数中读取宽度或字符串精度，而不是从格式串取数字。 */
        case '*':
            precision = va_arg(ap, int);
            goto process_precision;

        /* 点号切换到精度字段；若此前无宽度，先把 width 设为零。 */
        case '.':
            if (width < 0)
                width = 0;
            goto reswitch;

        /* 当前实现只在字符串输出中用此标志过滤不可打印字符。 */
        case '#':
            altflag = 1;
            goto reswitch;

        process_precision:
            if (width < 0)
                width = precision, precision = -1;
            goto reswitch;

        // l 出现一次读取 long，出现两次或更多读取 long long。
        case 'l':
            lflag ++;
            goto reswitch;

        // 字符参数按默认整数提升规则以 int 读取。
        case 'c':
            putch(va_arg(ap, int), putdat);
            break;

        // 内核扩展 %e：负错误码先取绝对值，再查描述表或输出未知编号。
        case 'e':
            err = va_arg(ap, int);
            if (err < 0) {
                err = -err;
            }
            if (err > MAXERROR || (p = error_string[err]) == NULL) {
                printfmt(putch, putdat, "error %d", err);
            }
            else {
                printfmt(putch, putdat, "%s", p);
            }
            break;

        // 空指针显示为 (null)；精度限制长度，宽度控制前后补齐。
        case 's':
            if ((p = va_arg(ap, char *)) == NULL) {
                p = "(null)";
            }
            if (width > 0 && padc != '-') {
                for (width -= strnlen(p, precision); width > 0; width --) {
                    putch(padc, putdat);
                }
            }
            for (; (ch = *p ++) != '\0' && (precision < 0 || -- precision >= 0); width --) {
                if (altflag && (ch < ' ' || ch > '~')) {
                    putch('?', putdat);
                }
                else {
                    putch(ch, putdat);
                }
            }
            for (; width > 0; width --) {
                putch(' ', putdat);
            }
            break;

        // 带符号十进制：先打印负号，再把绝对值交给统一数字分支。
        case 'd':
            num = getint(&ap, lflag);
            if ((long long)num < 0) {
                putch('-', putdat);
                num = -(long long)num;
            }
            base = 10;
            goto number;

        // 无符号十进制，依据 lflag 读取相应宽度。
        case 'u':
            num = getuint(&ap, lflag);
            base = 10;
            goto number;

        // 无符号八进制，复用同一数字输出算法。
        case 'o':
            num = getuint(&ap, lflag);
            base = 8;
            goto number;

        // 指针：输出 0x 前缀，再按十六进制显示地址值。
        case 'p':
            putch('0', putdat);
            putch('x', putdat);
            num = (unsigned long long)va_arg(ap, void *);
            base = 16;
            goto number;

        // 小写十六进制，和其他数字格式共用 number 标签。
        case 'x':
            num = getuint(&ap, lflag);
            base = 16;
        number:
            printnum(putch, putdat, num, base, width, padc);
            break;

        // %% 输出一个字面的百分号，不消耗参数。
        case '%':
            putch(ch, putdat);
            break;

        // 未知格式先输出 %，回退格式指针，使剩余内容按普通文本输出。
        default:
            putch('%', putdat);
            for (fmt --; fmt[-1] != '%'; fmt --)
                /* do nothing */;
            break;
        }
    }
}

/* sprintbuf is used to save enough information of a buffer */
/*
 * 缓冲区回调的状态：buf 指向下一个可写位置，ebuf 指向预留结束符的位置，
 * cnt 记录本应生成的字符数，可能大于实际存入缓冲区的字符数。
 */
struct sprintbuf {
    char *buf;            // address pointer points to the first unused memory
    char *ebuf;            // points the end of the buffer
    int cnt;            // the number of characters that have been placed in this buffer
};

/* *
 * sprintputch - 'print' a single character in a buffer
 * @ch:            the character will be printed
 * @b:            the buffer to place the character @ch
 * */
/*
 * sprintputch：将格式化字符写入有限容量缓冲区的回调。
 * 参数 ch：字符；b：记录当前位置、写入边界和计数的缓冲区上下文。
 * 先无条件增加 cnt，只有 buf<ebuf 才写入并推进 buf；返回值：无。
 * 因此 cnt 统计本应输出的字符数，超过容量时丢弃字符但仍继续计数。
 * ebuf 指向预留给末尾 NUL 的位置，不允许普通字符写到那里。
 */
static void
sprintputch(int ch, struct sprintbuf *b) {
    b->cnt ++;
    if (b->buf < b->ebuf) {
        *b->buf ++ = ch;
    }
}

/* *
 * snprintf - format a string and place it in a buffer
 * @str:        the buffer to place the result into
 * @size:        the size of buffer, including the trailing null space
 * @fmt:        the format string to use
 * */
/*
 * snprintf：把可变参数格式化结果写入有限容量字符串缓冲区。
 * 参数 str：目标缓冲区；size：容量，包含末尾 NUL；fmt：格式串；其余为参数。
 * 初始化参数列表后调用 vsnprintf，返回其结果：本应生成的字符数（不含 NUL），
 * 或参数检查失败时的负错误码；容量不足时结果会截断。
 */
int
snprintf(char *str, size_t size, const char *fmt, ...) {
    va_list ap;
    int cnt;
    va_start(ap, fmt);
    cnt = vsnprintf(str, size, fmt, ap);
    va_end(ap);
    return cnt;
}

/* *
 * vsnprintf - format a string and place it in a buffer, it's called with a va_list
 * instead of a variable number of arguments
 * @str:        the buffer to place the result into
 * @size:        the size of buffer, including the trailing null space
 * @fmt:        the format string to use
 * @ap:            arguments for the format string
 *
 * The return value is the number of characters which would be generated for the
 * given input, excluding the trailing '\0'.
 *
 * Call this function if you are already dealing with a va_list.
 * Or you probably want snprintf() instead.
 * */
/*
 * vsnprintf：使用已有参数列表，把格式化文本写入缓冲区。
 * 参数 str：目标缓冲区；size：包含 NUL 的容量，正常使用需至少为 1；
 * fmt：格式串；ap：与格式匹配的参数列表。
 * 保留最后一个字节给 NUL，通过 sprintputch 限制写入，再追加结束符。
 * 返回本应产生的字符数，即使截断也计入未写入部分；检查失败返回 -E_INVAL。
 * 注意原实现先计算 str+size-1 再检查参数，不能把它当作标准库那样
 * 安全支持 NULL/零容量的接口；本次保留既有代码，仅解释其使用前提。
 */
int
vsnprintf(char *str, size_t size, const char *fmt, va_list ap) {
    struct sprintbuf b = {str, str + size - 1, 0};
    if (str == NULL || b.buf > b.ebuf) {
        return -E_INVAL;
    }
    // 回调只写入允许区域，但继续统计超出容量的字符。
    vprintfmt((void*)sprintputch, &b, fmt, ap);
    // 在当前写入位置添加结束符，确保截断后的字符串仍有 NUL。
    *b.buf = '\0';
    return b.cnt;
}

