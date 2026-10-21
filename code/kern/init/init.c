/*
 * Lab1 的 C 初始化入口。执行前由 entry.S 建立栈，地址边界由 kernel.ld 提供。
 */
/* 逐行：引入本项目的 stdio 接口声明；头文件由 Makefile 的 -Ilibs 找到，不是宿主 glibc。 */
#include <stdio.h>
/* 逐行：引入内核字符串/内存函数声明；初始化用 memset，格式化器用 strnlen。 */
#include <string.h>
/* 逐行：引入 SBI 包装接口声明，使 C 代码可以通过函数调用发起固件服务请求。 */
#include <sbi.h>
/*
 * kern_init：汇编入口建立启动栈后进入的 C 初始化函数。
 * 参数：无。返回值：不会正常返回，noreturn 声明与末尾无限循环相对应。
 * 先利用链接脚本导出的 edata、end 清零 BSS，再输出启动消息。
 * 边界符号代表地址，不是在这里分配的数组；若二者相等，memset 的长度为零。
 * 启动栈位于 .data，不在清零范围内；执行本函数时栈已经可用。
 */
/* 逐行：提前声明入口函数不正常返回，供编译器分析控制流；这里只声明，不执行初始化。 */
int kern_init(void) __attribute__((noreturn));

/* 逐行：进入 C 初始化函数；entry.S 已经设置 SP，因此可以调用 memset 和 cprintf。 */
int kern_init(void) {
    /* 链接器导出的边界地址；差值就是清零区间的字节长度。 */
    /* 逐行：引用链接器定义的两个地址边界；extern 不在这里分配数组空间。 */
    extern char edata[], end[];
    /* 逐行：把 [edata,end) 按字节填零；差值为零时不写内存，用于建立 BSS 零初值。 */
    memset(edata, 0, end - edata);

    /* 逐行：让局部指针指向只读启动消息；末尾的转义序列表示一个换行字符。 */
    const char *message = "(THU.CST) os is loading ...\n";
    /* message 自带换行，格式串再追加两个换行，保持模板原有输出。 */
    /* 逐行：用 %s 输出 message，再追加两个换行；由后续字符回调最终转交 OpenSBI。 */
    cprintf("%s\n\n", message);
   /* 初始化结束后持续停留在内核，没有退出到固件的返回路径。 */
   /* 逐行：条件恒真，初始化和输出后反复循环，保证内核不返回汇编入口或固件。 */
   while (1)
        /* 逐行：空语句本身不做操作；作为循环体时只保留循环条件与指针推进等控制效果。 */
        ;
/* 逐行：结束当前函数、分支或循环的作用域；右花括号本身不执行输出或修改数据。 */
}
