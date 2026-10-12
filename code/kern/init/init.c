/*
 * Lab1 的 C 初始化入口。执行前由 entry.S 建立栈，地址边界由 kernel.ld 提供。
 */
#include <stdio.h>
#include <string.h>
#include <sbi.h>
/*
 * kern_init：汇编入口建立启动栈后进入的 C 初始化函数。
 * 参数：无。返回值：不会正常返回，noreturn 声明与末尾无限循环相对应。
 * 先利用链接脚本导出的 edata、end 清零 BSS，再输出启动消息。
 * 边界符号代表地址，不是在这里分配的数组；若二者相等，memset 的长度为零。
 * 启动栈位于 .data，不在清零范围内；执行本函数时栈已经可用。
 */
int kern_init(void) __attribute__((noreturn));

int kern_init(void) {
    /* 链接器导出的边界地址；差值就是清零区间的字节长度。 */
    extern char edata[], end[];
    memset(edata, 0, end - edata);

    const char *message = "(THU.CST) os is loading ...\n";
    /* message 自带换行，格式串再追加两个换行，保持模板原有输出。 */
    cprintf("%s\n\n", message);
   /* 初始化结束后持续停留在内核，没有退出到固件的返回路径。 */
   while (1)
        ;
}
