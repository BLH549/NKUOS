#ifndef __KERN_DRIVER_CLOCK_H__
#define __KERN_DRIVER_CLOCK_H__

#include <defs.h>

/* 时钟中断累计次数，由中断处理程序递增。 */
extern volatile size_t ticks;

/* 初始化时钟中断源和首次事件。 */
void clock_init(void);
/* 安排下一个一次性定时事件。 */
void clock_set_next_event(void);

#endif /* !__KERN_DRIVER_CLOCK_H__ */
