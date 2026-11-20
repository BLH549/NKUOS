/* Included by verify.sh in a temporary copy of proc.c. */
enum b_failure
{
    B_TRACK,
    B_FAIL_PCB,
    B_FAIL_STACK,
};

static bool b_tracking;
static enum b_failure b_failure;
static void *b_pcb;
static struct Page *b_stack;
static unsigned int b_pcb_allocs, b_pcb_frees, b_stack_allocs, b_stack_frees;
static struct proc_struct *b_second;
static const int b_arg1 = 2413575, b_arg2 = 20261120;

void *__real_kmalloc(size_t);
void __real_kfree(void *);
struct Page *__real_alloc_pages(size_t);
void __real_free_pages(struct Page *, size_t);

void *
__wrap_kmalloc(size_t size)
{
    if (!b_tracking || size != sizeof(struct proc_struct))
    {
        return __real_kmalloc(size);
    }
    b_pcb_allocs++;
    if (b_failure == B_FAIL_PCB)
    {
        return NULL;
    }
    return b_pcb = __real_kmalloc(size);
}

void
__wrap_kfree(void *ptr)
{
    if (b_tracking && b_pcb != NULL && ptr == b_pcb)
    {
        b_pcb_frees++;
    }
    __real_kfree(ptr);
}

struct Page *
__wrap_alloc_pages(size_t n)
{
    if (!b_tracking || n != KSTACKPAGE)
    {
        return __real_alloc_pages(n);
    }
    b_stack_allocs++;
    if (b_failure == B_FAIL_STACK)
    {
        return NULL;
    }
    return b_stack = __real_alloc_pages(n);
}

void
__wrap_free_pages(struct Page *page, size_t n)
{
    if (b_tracking && b_stack != NULL && page == b_stack && n == KSTACKPAGE)
    {
        b_stack_frees++;
    }
    __real_free_pages(page, n);
}

static size_t
b_registry_size(void)
{
    size_t count = 0, hashed = 0;
    for (list_entry_t *le = list_next(&proc_list); le != &proc_list; le = list_next(le))
    {
        assert(list_prev(list_next(le)) == le);
        struct proc_struct *proc = le2proc(le, list_link);
        assert(find_proc(proc->pid) == proc);
        assert(++count < MAX_PROCESS);
    }
    for (size_t i = 0; i < HASH_LIST_SIZE; i++)
    {
        list_entry_t *head = &hash_list[i];
        for (list_entry_t *le = list_next(head); le != head; le = list_next(le))
        {
            assert(list_prev(list_next(le)) == le);
            assert(pid_hashfn(le2proc(le, hash_link)->pid) == i);
            assert(++hashed <= count);
        }
    }
    assert(hashed == count);
    return count;
}

static void
b_test_failure(const char *name, enum b_failure failure, int expected,
               unsigned int pcb_allocs, unsigned int pcb_frees,
               unsigned int stack_allocs, unsigned int stack_frees)
{
    int count_before = nr_process;
    size_t pages_before = nr_free_pages();
    size_t links_before = b_registry_size();
    struct proc_struct *parent = current;

    b_failure = failure;
    b_pcb = NULL;
    b_stack = NULL;
    b_pcb_allocs = b_pcb_frees = b_stack_allocs = b_stack_frees = 0;
    b_tracking = 1;
    int ret = kernel_thread(init_main, NULL, 0);
    b_tracking = 0;

    assert(ret == expected && current == parent && nr_process == count_before);
    assert(b_registry_size() == links_before && find_proc(2) == NULL);
    assert(nr_free_pages() == pages_before);
    assert(b_pcb_allocs == pcb_allocs && b_pcb_frees == pcb_frees);
    assert(b_stack_allocs == stack_allocs && b_stack_frees == stack_frees);
    cprintf("[B rollback] %s PASS ret=%d pcb=%u/%u stack=%u/%u pages_delta=0 count_delta=0\n",
            name, ret, b_pcb_allocs, b_pcb_frees, b_stack_allocs, b_stack_frees);
}

static void
b_check_frame(struct proc_struct *proc, int (*fn)(void *), const void *arg)
{
    uintptr_t top = proc->kstack + KSTACKSIZE;
    assert(proc->state == PROC_RUNNABLE && proc->parent == idleproc && proc->mm == NULL);
    assert(proc->pgdir == boot_pgdir_pa && proc->kstack != (uintptr_t)bootstack);
    assert((proc->kstack % PGSIZE) == 0 && proc->kstack != 0);
    assert((uintptr_t)(proc->tf + 1) == top && ((uintptr_t)proc->tf % 16) == 0);
    assert(proc->context.ra == (uintptr_t)forkret && proc->context.sp == (uintptr_t)proc->tf);
    assert(proc->tf->gpr.a0 == 0 && proc->tf->gpr.sp == top);
    assert(proc->tf->gpr.s0 == (uintptr_t)fn && proc->tf->gpr.s1 == (uintptr_t)arg);
    assert(proc->tf->epc == (uintptr_t)kernel_thread_entry);
    assert((proc->tf->status & (SSTATUS_SPP | SSTATUS_SPIE | SSTATUS_SIE)) ==
           (SSTATUS_SPP | SSTATUS_SPIE));
    cprintf("[B frame] pid=%d bytes=%u kstack=%p tf=%p sp=%p status=0x%lx\n",
            proc->pid, (unsigned int)sizeof(*proc->tf), (void *)proc->kstack,
            proc->tf, (void *)proc->tf->gpr.sp, proc->tf->status);
}

