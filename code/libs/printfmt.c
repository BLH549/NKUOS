/*
 * 设备无关的精简格式化器：通过回调输出到控制台或有限容量缓冲区。
 * 错误码文字表与 error.h 一致；同一解析器由 cprintf、snprintf 等接口复用。
 */
/* 逐行：引入 uint64_t、size_t、NULL 等内核基础类型和宏。 */
#include <defs.h>
/* 逐行：引入 RISC-V 辅助宏；本文件的数字格式化主要使用 do_div 拆分商和余数。 */
#include <riscv.h>
/* 逐行：引入 E_INVAL 等错误编号与 MAXERROR，供错误文字格式化和参数检查使用。 */
#include <error.h>
/* 逐行：引入本项目的 stdio 接口声明；头文件由 Makefile 的 -Ilibs 找到，不是宿主 glibc。 */
#include <stdio.h>
/* 逐行：引入内核字符串/内存函数声明；初始化用 memset，格式化器用 strnlen。 */
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
/* 逐行：创建文件内可见的只读错误描述指针表，索引范围为 0 到 MAXERROR。 */
static const char * const error_string[MAXERROR + 1] = {
    /* 逐行：编号 0 没有错误描述，格式化 %e 时会走未知编号的回退输出。 */
    [0]                        NULL,
    /* 逐行：用指定下标初始化方式，把通用错误编号映射到未知错误描述。 */
    [E_UNSPECIFIED]            "unspecified error",
    /* 逐行：把无效进程编号映射到对应英文描述，避免依赖表项书写顺序。 */
    [E_BAD_PROC]            "bad process",
    /* 逐行：把无效参数错误映射到说明文本，也用于缓冲区格式化的参数检查。 */
    [E_INVAL]                "invalid parameter",
    /* 逐行：把内存不足错误映射到说明文本，不在此进行内存分配。 */
    [E_NO_MEM]                "out of memory",
    /* 逐行：把进程槽位不足错误映射到说明文本，当前 Lab1 尚未实现进程管理。 */
    [E_NO_FREE_PROC]        "out of processes",
    /* 逐行：把访问故障错误映射到描述文本，表项本身不执行异常处理。 */
    [E_FAULT]                "segmentation fault",
/* 逐行：结束数组初始化或结构类型定义；分号使这一条声明完整结束。 */
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
/* 逐行：声明后续函数仅在本文件可见，返回类型为 void；参数和实现写在下一行。 */
static void
/* 逐行：定义数字输出函数前半部分：putch 是字符回调，putdat 是原样传给回调的上下文。 */
printnum(void (*putch)(int, void*), void *putdat,
        /* 逐行：补全参数：num 是数值，base 是进制，width 是最小宽度，padc 是左侧补齐字符。 */
        unsigned long long num, unsigned base, int width, int padc) {
    /* 逐行：复制数值到 result，因为 do_div 会把它原地改为商，而 num 仍用于判断是否有高位。 */
    unsigned long long result = num;
    /* 逐行：计算当前最低位余数 mod，同时把 result 更新成商；要求 base 非零。 */
    unsigned mod = do_div(result, base);

    // 先递归输出商中的高位，使最终显示仍然从最高位到最低位。
    /* 逐行：数值至少为一个进制单位时还有更高位，需要递归输出商。 */
    if (num >= base) {
        /* 逐行：递归处理高位商，先输出高位；减小宽度，为当前层稍后输出的一位留位置。 */
        printnum(putch, putdat, result, base, width - 1, padc);
    /* 逐行：前一个条件不成立时进入备用分支；这里两条分支互斥，只执行其中一条。 */
    } else {
        // 到达最高位后，在它之前补足剩余最小宽度。
        /* 逐行：在最高位之前补齐剩余宽度；前置递减扣除当前数字本身占用的一位。 */
        while (-- width > 0)
            /* 逐行：通过回调输出一个补齐字符，并把同一上下文交给回调。 */
            putch(padc, putdat);
    /* 逐行：结束当前函数、分支或循环的作用域；右花括号本身不执行输出或修改数据。 */
    }
    // 当前层的余数对应一个数位，使用小写十六进制字符表。
    /* 逐行：把余数作为字符表下标，输出当前数位；递归返回顺序保证从高位到低位显示。 */
    putch("0123456789abcdef"[mod], putdat);
/* 逐行：结束当前函数、分支或循环的作用域；右花括号本身不执行输出或修改数据。 */
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
/* 逐行：声明文件内的无符号取参辅助函数，统一返回 unsigned long long。 */
static unsigned long long
/* 逐行：接收参数列表地址和 l 修饰符数量；读取后会推进调用者的取参状态。 */
getuint(va_list *ap, int lflag) {
    /* 逐行：两个或更多 l 表示按 long long 对应宽度读取可变参数。 */
    if (lflag >= 2) {
        /* 逐行：读取一个 unsigned long long 并返回；实际参数类型必须与格式匹配。 */
        return va_arg(*ap, unsigned long long);
    /* 逐行：结束当前函数、分支或循环的作用域；右花括号本身不执行输出或修改数据。 */
    }
    /* 逐行：未达到两个 l 但 lflag 非零，按单个 l 的 long 类型取参。 */
    else if (lflag) {
        /* 逐行：读取 unsigned long，提升到本函数的 unsigned long long 返回类型。 */
        return va_arg(*ap, unsigned long);
    /* 逐行：结束当前函数、分支或循环的作用域；右花括号本身不执行输出或修改数据。 */
    }
    /* 逐行：当前面的条件均不成立时进入默认分支，例如无 l 修饰符的普通整数取参。 */
    else {
        /* 逐行：读取未带 l 修饰符的 unsigned int，再以统一无符号返回类型交给格式化器。 */
        return va_arg(*ap, unsigned int);
    /* 逐行：结束当前函数、分支或循环的作用域；右花括号本身不执行输出或修改数据。 */
    }
/* 逐行：结束当前函数、分支或循环的作用域；右花括号本身不执行输出或修改数据。 */
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
/* 逐行：声明文件内的带符号取参辅助函数，统一返回 long long，以保留负数符号。 */
static long long
/* 逐行：根据 lflag 从同一可变参数列表读取带符号整数。 */
getint(va_list *ap, int lflag) {
    /* 逐行：两个或更多 l 表示按 long long 对应宽度读取可变参数。 */
    if (lflag >= 2) {
        /* 逐行：读取 long long 并返回，适用于带 ll 修饰符的格式。 */
        return va_arg(*ap, long long);
    /* 逐行：结束当前函数、分支或循环的作用域；右花括号本身不执行输出或修改数据。 */
    }
    /* 逐行：未达到两个 l 但 lflag 非零，按单个 l 的 long 类型取参。 */
    else if (lflag) {
        /* 逐行：读取 long 并按有符号规则扩展为 long long，适用于单个 l。 */
        return va_arg(*ap, long);
    /* 逐行：结束当前函数、分支或循环的作用域；右花括号本身不执行输出或修改数据。 */
    }
    /* 逐行：当前面的条件均不成立时进入默认分支，例如无 l 修饰符的普通整数取参。 */
    else {
        /* 逐行：读取普通 int 并符号扩展为 long long，不能按无符号类型替代读取。 */
        return va_arg(*ap, int);
    /* 逐行：结束当前函数、分支或循环的作用域；右花括号本身不执行输出或修改数据。 */
    }
/* 逐行：结束当前函数、分支或循环的作用域；右花括号本身不执行输出或修改数据。 */
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
/* 逐行：声明下一行函数的返回类型为 void，输出动作通过回调完成而非返回字符串。 */
void
/* 逐行：定义通用可变参数格式化入口，字符回调决定结果输出到控制台还是其他目标。 */
printfmt(void (*putch)(int, void*), void *putdat, const char *fmt, ...) {
    /* 逐行：声明由编译器维护的参数遍历状态，必须通过 va_start 初始化后才能取参。 */
    va_list ap;

    /* 逐行：以最后一个固定参数 fmt 初始化 ap，使后续格式化代码可以读取省略号参数。 */
    va_start(ap, fmt);
    /* 逐行：把回调、上下文、格式串及已有参数列表交给核心解析器，不直接访问硬件。 */
    vprintfmt(putch, putdat, fmt, ap);
    /* 逐行：结束本层可变参数遍历生命周期；本项目 stdarg.h 将该宏定义为空操作。 */
    va_end(ap);
/* 逐行：结束当前函数、分支或循环的作用域；右花括号本身不执行输出或修改数据。 */
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
/* 逐行：声明下一行函数的返回类型为 void，输出动作通过回调完成而非返回字符串。 */
void
/* 逐行：进入格式解析核心，按 fmt 消费 ap，并逐字符调用 putch。 */
vprintfmt(void (*putch)(int, void*), void *putdat, const char *fmt, va_list ap) {
    /* 逐行：声明只读文本遍历指针；register 是寄存器分配提示，不绑定某个硬件寄存器。 */
    register const char *p;
    /* 逐行：声明当前格式字符和错误编号变量，均在实际使用前被赋值。 */
    register int ch, err;
    /* 逐行：声明数字格式共用的数值容器；各整数和指针分支先赋值再统一输出。 */
    unsigned long long num;
    /* 逐行：声明进制、字段宽度、字符串精度、l 次数和替代格式标志，各 % 序列重新初始化状态。 */
    int base, width, precision, lflag, altflag;

    /* 逐行：持续解析格式串；遇到 NUL 执行 return 退出，不靠循环条件结束。 */
    while (1) {
        /* 逐行：读取当前格式字节并推进 fmt；普通字符直接输出，遇到 % 退出内层循环开始解析。 */
        while ((ch = *(unsigned char *)fmt ++) != '%') {
            /* 逐行：发现格式字符串结束符时结束整个格式化过程，避免继续读取后面的内存。 */
            if (ch == '\0') {
                /* 逐行：结束无返回值函数，把控制权交回调用者；格式串末尾的 NUL 不会输出。 */
                return;
            /* 逐行：结束当前函数、分支或循环的作用域；右花括号本身不执行输出或修改数据。 */
            }
            /* 逐行：通过指定回调输出当前字符，目标与计数方式由 putch/putdat 决定。 */
            putch(ch, putdat);
        /* 逐行：结束当前函数、分支或循环的作用域；右花括号本身不执行输出或修改数据。 */
        }

        // 每个新的 % 格式都重新设置状态，避免沿用前一个格式的宽度和标志。
        /* 逐行：每次进入新 % 格式时，把默认补齐字符设为空格。 */
        char padc = ' ';
        /* 逐行：用 -1 表示宽度和精度尚未指定；后续数字、星号和点号分支更新它们。 */
        width = precision = -1;
        /* 逐行：清零整数长度修饰与替代格式标志，防止前一个格式的状态泄漏到下一个。 */
        lflag = altflag = 0;

    /* 逐行：重新取格式字符的跳转标签；连续标志或修饰符处理后回到这里。 */
    reswitch:
        /* 逐行：取 % 后的下一个字节并推进指针，按字符选择标志、宽度或实际输出格式。 */
        switch (ch = *(unsigned char *)fmt ++) {

        // 字符串使用 - 标记左对齐；数字分支并未实现完整标准左对齐语义。
        /* 逐行：匹配左对齐标志；当前实现主要用于字符串分支，数字没有完整标准左对齐行为。 */
        case '-':
            /* 逐行：用减号作为左对齐状态标记；字符串分支据此跳过左侧补齐。 */
            padc = '-';
            /* 逐行：回到 reswitch 继续读取同一 % 序列的后续字符，不开始一个新的输出格式。 */
            goto reswitch;

        // 将补齐字符设为 0，替代默认空格。
        /* 逐行：匹配零补齐标志，例如 %08x 中最前面的 0。 */
        case '0':
            /* 逐行：将补齐字符从默认空格改为字符 0，后续数字输出使用这个值。 */
            padc = '0';
            /* 逐行：回到 reswitch 继续读取同一 % 序列的后续字符，不开始一个新的输出格式。 */
            goto reswitch;

        // 累积十进制宽度/精度，随后由 process_precision 分配给对应状态。
        /* 逐行：用 GNU C case 范围语法匹配非零数字，开始读取十进制宽度或精度。 */
        case '1' ... '9':
            /* 逐行：先清零数字累加器，循环末尾推进 fmt；退出条件放在循环体中的 break。 */
            for (precision = 0; ; ++ fmt) {
                /* 逐行：把当前 ASCII 数字转成数值，并按十进制逐位累加，例如 1、2 形成 12。 */
                precision = precision * 10 + ch - '0';
                /* 逐行：查看指针当前指向的下一个字节；这里不推进，便于保留第一个非数字格式字符。 */
                ch = *fmt;
                /* 逐行：下一个字节不再是十进制数字时停止累加，交回外层解析器处理。 */
                if (ch < '0' || ch > '9') {
                    /* 逐行：退出当前所在的最内层循环或 switch；不退出整个格式化函数。 */
                    break;
                /* 逐行：结束当前函数、分支或循环的作用域；右花括号本身不执行输出或修改数据。 */
                }
            /* 逐行：结束当前函数、分支或循环的作用域；右花括号本身不执行输出或修改数据。 */
            }
            /* 逐行：把已读数字或星号参数交给公共宽度/精度分配逻辑。 */
            goto process_precision;

        /* 星号从可变参数中读取宽度或字符串精度，而不是从格式串取数字。 */
        /* 逐行：匹配动态宽度或精度标志，从参数列表读取值，例如 %*s。 */
        case '*':
            /* 逐行：读取一个 int 作为宽度/精度候选值，并推进可变参数列表。 */
            precision = va_arg(ap, int);
            /* 逐行：把已读数字或星号参数交给公共宽度/精度分配逻辑。 */
            goto process_precision;

        /* 点号切换到精度字段；若此前无宽度，先把 width 设为零。 */
        /* 逐行：匹配精度分隔点；后面的数字或星号将用于字符串最大输出长度。 */
        case '.':
            /* 逐行：检查是否还没有设置宽度；同一条件在 process_precision 中区分首次数值与精度。 */
            if (width < 0)
                /* 逐行：将宽度设为零，标记后续数值不再作为首次宽度而会保留在 precision。 */
                width = 0;
            /* 逐行：回到 reswitch 继续读取同一 % 序列的后续字符，不开始一个新的输出格式。 */
            goto reswitch;

        /* 当前实现只在字符串输出中用此标志过滤不可打印字符。 */
        /* 逐行：匹配替代格式标志；当前实现对字符串过滤不可打印字符。 */
        case '#':
            /* 逐行：记录启用了替代格式；这里只存状态，具体替换在字符串输出分支执行。 */
            altflag = 1;
            /* 逐行：回到 reswitch 继续读取同一 % 序列的后续字符，不开始一个新的输出格式。 */
            goto reswitch;

        /* 逐行：公共标签：数字与星号读取后跳到这里，确定该值表示宽度还是精度。 */
        process_precision:
            /* 逐行：检查是否还没有设置宽度；同一条件在 process_precision 中区分首次数值与精度。 */
            if (width < 0)
                /* 逐行：首次遇到数值时将它移到 width，再把 precision 恢复为未指定状态。 */
                width = precision, precision = -1;
            /* 逐行：回到 reswitch 继续读取同一 % 序列的后续字符，不开始一个新的输出格式。 */
            goto reswitch;

        // l 出现一次读取 long，出现两次或更多读取 long long。
        /* 逐行：匹配整数长度修饰符；连续两个 l 对应 long long。 */
        case 'l':
            /* 逐行：累计 l 出现次数，后续 getint/getuint 根据此值选择取参类型。 */
            lflag ++;
            /* 逐行：回到 reswitch 继续读取同一 % 序列的后续字符，不开始一个新的输出格式。 */
            goto reswitch;

        // 字符参数按默认整数提升规则以 int 读取。
        /* 逐行：匹配字符格式；可变参数中的 char 经过默认提升，需要按 int 读取。 */
        case 'c':
            /* 逐行：读取一个提升后的字符参数，并立即通过回调输出。 */
            putch(va_arg(ap, int), putdat);
            /* 逐行：退出当前所在的最内层循环或 switch；不退出整个格式化函数。 */
            break;

        // 内核扩展 %e：负错误码先取绝对值，再查描述表或输出未知编号。
        /* 逐行：匹配内核扩展错误格式，参数是整数错误编号，不是标准 printf 的浮点 %e。 */
        case 'e':
            /* 逐行：从列表读取当前错误编号，后面统一正负编号的查表方式。 */
            err = va_arg(ap, int);
            /* 逐行：检测负错误编号；例如 -E_NO_MEM 与 E_NO_MEM 使用同一描述。 */
            if (err < 0) {
                /* 逐行：把负编号取反以用作正下标；当前实现没有单独处理最小 int 取反的边界。 */
                err = -err;
            /* 逐行：结束当前函数、分支或循环的作用域；右花括号本身不执行输出或修改数据。 */
            }
            /* 逐行：先用短路条件排除超范围编号，再查表；空表项也按未知错误处理。 */
            if (err > MAXERROR || (p = error_string[err]) == NULL) {
                /* 逐行：对未知错误编号递归调用格式化接口，输出 error 和十进制编号。 */
                printfmt(putch, putdat, "error %d", err);
            /* 逐行：结束当前函数、分支或循环的作用域；右花括号本身不执行输出或修改数据。 */
            }
            /* 逐行：当前面的条件均不成立时进入默认分支，例如无 l 修饰符的普通整数取参。 */
            else {
                /* 逐行：对已知错误输出查表所得描述，复用同一回调及上下文。 */
                printfmt(putch, putdat, "%s", p);
            /* 逐行：结束当前函数、分支或循环的作用域；右花括号本身不执行输出或修改数据。 */
            }
            /* 逐行：退出当前所在的最内层循环或 switch；不退出整个格式化函数。 */
            break;

        // 空指针显示为 (null)；精度限制长度，宽度控制前后补齐。
        /* 逐行：匹配字符串格式，参数必须为字符指针；后续处理空指针、精度和补齐。 */
        case 's':
            /* 逐行：读取字符串指针并判断是否为空，不直接解引用 NULL。 */
            if ((p = va_arg(ap, char *)) == NULL) {
                /* 逐行：空指针改用可打印占位文本，随后像普通字符串一样处理。 */
                p = "(null)";
            /* 逐行：结束当前函数、分支或循环的作用域；右花括号本身不执行输出或修改数据。 */
            }
            /* 逐行：只有指定正宽度且不是左对齐，才先输出字符串左侧补齐。 */
            if (width > 0 && padc != '-') {
                /* 逐行：减去受精度限制的字符串长度，输出剩余补齐；precision=-1 转 size_t 后通常相当于不限制扫描上限。 */
                for (width -= strnlen(p, precision); width > 0; width --) {
                    /* 逐行：通过回调输出一个补齐字符，并把同一上下文交给回调。 */
                    putch(padc, putdat);
                /* 逐行：结束当前函数、分支或循环的作用域；右花括号本身不执行输出或修改数据。 */
                }
            /* 逐行：结束当前函数、分支或循环的作用域；右花括号本身不执行输出或修改数据。 */
            }
            /* 逐行：逐字符读取并推进 p，受 NUL 与精度限制；每输出一位，剩余字段宽度减一。 */
            for (; (ch = *p ++) != '\0' && (precision < 0 || -- precision >= 0); width --) {
                /* 逐行：开启 # 时判断字符是否超出可打印 ASCII 范围，决定是否替换显示。 */
                if (altflag && (ch < ' ' || ch > '~')) {
                    /* 逐行：把不可打印字符显示为问号；只改变输出，不修改原字符串。 */
                    putch('?', putdat);
                /* 逐行：结束当前函数、分支或循环的作用域；右花括号本身不执行输出或修改数据。 */
                }
                /* 逐行：当前面的条件均不成立时进入默认分支，例如无 l 修饰符的普通整数取参。 */
                else {
                    /* 逐行：通过指定回调输出当前字符，目标与计数方式由 putch/putdat 决定。 */
                    putch(ch, putdat);
                /* 逐行：结束当前函数、分支或循环的作用域；右花括号本身不执行输出或修改数据。 */
                }
            /* 逐行：结束当前函数、分支或循环的作用域；右花括号本身不执行输出或修改数据。 */
            }
            /* 逐行：字符串结束后，按剩余宽度补空格；这是左对齐时的右侧补齐路径。 */
            for (; width > 0; width --) {
                /* 逐行：输出一个用于字段补齐的空格，并由回调统计它。 */
                putch(' ', putdat);
            /* 逐行：结束当前函数、分支或循环的作用域；右花括号本身不执行输出或修改数据。 */
            }
            /* 逐行：退出当前所在的最内层循环或 switch；不退出整个格式化函数。 */
            break;

        // 带符号十进制：先打印负号，再把绝对值交给统一数字分支。
        /* 逐行：匹配有符号十进制格式，负数先输出符号，再显示数值部分。 */
        case 'd':
            /* 逐行：按 l 修饰符读取带符号值，保存在统一数值容器中，随后检查其符号。 */
            num = getint(&ap, lflag);
            /* 逐行：把容器转换回带符号 long long 判断是否为负。 */
            if ((long long)num < 0) {
                /* 逐行：输出负号；这一步在数字递归前执行，不由 printnum 处理符号。 */
                putch('-', putdat);
                /* 逐行：对负值取反以输出其大小；当前实现没有单独规避最小 long long 取反的溢出边界。 */
                num = -(long long)num;
            /* 逐行：结束当前函数、分支或循环的作用域；右花括号本身不执行输出或修改数据。 */
            }
            /* 逐行：选择十进制字符拆分规则，随后与其他数值格式共用 number 分支。 */
            base = 10;
            /* 逐行：跳到统一数字输出标签，复用宽度、补齐和递归输出逻辑。 */
            goto number;

        // 无符号十进制，依据 lflag 读取相应宽度。
        /* 逐行：匹配无符号十进制格式，不需要输出负号。 */
        case 'u':
            /* 逐行：按 l 修饰符读取无符号整数，并推进参数列表。 */
            num = getuint(&ap, lflag);
            /* 逐行：选择十进制字符拆分规则，随后与其他数值格式共用 number 分支。 */
            base = 10;
            /* 逐行：跳到统一数字输出标签，复用宽度、补齐和递归输出逻辑。 */
            goto number;

        // 无符号八进制，复用同一数字输出算法。
        /* 逐行：匹配无符号八进制格式，数字字符只会使用 0 到 7。 */
        case 'o':
            /* 逐行：按 l 修饰符读取无符号整数，并推进参数列表。 */
            num = getuint(&ap, lflag);
            /* 逐行：把数字拆分进制设置为八进制。 */
            base = 8;
            /* 逐行：跳到统一数字输出标签，复用宽度、补齐和递归输出逻辑。 */
            goto number;

        // 指针：输出 0x 前缀，再按十六进制显示地址值。
        /* 逐行：匹配指针格式，输出固定 0x 前缀及十六进制地址值。 */
        case 'p':
            /* 逐行：输出地址前缀的第一个字符 0。 */
            putch('0', putdat);
            /* 逐行：输出地址前缀的第二个字符 x。 */
            putch('x', putdat);
            /* 逐行：按 void 指针读取参数，再转换为可按十六进制打印的地址数值。 */
            num = (unsigned long long)va_arg(ap, void *);
            /* 逐行：选择十六进制，使用 printnum 的小写 a 到 f 字符表。 */
            base = 16;
            /* 逐行：跳到统一数字输出标签，复用宽度、补齐和递归输出逻辑。 */
            goto number;

        // 小写十六进制，和其他数字格式共用 number 标签。
        /* 逐行：匹配小写十六进制整数格式；该分支不自动添加 0x 前缀。 */
        case 'x':
            /* 逐行：按 l 修饰符读取无符号整数，并推进参数列表。 */
            num = getuint(&ap, lflag);
            /* 逐行：选择十六进制，使用 printnum 的小写 a 到 f 字符表。 */
            base = 16;
        /* 逐行：各整数和指针分支汇合的标签，此时 num/base 已由相应分支赋值。 */
        number:
            /* 逐行：调用递归数字输出，参数包含数值、进制、最小字段宽度和补齐字符。 */
            printnum(putch, putdat, num, base, width, padc);
            /* 逐行：退出当前所在的最内层循环或 switch；不退出整个格式化函数。 */
            break;

        // %% 输出一个字面的百分号，不消耗参数。
        /* 逐行：匹配 %% 转义，只输出一个百分号，不消费可变参数。 */
        case '%':
            /* 逐行：通过指定回调输出当前字符，目标与计数方式由 putch/putdat 决定。 */
            putch(ch, putdat);
            /* 逐行：退出当前所在的最内层循环或 switch；不退出整个格式化函数。 */
            break;

        // 未知格式先输出 %，回退格式指针，使剩余内容按普通文本输出。
        /* 逐行：匹配未支持的格式字符，采用字面回退，保留原格式文本。 */
        default:
            /* 逐行：先输出已遇到的百分号，后续将不认识的格式字符作为普通文本处理。 */
            putch('%', putdat);
            /* 逐行：回退格式指针，直到其前一个字节是 %，使下一轮普通文本扫描重新读取后续内容。 */
            for (fmt --; fmt[-1] != '%'; fmt --)
                /* do nothing */;
            /* 逐行：退出当前所在的最内层循环或 switch；不退出整个格式化函数。 */
            break;
        /* 逐行：结束当前函数、分支或循环的作用域；右花括号本身不执行输出或修改数据。 */
        }
    /* 逐行：结束当前函数、分支或循环的作用域；右花括号本身不执行输出或修改数据。 */
    }
/* 逐行：结束当前函数、分支或循环的作用域；右花括号本身不执行输出或修改数据。 */
}

/* sprintbuf is used to save enough information of a buffer */
/*
 * 缓冲区回调的状态：buf 指向下一个可写位置，ebuf 指向预留结束符的位置，
 * cnt 记录本应生成的字符数，可能大于实际存入缓冲区的字符数。
 */
/* 逐行：定义缓冲区输出上下文：记录写入位置、预留结束符位置及逻辑字符总数。 */
struct sprintbuf {
    /* 逐行：buf 指向下一个可写字符位置；每次实际写入后前进一个字节。 */
    char *buf;            // address pointer points to the first unused memory
    /* 逐行：ebuf 指向最后保留给 NUL 的位置，普通字符只允许写在它之前。 */
    char *ebuf;            // points the end of the buffer
    /* 逐行：cnt 计入所有本应输出的字符，包括容量不足而未存入的字符。 */
    int cnt;            // the number of characters that have been placed in this buffer
/* 逐行：结束数组初始化或结构类型定义；分号使这一条声明完整结束。 */
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
/* 逐行：声明后续函数仅在本文件可见，返回类型为 void；参数和实现写在下一行。 */
static void
/* 逐行：定义缓冲区字符回调：ch 是待输出字符，b 保存容量边界和计数状态。 */
sprintputch(int ch, struct sprintbuf *b) {
    /* 逐行：不论缓冲区是否已满，先增加逻辑字符数，供 snprintf 返回完整所需长度。 */
    b->cnt ++;
    /* 逐行：检查当前写入位置尚未达到为 NUL 保留的末尾，避免普通字符越过容量。 */
    if (b->buf < b->ebuf) {
        /* 逐行：在当前 buf 指向的位置写入字符，再推进指针；容量满时整条语句不执行。 */
        *b->buf ++ = ch;
    /* 逐行：结束当前函数、分支或循环的作用域；右花括号本身不执行输出或修改数据。 */
    }
/* 逐行：结束当前函数、分支或循环的作用域；右花括号本身不执行输出或修改数据。 */
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
/* 逐行：声明下一行函数返回 int，用于返回字符数量或负错误编号。 */
int
/* 逐行：进入有限缓冲区可变参数接口，size 包含结束符空间，格式参数随后交给 vsnprintf。 */
snprintf(char *str, size_t size, const char *fmt, ...) {
    /* 逐行：声明由编译器维护的参数遍历状态，必须通过 va_start 初始化后才能取参。 */
    va_list ap;
    /* 逐行：声明结果计数变量；后续调用先赋值，再由 return 返回，当前不读取未初始化值。 */
    int cnt;
    /* 逐行：以最后一个固定参数 fmt 初始化 ap，使后续格式化代码可以读取省略号参数。 */
    va_start(ap, fmt);
    /* 逐行：调用已有参数列表版本，保存逻辑输出长度或负错误结果。 */
    cnt = vsnprintf(str, size, fmt, ap);
    /* 逐行：结束本层可变参数遍历生命周期；本项目 stdarg.h 将该宏定义为空操作。 */
    va_end(ap);
    /* 逐行：返回本次累计字符数；cputs 中包含追加换行，格式化接口中不包含 NUL。 */
    return cnt;
/* 逐行：结束当前函数、分支或循环的作用域；右花括号本身不执行输出或修改数据。 */
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
/* 逐行：声明下一行函数返回 int，用于返回字符数量或负错误编号。 */
int
/* 逐行：进入缓冲区格式化核心；正常调用要求 str 有效且 size 至少为 1。 */
vsnprintf(char *str, size_t size, const char *fmt, va_list ap) {
    /* 逐行：建立回调上下文：起点是 str，末尾保留一个字节给 NUL，逻辑计数初始为零。 */
    struct sprintbuf b = {str, str + size - 1, 0};
    /* 逐行：检查空指针或无可用容量；原实现已先计算末尾指针，不能据此认为 NULL/size=0 安全。 */
    if (str == NULL || b.buf > b.ebuf) {
        /* 逐行：返回负的无效参数编号，调用方应区分它和非负字符数量。 */
        return -E_INVAL;
    /* 逐行：结束当前函数、分支或循环的作用域；右花括号本身不执行输出或修改数据。 */
    }
    // 回调只写入允许区域，但继续统计超出容量的字符。
    /* 逐行：让相同格式解析器改用缓冲区回调，输出容量控制与总计数由 b 维护。 */
    vprintfmt((void*)sprintputch, &b, fmt, ap);
    // 在当前写入位置添加结束符，确保截断后的字符串仍有 NUL。
    /* 逐行：在实际写入后的当前位置追加 NUL，使容量不足时的截断结果仍为字符串。 */
    *b.buf = '\0';
    /* 逐行：返回本应生成的字符数量，不含 NUL；即使截断也计入未写入的部分。 */
    return b.cnt;
/* 逐行：结束当前函数、分支或循环的作用域；右花括号本身不执行输出或修改数据。 */
}

