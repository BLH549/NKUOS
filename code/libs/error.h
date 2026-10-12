/*
 * 内核错误编号。宏值为正，函数可以返回负编号；格式化器的 %e 接受正负编号。
 * 这里只提供常量，不实现错误处理；printfmt.c 的 error_string 按这些值索引。
 */
#ifndef __LIBS_ERROR_H__
#define __LIBS_ERROR_H__

/* kernel error codes -- keep in sync with list in lib/printfmt.c */
#define E_UNSPECIFIED        1    // Unspecified or unknown problem
#define E_BAD_PROC            2    // Process doesn't exist or otherwise
#define E_INVAL                3    // Invalid parameter
#define E_NO_MEM            4    // Request failed due to memory shortage
#define E_NO_FREE_PROC        5    // Attempt to create a new process beyond
#define E_FAULT                6    // Memory fault

/* the maximum allowed */
#define MAXERROR            6

#endif /* !__LIBS_ERROR_H__ */

