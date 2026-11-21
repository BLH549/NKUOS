#include <assert.h>
#include <clock.h>
#include <intr.h>
#include <pmm.h>
#include <proc.h>
#include <riscv.h>
#include <sbi.h>
#include <sched.h>
#include <stdio.h>
#include <string.h>
#include <sync.h>

#define ROUNDS 8
#define PROBE_VA 0x400000UL

static struct worker {
    struct proc_struct *proc;
    uintptr_t value;
} workers[2];

static unsigned int turn;

void check_saved_registers(uintptr_t seed);
void check_exception_return(void);

void
fixture_bad_register(void)
{
    panic("C fixture: saved register changed");
}

static bool
interrupts_enabled(void)
{
    return (read_csr(sstatus) & SSTATUS_SIE) != 0;
}

static void
check_cpu(struct proc_struct *proc)
{
    uintptr_t sp;
    asm volatile("mv %0, sp" : "=r"(sp));
    assert(current == proc);
    assert(sp >= proc->kstack && sp < proc->kstack + KSTACKSIZE);
    assert((sp & 15) == 0);
    assert(read_csr(satp) == (0x8000000000000000UL | (proc->pgdir >> 12)));
}

static void
print_cpu(const char *stage, struct proc_struct *proc)
{
    uintptr_t sp;
    asm volatile("mv %0, sp" : "=r"(sp));
    cprintf("[C %s] pid=%d runs=%d pgdir=0x%016lx satp=0x%016lx sp=0x%016lx sie=%d\n",
            stage, proc->pid, proc->runs, proc->pgdir, read_csr(satp), sp,
            interrupts_enabled());
    cprintf("[C context] kstack=0x%016lx ra=0x%016lx sp=0x%016lx s0=0x%016lx s11=0x%016lx\n",
            proc->kstack, proc->context.ra, proc->context.sp,
            proc->context.s0, proc->context.s11);
}

static int
single_worker(void *arg)
{
    struct proc_struct *self = *(struct proc_struct **)arg;
    struct context initial = self->context;
    check_cpu(self);
    assert(interrupts_enabled());
    self->need_resched = 1;
    schedule();
    assert(self->runs == 1 && !self->need_resched && interrupts_enabled());
    check_cpu(self);
    assert(memcmp(&initial, &self->context, sizeof(initial)) == 0);

    self->state = PROC_SLEEPING;
    intr_disable();
    schedule();
    panic("single worker should remain asleep");
}

static int
round_robin_worker(void *arg)
{
    const struct worker *worker = arg;
    const unsigned int index = worker - workers;
    struct proc_struct *self = worker->proc;
    volatile uintptr_t stack_value = worker->value;

    check_cpu(self);
    assert(interrupts_enabled());
    print_cpu("first entry", self);

    for (unsigned int i = 0; i < ROUNDS; i++) {
        bool enable = ((i + index) & 1) == 0;
        if (enable) {
            intr_enable();
        } else {
            intr_disable();
        }

        assert(turn == i * 2 + 1 - index);
        turn++;
        assert(self->runs == (int)i + 1);
        assert(*(volatile uintptr_t *)PROBE_VA == worker->value);
        self->need_resched = 1;
        check_saved_registers(worker->value + i * 16);
        assert(interrupts_enabled() == enable);
        assert(!self->need_resched && stack_value == worker->value);
        assert(*(volatile uintptr_t *)PROBE_VA == worker->value);
        check_cpu(self);
        if (i == 0 || i == ROUNDS - 1) {
            print_cpu("resumed", self);
        }
    }

    self->state = PROC_SLEEPING;
    schedule();
    panic("round-robin worker should remain asleep");
}

static void
create_worker(struct worker *worker, uintptr_t value)
{
    struct Page *root = alloc_page();
    struct Page *data = alloc_page();
    assert(root != NULL && data != NULL);
    pde_t *pgdir = page2kva(root);
    memcpy(pgdir, boot_pgdir_va, PGSIZE);
    assert(pgdir[PDX1(PROBE_VA)] == 0);
    *(uintptr_t *)page2kva(data) = value;
    assert(page_insert(pgdir, data, PROBE_VA, PTE_R) == 0);

    worker->value = value;
    int pid = kernel_thread(round_robin_worker, worker, 0);
    assert(pid > 0);
    worker->proc = find_proc(pid);
    assert(worker->proc != NULL);
    worker->proc->pgdir = page2pa(root);
}

void
test_c_schedule(void)
{
    bool outer, inner;
    assert(interrupts_enabled());
    local_intr_save(outer);
    local_intr_save(inner);
    assert(outer && !inner && !interrupts_enabled());
    local_intr_restore(inner);
    assert(!interrupts_enabled());
    local_intr_restore(outer);
    assert(interrupts_enabled());

    check_cpu(idleproc);
    initproc->state = PROC_SLEEPING;
    list_del_init(&initproc->list_link);
    idleproc->need_resched = 1;
    schedule();
    assert(idleproc->runs == 1 && !idleproc->need_resched && interrupts_enabled());
    list_add(&proc_list, &initproc->list_link);

    intr_disable();
    idleproc->need_resched = 1;
    schedule();
    assert(idleproc->runs == 2 && !idleproc->need_resched && !interrupts_enabled());
    check_cpu(idleproc);
    intr_enable();

    struct proc_struct *single;
    int pid = kernel_thread(single_worker, &single, 0);
    assert(pid > 0);
    single = find_proc(pid);
    assert(single != NULL);
    proc_run(single);
    check_cpu(idleproc);
    assert(interrupts_enabled() && single->state == PROC_SLEEPING);
    assert(idleproc->runs == 3);
    cprintf("[C scheduler] empty, all sleeping, self-selection and direct proc_run PASS\n");

    create_worker(&workers[0], 0x1100000011000000UL);
    create_worker(&workers[1], 0x2200000022000000UL);
    assert(workers[0].proc->pgdir != workers[1].proc->pgdir);
    assert(workers[0].proc->pgdir != idleproc->pgdir);
    assert(workers[1].proc->pgdir != idleproc->pgdir);
    intr_disable();
    idleproc->need_resched = 1;
    schedule();
    check_cpu(idleproc);
    assert(!interrupts_enabled() && !idleproc->need_resched);
    assert(turn == 2 * ROUNDS && idleproc->runs == 4);
    assert(workers[0].proc->runs == ROUNDS + 1 && workers[1].proc->runs == ROUNDS + 1);
    assert(workers[0].proc->state == PROC_SLEEPING && workers[1].proc->state == PROC_SLEEPING);
    print_cpu("back to idle", idleproc);
    cprintf("[C scheduler] %u ordered yields, s0-s11, stack, SIE and Sv39 aliases PASS\n", turn);

    check_exception_return();
    assert(!interrupts_enabled());
    intr_enable();
    size_t start = ticks;
    while (ticks - start < 101) {
        asm volatile("wfi");
    }
    assert(interrupts_enabled());
    check_cpu(idleproc);
    cprintf("[C trap] mixed-width exceptions and %lu timer interrupts returned PASS\n", ticks - start);
    cprintf("[C result] PASS\n");
    sbi_shutdown();
}
