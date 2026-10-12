/*
 * 页大小及对齐参数：PGSHIFT=12，PGSIZE=1<<12=4096 字节。
 * entry.S 使用 PGSHIFT 做汇编对齐，memlayout.h 使用 PGSIZE 计算栈大小。
 * 这些常量并不表示 Lab1 已经启用分页或建立页表。
 */
#ifndef __KERN_MM_MMU_H__
#define __KERN_MM_MMU_H__

#define PGSIZE          4096                    // bytes mapped by a page
#define PGSHIFT         12                      // log2(PGSIZE)

#endif /* !__KERN_MM_MMU_H__ */
