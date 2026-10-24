#ifndef __KERN_MM_SLUB_H__
#define __KERN_MM_SLUB_H__

#include <defs.h>

#define SLUB_ALIGNMENT 16
#define SLUB_MAX_SIZE 2048

void slub_init(void);
void *slub_alloc(size_t size);
/* NULL is a successful no-op; invalid or already freed slots return false. */
bool slub_free(void *object);
void slub_check(void);

#endif
