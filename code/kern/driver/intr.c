#include <intr.h>
#include <riscv.h>

/* 置 sstatus.SIE，允许当前 S 态接收已单独使能的中断。 */
void intr_enable(void) { set_csr(sstatus, SSTATUS_SIE); }

/* 清 sstatus.SIE，屏蔽当前 S 态的可屏蔽中断。 */
void intr_disable(void) { clear_csr(sstatus, SSTATUS_SIE); }
