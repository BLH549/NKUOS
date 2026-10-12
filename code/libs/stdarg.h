/*
 * 可变参数支持由 GCC 内建机制提供，参数在寄存器和栈中的布局由编译器处理。
 * va_list 保存取参状态，不应把它手工当作连续内存中的普通指针使用。
 */
#ifndef __LIBS_STDARG_H__
#define __LIBS_STDARG_H__

/* compiler provides size of save area */
typedef __builtin_va_list va_list;

/*
 * va_start(ap,last)：以最后一个固定参数 last 为锚点初始化参数列表。
 * 必须在使用 va_arg 前调用；实际布局和寄存器保存区由 GCC 管理。
 */
#define va_start(ap, last)              (__builtin_va_start(ap, last))
/*
 * va_arg(ap,type)：按 type 读取下一个参数并推进列表。
 * 窄整数会默认提升为 int，float 会提升为 double；读取类型应遵守调用约定。
 */
#define va_arg(ap, type)                (__builtin_va_arg(ap, type))
/*
 * va_end(ap)：参数读取结束的配套接口。本模板将其定义为空操作，
 * 保留调用点以表达生命周期；不要据此推断所有平台都无需清理参数列表。
 */
#define va_end(ap)                      /*nothing*/

#endif /* !__LIBS_STDARG_H__ */

