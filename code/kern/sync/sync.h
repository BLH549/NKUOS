#ifndef __KERN_SYNC_SYNC_H__
#define __KERN_SYNC_SYNC_H__

#include <defs.h>
#include <intr.h>
#include <riscv.h>

/* 保存当前 hart 的 SIE 状态；原先已开才关闭并返回真。 */
static inline bool __intr_save(void) {
    if (read_csr(sstatus) & SSTATUS_SIE) {
        intr_disable();
        return 1;
    }
    return 0;
}

/* 仅在进入临界区前已开启时重新开启，支持嵌套使用。 */
static inline void __intr_restore(bool flag) {
    if (flag) {
        intr_enable();
    }
}

/* do-while 将多条语句包装为单条语句，适合 if/else 调用。 */
#define local_intr_save(x) \
    do {                   \
        x = __intr_save(); \
    } while (0)
/* 恢复调用者保存的使能状态；此机制不代替多核锁。 */
#define local_intr_restore(x) __intr_restore(x);

#endif /* !__KERN_SYNC_SYNC_H__ */