static __attribute__((noreturn, noinline)) void
b_enter_first_context(struct proc_struct *proc)
{
    intr_disable();
    current = proc;
    asm volatile("mv sp, %0\n\tjr %1"
                 :: "r"(proc->context.sp), "r"(proc->context.ra) : "memory");
    __builtin_unreachable();
}

static int
b_worker(void *arg)
{
    bool first = current->pid == 2;
    assert(first || current == b_second);
    assert(arg == (first ? &b_arg1 : &b_arg2));
    assert(read_csr(sstatus) & SSTATUS_SIE);
    cprintf("[B first-run] pid=%d arg=%d SIE=1 PASS\n", current->pid, *(const int *)arg);
    if (first)
    {
        b_enter_first_context(b_second);
    }

    cprintf("[B result] PASS\n");
    sbi_shutdown();
    while (1)
    {
        asm volatile("wfi");
    }
}

void
test_lab4_b_run(void)
{
    struct proc_struct *fresh = alloc_proc();
    assert(fresh != NULL && fresh->state == PROC_UNINIT && fresh->pid == -1);
    assert(fresh->kstack == 0 && fresh->tf == NULL && fresh->pgdir == boot_pgdir_pa);
    assert(fresh->runs == 0 && !fresh->need_resched && fresh->parent == NULL);
    assert(fresh->mm == NULL && fresh->flags == 0);
    struct context zero_context = {0};
    const char zero_name[PROC_NAME_LEN + 1] = {0};
    assert(memcmp(&fresh->context, &zero_context, sizeof(zero_context)) == 0);
    assert(memcmp(fresh->name, zero_name, sizeof(zero_name)) == 0);
    assert(fresh->list_link.prev == &fresh->list_link && list_empty(&fresh->list_link));
    assert(fresh->hash_link.prev == &fresh->hash_link && list_empty(&fresh->hash_link));
    kfree(fresh);

    assert(nr_process == 2 && b_registry_size() == 1 && find_proc(0) == NULL);
    assert(find_proc(1) == initproc && idleproc == current && idleproc->need_resched);
    assert(idleproc->pgdir == initproc->pgdir && strcmp(idleproc->name, "idle") == 0);
    assert(strcmp(initproc->name, "init") == 0);
    assert(strcmp((const char *)initproc->tf->gpr.s1, "Hello world!!") == 0);
    cprintf("[B PCB] alloc_proc, idle (pid=0), init (pid=1) PASS\n");
    b_check_frame(initproc, init_main, (const void *)initproc->tf->gpr.s1);

    b_test_failure("PCB", B_FAIL_PCB, -E_NO_MEM, 1, 0, 0, 0);
    b_test_failure("kernel stack", B_FAIL_STACK, -E_NO_MEM, 1, 1, 1, 0);
    struct mm_struct unsupported_mm = {0};
    current->mm = &unsupported_mm;
    b_test_failure("copy_mm", B_TRACK, -E_INVAL, 1, 1, 1, 1);
    current->mm = NULL;
    nr_process = MAX_PROCESS;
    b_test_failure("process limit", B_TRACK, -E_NO_FREE_PROC, 0, 0, 0, 0);
    nr_process = 2;

    assert((read_csr(sstatus) & SSTATUS_SIE) == 0);
    assert(kernel_thread(b_worker, (void *)&b_arg1, 0) == 2);
    assert((read_csr(sstatus) & SSTATUS_SIE) == 0);
    intr_enable();
    assert(kernel_thread(b_worker, (void *)&b_arg2, 0) == 3);
    assert(read_csr(sstatus) & SSTATUS_SIE);
    intr_disable();
    struct proc_struct *first = find_proc(2);
    b_second = find_proc(3);
    assert(first != NULL && b_second != NULL && first->kstack != b_second->kstack);
    b_check_frame(first, b_worker, &b_arg1);
    b_check_frame(b_second, b_worker, &b_arg2);

    struct trapframe original = {0};
    original.gpr.a0 = 42;
    original.gpr.t3 = 57;
    uintptr_t explicit_sp = 0x10000;
    assert(do_fork(0, explicit_sp, &original) == 4);
    struct proc_struct *child = find_proc(4);
    assert(child != NULL && child->tf != &original && original.gpr.a0 == 42);
    assert(child->tf->gpr.sp == explicit_sp && child->tf->gpr.a0 == 0);
    assert(child->tf->gpr.t3 == original.gpr.t3);
    assert(nr_process == 5 && b_registry_size() == 4);
    cprintf("[B create] PIDs 2/3/4, PID hash, explicit SP and SIE restoration PASS\n");

    b_enter_first_context(first);
}
