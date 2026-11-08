#include <clock.h>
#include <console.h>
#include <defs.h>
#include <intr.h>
#include <kdebug.h>
#include <kmonitor.h>
#include <pmm.h>
#include <stdio.h>
#include <string.h>
#include <trap.h>
#include <dtb.h>

int kern_init(void) __attribute__((noreturn));
void grade_backtrace(void);

/* 内核启动主线：初始化基础设施、Trap、内存和定时器，最后开启中断。 */
int kern_init(void) {
    extern char edata[], end[];
    // Clear BSS before parsing and storing the DTB memory information.
    memset(edata, 0, end - edata);
    dtb_init(); /* 解析设备树，取得平台信息。 */
    cons_init();  // init the console
    const char *message = "(THU.CST) os is loading ...\0";
    //cprintf("%s\n\n", message);
    cputs(message);

    print_kerninfo();

    // grade_backtrace();
    idt_init();  // 设置 stvec 指向统一 Trap 入口。

    pmm_init();  // 初始化物理内存管理。

    idt_init();  // 再次设置同一 Trap 入口；保留现有初始化顺序。

    clock_init();   // 配置时钟中断源并预约首次事件。
    intr_enable();  // 开启 S 态全局中断。

    /* do nothing */
    while (1)
        ;
}

/* 以下 noinline 调用链用于调试栈回溯，避免编译器内联消除栈帧。 */
void __attribute__((noinline))
grade_backtrace2(int arg0, int arg1, int arg2, int arg3) {
    mon_backtrace(0, NULL, NULL);
}

/* 将参数及其地址向下一层传递，构造可观察的调用栈。 */
void __attribute__((noinline)) grade_backtrace1(int arg0, int arg1) {
    grade_backtrace2(arg0, (uintptr_t)&arg0, arg1, (uintptr_t)&arg1);
}

/* 回溯测试调用链的第一层。 */
void __attribute__((noinline)) grade_backtrace0(int arg0, int arg1, int arg2) {
    grade_backtrace1(arg0, arg2);
}

/* 用固定参数启动回溯测试。 */
void grade_backtrace(void) { grade_backtrace0(0, (uintptr_t)kern_init, 0xffff0000); }

