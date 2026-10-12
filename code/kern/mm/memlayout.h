/*
 * 内核启动栈的尺寸定义：KSTACKPAGE=2，KSTACKSIZE=8192 字节。
 * PGSIZE 来自 mmu.h；entry.S 先包含 mmu.h，再包含此文件。
 * 只定义尺寸，实际栈空间由 entry.S 的 .space 在 .data 段预留。
 */
#ifndef __KERN_MM_MEMLAYOUT_H__
#define __KERN_MM_MEMLAYOUT_H__

#define KSTACKPAGE          2                           // # of pages in kernel stack
#define KSTACKSIZE          (KSTACKPAGE * PGSIZE)       // sizeof kernel stack

#endif /* !__KERN_MM_MEMLAYOUT_H__ */

