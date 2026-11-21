#include <list.h>
#include <sync.h>
#include <proc.h>
#include <sched.h>
#include <assert.h>

void
wakeup_proc(struct proc_struct *proc) {
    assert(proc->state != PROC_ZOMBIE && proc->state != PROC_RUNNABLE);
    proc->state = PROC_RUNNABLE;
}

//LAB4 function3 2411238
void
schedule(void) {
    bool intr_flag;
    local_intr_save(intr_flag);

    current->need_resched = 0;
    list_entry_t *head = &proc_list;
    list_entry_t *start = current == idleproc ? head : &current->list_link;
    list_entry_t *entry = start;
    struct proc_struct *next = idleproc;

    // Walk one full cycle, including the current thread on wraparound.
    do {
        entry = list_next(entry);
        if (entry != head) {
            struct proc_struct *proc = le2proc(entry, list_link);
            if (proc->state == PROC_RUNNABLE) {
                next = proc;
                break;
            }
        }
    } while (entry != start);

    next->runs++;
    if (next != current) {
        proc_run(next);
    }
    local_intr_restore(intr_flag);
}
