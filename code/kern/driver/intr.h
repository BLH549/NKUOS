#ifndef __KERN_DRIVER_INTR_H__
#define __KERN_DRIVER_INTR_H__

/* 控制当前 hart 的 S 态全局中断使能位。 */
void intr_enable(void);
/* 清除该使能位；不改变各中断源的 sie 配置。 */
void intr_disable(void);

#endif /* !__KERN_DRIVER_INTR_H__ */
