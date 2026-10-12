/*
 * Lab1 内核的汇编入口，地址由 tools/kernel.ld 安排。
 * OpenSBI 交接后先执行这里，再进入 C 函数 kern_init。
 * 入口不是普通被调用函数，不需要建立返回到固件的调用栈帧。
 */
#include <mmu.h>
#include <memlayout.h>

/*
 * .text 标记代码段；ax 表示可分配且可执行，progbits 表示段内有实际内容。
 * .globl 导出入口符号，供链接脚本 ENTRY(kern_entry) 和 GDB 使用。
 */
    .section .text,"ax",%progbits
    .globl kern_entry
/*
 * kern_entry：启动入口，无 C 形式的参数列表，也不会正常返回。
 * 这里只建立 SP 并跳到 kern_init；不在此设置页表或安装内核异常处理。
 */
kern_entry:
/*
 * la 取得栈顶符号的地址并写入 x2/SP，不是读取该地址中的数据。
 * RISC-V 栈向低地址增长，初始 SP 使用预留区的高地址边界。
 * 这是伪指令，链接后的机器指令可能多于一条，GDB si 按机器指令单步。
 */
    la sp, bootstacktop

/*
 * tail 以尾调用方式跳到 C 初始化入口，不向 RA 写入新的返回地址。
 * kern_init 的末尾无限循环，因此不需要返回到本入口。
 */
    tail kern_init

/*
 * 启动栈放在 .data 中，由汇编器预留空间，不在运行时动态申请。
 * 因此 kern_init 对 [edata,end) 的 BSS 清零不会覆盖当前正在使用的栈。
 */
.section .data
    # .align 2^12
/*
 * 此处 GNU RISC-V 汇编的 .align 参数是二进制指数：12 表示按 4096 字节对齐。
 */
    .align PGSHIFT
    .global bootstack
/*
 * bootstack 是栈区低地址起点，.space 预留 KSTACKSIZE=2*4096 字节。
 */
bootstack:
    .space KSTACKSIZE
    .global bootstacktop
/*
 * bootstacktop 是栈区高地址边界，标号本身不再占用额外字节。
 * 可用栈区为 [bootstack,bootstacktop)，两页尺寸也保证栈顶满足 16 字节对齐。
 */
bootstacktop: