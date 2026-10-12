/* tools/kernel.ld */

/*
 * Lab1 链接布局：把各输入目标文件中的段组合成一个 RISC-V 内核 ELF。
 * 基址为 0x80200000，入口符号为 kern_entry。裸镜像不保留 ELF 入口字段，
 * QEMU 加载位置与 OpenSBI 的交接地址仍需与此布局一致。
 */
OUTPUT_ARCH(riscv) /* 指定输出文件的指令集架构, 在riscv平台上运行 */
ENTRY(kern_entry)  /* 指定程序的入口点, 是一个叫做kern_entry的符号。我们之后会在汇编代码里定义它*/

BASE_ADDRESS = 0x80200000;/*定义了一个变量BASE_ADDRESS并初始化 */

/* 链接脚本剩余的部分是一整条 SECTIONS 指令，用来指定输出段布局。
 * SECTIONS 规定输出段的次序，点号 . 是当前位置计数器。
 * 通配表达式 *(...) 匹配所有输入目标文件中的段名，不是普通正则表达式。
 */
SECTIONS
{
    /* Load the kernel at this address: "." means the current address */
    . = BASE_ADDRESS;/*对 "."进行赋值*/
    /* 下面一句的意思是：从.的当前值（当前地址）开始放置一个叫做text的section.
     花括号内部的*(.text.kern_entry .text .stub .text.* .gnu.linkonce.t.*)是段名通配匹配表达式
     如果输入文件中有一个section的名称符合花括号内部的格式
     那么这个section就被加到输出文件的text这个section里
     输入文件中section的名称,有些是编译器自动生成的,有些是我们自己定义的*/
/*
 * 可执行代码先放置；显式优先匹配 .text.kern_entry，再收集其余代码段。
 * 当前 entry.S 使用普通 .text，实际入口地址还取决于输入对象顺序与链接布局。
 */
    .text : {
        *(.text.kern_entry) /*把输入中kern_entry这一段放到输出中text的开头*/
        *(.text .stub .text.* .gnu.linkonce.t.*)
    }

/*
 * etext 标记代码结束位置；PROVIDE 在需要且尚无该符号定义时提供边界符号。
 */
    PROVIDE(etext = .); /* Define the 'etext' symbol to this value */
    /* read only data, 只读数据，如程序里的常量 */
/*
 * 只读常量段：包含启动消息、格式字符串和格式化器的查找表等数据。
 */
    .rodata : {
        *(.rodata .rodata.* .gnu.linkonce.r.*)
    }

    /* 进行地址对齐，将 "."增加到 0x1000（4096）的整数倍，也就是下一个内存页的起始处 */
/*
 * ALIGN(0x1000) 将当前地址推进到 4096 的整数倍，并非 2 的 4096 次方。
 * 代码/只读数据结束后，后续数据从页边界开始；这是布局约束，不是开启分页。
 */
    . = ALIGN(0x1000);

/*
 * 保存有初始内容的可写数据，也包含 entry.S 预留的启动栈。
 */
    .data : {
        *(.data)
        *(.data.*)
    }
    /* small data section, 存储字节数小于某个标准的变量，一般是char, short等类型的 */
/*
 * 收集编译器生成的小数据段，使其与普通初始化数据都位于 edata 之前。
 */
    .sdata : {
        *(.sdata)
        *(.sdata.*)
    }

/*
 * edata 是初始化数据结束位置，也是 kern_init 清零区间的起点。
 */
    PROVIDE(edata = .);
    /* 初始化为零的数据 */
/*
 * 收集未初始化或零初始化的静态存储对象，包括小 BSS；
 * 通常只记录所需内存空间，运行时由 kern_init 负责置零。
 */
    .bss : {
        *(.bss)
        *(.bss.*)
        *(.sbss*)
    }

/*
 * end 标记 BSS 结束位置；end-edata 是 memset 使用的字节数，可能为零。
 */
    PROVIDE(end = .);
    /* /DISCARD/表示忽略，输入文件里 *(.eh_frame .note.GNU-stack)这些section都被忽略，不会加入到输出文件中 */
/*
 * 丢弃异常展开元数据和 GNU 栈注记，不把这些辅助段放入本实验的加载布局。
 */
    /DISCARD/ : {
        *(.eh_frame .note.GNU-stack)
    }
}