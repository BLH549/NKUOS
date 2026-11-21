/* Installed only in the temporary kernel built by check_a.py. */
#include <assert.h>
#include <error.h>
#include <intr.h>
#include <pmm.h>
#include <proc.h>
#include <riscv.h>
#include <sbi.h>
#include <stdio.h>
#include <string.h>
#include <trap.h>

static const struct pmm_manager *real_manager;
static struct pmm_manager failing_manager;
static unsigned int allocation_calls, fail_at;
static volatile unsigned int expected_fault, fault_count;
static uintptr_t fault_address;

extern void a_store(uintptr_t address, uintptr_t value);
extern uintptr_t a_load(uintptr_t address);
void __real_trap(struct trapframe *tf);

void __wrap_trap(struct trapframe *tf)
{
    if (expected_fault != 0)
    {
        assert(tf->cause == expected_fault && tf->badvaddr == fault_address);
        ++fault_count;
        tf->epc += 4;
        return;
    }
    __real_trap(tf);
}

static struct Page *failing_alloc(size_t n)
{
    if (++allocation_calls == fail_at)
    {
        return NULL;
    }
    return real_manager->alloc_pages(n);
}

static void fail_allocation(unsigned int call)
{
    real_manager = pmm_manager;
    failing_manager = *real_manager;
    failing_manager.alloc_pages = failing_alloc;
    allocation_calls = 0;
    fail_at = call;
    pmm_manager = &failing_manager;
}

static void restore_allocator(void)
{
    pmm_manager = real_manager;
}

static pde_t *new_directory(void)
{
    struct Page *page = alloc_page();
    assert(page != NULL);
    set_page_ref(page, 1);
    pde_t *directory = page2kva(page);
    memset(directory, 0, PGSIZE);
    return directory;
}

static void release_branch(pde_t *directory, size_t index)
{
    if (!(directory[index] & PTE_V))
    {
        return;
    }
    pde_t *middle = KADDR(PDE_ADDR(directory[index]));
    for (size_t i = 0; i < NPDEENTRY; ++i)
    {
        if (middle[i] & PTE_V)
        {
            pte_t *leaf = KADDR(PDE_ADDR(middle[i]));
            for (size_t j = 0; j < NPTEENTRY; ++j)
            {
                assert((leaf[j] & PTE_V) == 0);
            }
            struct Page *page = pde2page(middle[i]);
            assert(page_ref(page) == 1);
            set_page_ref(page, 0);
            free_page(page);
        }
    }
    struct Page *page = pde2page(directory[index]);
    assert(page_ref(page) == 1);
    directory[index] = 0;
    flush_tlb();
    set_page_ref(page, 0);
    free_page(page);
}

static void release_directory(pde_t *directory)
{
    for (size_t i = 0; i < NPDEENTRY; ++i)
    {
        release_branch(directory, i);
    }
    struct Page *page = kva2page(directory);
    set_page_ref(page, 0);
    free_page(page);
}

static void mapping_cases(void)
{
    size_t initial_free = nr_free_pages();
    pde_t *directory = new_directory();
    size_t empty_free = nr_free_pages();
    page_remove(directory, 0);
    page_remove(directory, 5 * PTSIZE + 123);
    assert(nr_free_pages() == empty_free && directory[0] == 0);

    struct Page *first = alloc_page(), *second = alloc_page();
    assert(first != NULL && second != NULL);
    assert(page_insert(directory, first, 123, PTE_R | PTE_W) == 0);
    assert(page_ref(first) == 1 && get_page(directory, 0, NULL) == first);
    size_t mapped_free = nr_free_pages();
    assert(page_insert(directory, first, 0, PTE_R | PTE_U) == 0);
    assert(page_ref(first) == 1 && nr_free_pages() == mapped_free);
    pte_t *entry = get_pte(directory, 0, 0);
    assert((*entry & 0x3ff) == (PTE_V | PTE_R | PTE_U));
    assert(page_insert(directory, first, PGSIZE + 19, PTE_R | PTE_W) == 0);
    assert(page_ref(first) == 2);
    assert(page_insert(directory, second, 0, PTE_R | PTE_W) == 0);
    assert(page_ref(first) == 1 && page_ref(second) == 1);
    page_remove(directory, PGSIZE + 40);
    assert(page_ref(first) == 0 && nr_free_pages() == mapped_free + 1);
    page_remove(directory, 1);
    assert(page_ref(second) == 0 && nr_free_pages() == mapped_free + 2);
    page_remove(directory, 1);
    assert(nr_free_pages() == mapped_free + 2);
    assert(directory[0] & PTE_V);
    release_directory(directory);
    assert(nr_free_pages() == initial_free);
    cprintf("[A mapping] first/same-page/permissions/replace/aliases/unaligned/remove PASS\n");
}

static void allocation_failures(void)
{
    size_t initial_free = nr_free_pages();
    pde_t *directory = new_directory();
    struct Page *page = alloc_page();
    assert(page != NULL && page_ref(page) == 0);
    size_t before = nr_free_pages();

    fail_allocation(1);
    assert(page_insert(directory, page, 0, PTE_R) == -E_NO_MEM);
    restore_allocator();
    assert(directory[0] == 0 && page_ref(page) == 0 && nr_free_pages() == before);
    cprintf("[A failure] first table allocation: ret=-4 refs=0 tables=0 PASS\n");

    fail_allocation(2);
    assert(page_insert(directory, page, 0, PTE_R) == -E_NO_MEM);
    restore_allocator();
    pde_t retained = directory[0];
    assert((retained & PTE_V) && page_ref(page) == 0);
    assert(get_pte(directory, 0, 0) == NULL && nr_free_pages() == before - 1);
    cprintf("[A failure] second table allocation: ret=-4 refs=0 retained_tables=1 PASS\n");

    assert(page_insert(directory, page, 0, PTE_R | PTE_W) == 0);
    assert(directory[0] == retained && page_ref(page) == 1);
    assert(nr_free_pages() == before - 2);
    pte_t saved = *get_pte(directory, 0, 0);
    struct Page *other = alloc_page();
    assert(other != NULL);
    size_t retry_free = nr_free_pages();
    fail_allocation(1);
    assert(page_insert(directory, other, PTSIZE, PTE_R) == -E_NO_MEM);
    assert(allocation_calls == 1);
    restore_allocator();
    assert(page_ref(other) == 0 && page_ref(page) == 1);
    assert(*get_pte(directory, 0, 0) == saved && nr_free_pages() == retry_free);

    fail_allocation(1);
    assert(page_insert(directory, page, 0, PTE_R) == 0);
    assert(allocation_calls == 0);
    restore_allocator();
    assert(page_ref(page) == 1);
    free_page(other);
    page_remove(directory, 0);
    release_directory(directory);
    assert(nr_free_pages() == initial_free);
    cprintf("[A failure] retry/existing mapping/permission-only update/cleanup PASS\n");
}

static void hardware_translation(void)
{
    const uintptr_t address = 0x40000100;
    const size_t index = PDX1(address);
    assert(boot_pgdir_va[index] == 0);
    size_t initial_free = nr_free_pages();
    struct Page *first = alloc_page(), *second = alloc_page();
    assert(first != NULL && second != NULL);
    set_page_ref(first, 1);
    set_page_ref(second, 1);
    *(uintptr_t *)(page2kva(second) + 0x100) = 0x5678;
    assert(page_insert(boot_pgdir_va, first, address, PTE_R | PTE_W) == 0);
    assert(page_insert(boot_pgdir_va, first, address + PGSIZE, PTE_R | PTE_W) == 0);
    a_store(address, 0x1234);
    assert(a_load(address + PGSIZE) == 0x1234);
    assert(page_ref(first) == 3);
    assert(page_insert(boot_pgdir_va, second, address, PTE_R | PTE_W) == 0);
    assert(a_load(address) == 0x5678 && a_load(address + PGSIZE) == 0x1234);
    a_store(address, 0x789a);

    assert(page_insert(boot_pgdir_va, second, address, PTE_R) == 0);
    expected_fault = CAUSE_STORE_PAGE_FAULT;
    fault_address = address;
    fault_count = 0;
    a_store(address, 0xdead);
    assert(fault_count == 1 && *(uintptr_t *)(page2kva(second) + 0x100) == 0x789a);
    expected_fault = 0;
    assert(page_insert(boot_pgdir_va, second, address, PTE_R | PTE_W) == 0);
    a_store(address, 0xabcd);
    assert(a_load(address) == 0xabcd);

    page_remove(boot_pgdir_va, address);
    expected_fault = CAUSE_LOAD_PAGE_FAULT;
    fault_count = 0;
    a_load(address);
    assert(fault_count == 1);
    expected_fault = 0;
    page_remove(boot_pgdir_va, address + PGSIZE);
    assert(page_ref(first) == 1 && page_ref(second) == 1);
    set_page_ref(first, 0);
    set_page_ref(second, 0);
    free_page(first);
    free_page(second);
    release_branch(boot_pgdir_va, index);
    assert(nr_free_pages() == initial_free);
    cprintf("[A hardware] alias/remap/RO fault/RW restore/unmapped fault/TLB PASS\n");
}

static unsigned int random_state = 0x2412235;
static unsigned int next_random(void)
{
    random_state ^= random_state << 13;
    random_state ^= random_state >> 17;
    random_state ^= random_state << 5;
    return random_state;
}

static void model_operations(void)
{
    enum { PAGE_COUNT = 16, SLOTS = 32, OPERATIONS = 4096 };
    size_t initial_free = nr_free_pages();
    pde_t *directory = new_directory();
    struct Page *pool[PAGE_COUNT];
    int mapping[SLOTS], refs[PAGE_COUNT] = {0};
    uint32_t permissions[SLOTS] = {0};
    for (size_t i = 0; i < PAGE_COUNT; ++i)
    {
        assert((pool[i] = alloc_page()) != NULL);
        set_page_ref(pool[i], 1);
    }
    for (size_t i = 0; i < SLOTS; ++i)
    {
        mapping[i] = -1;
    }
    for (size_t step = 0; step < OPERATIONS; ++step)
    {
        unsigned int value = next_random();
        size_t slot = (value >> 8) % SLOTS;
        if ((value & 3) != 0)
        {
            size_t selected = (value >> 16) % PAGE_COUNT;
            uint32_t perm = PTE_R | ((value & 4) ? PTE_W : 0) | ((value & 8) ? PTE_U : 0);
            assert(page_insert(directory, pool[selected], slot * PGSIZE, perm) == 0);
            if (mapping[slot] >= 0) --refs[mapping[slot]];
            ++refs[selected];
            mapping[slot] = selected;
            permissions[slot] = perm;
        }
        else
        {
            page_remove(directory, slot * PGSIZE);
            if (mapping[slot] >= 0) --refs[mapping[slot]];
            mapping[slot] = -1;
        }
        for (size_t i = 0; i < SLOTS; ++i)
        {
            pte_t *entry = NULL;
            struct Page *actual = get_page(directory, i * PGSIZE, &entry);
            assert(actual == (mapping[i] < 0 ? NULL : pool[mapping[i]]));
            if (mapping[i] >= 0)
            {
                assert((*entry & 0x3ff) == (PTE_V | permissions[i]));
            }
        }
        for (size_t i = 0; i < PAGE_COUNT; ++i)
        {
            assert(page_ref(pool[i]) == refs[i] + 1 && !PageProperty(pool[i]));
        }
    }
    for (size_t i = 0; i < SLOTS; ++i) page_remove(directory, i * PGSIZE);
    for (size_t i = 0; i < PAGE_COUNT; ++i)
    {
        assert(page_ref(pool[i]) == 1);
        set_page_ref(pool[i], 0);
        free_page(pool[i]);
    }
    release_directory(directory);
    assert(nr_free_pages() == initial_free);
    cprintf("[A model] 4096 operations, 32 slots, 16 pages, seed=0x2412235 PASS\n");
}

void __attribute__((noreturn)) __wrap_cpu_idle(void)
{
    intr_disable();
    uintptr_t direct_map = boot_pgdir_va[511];
    cprintf("[A baseline] Page=%u bytes free=%lu npage=%lu SATP=0x%lx\n",
            (unsigned int)sizeof(struct Page), nr_free_pages(), npage, read_csr(satp));
    mapping_cases();
    allocation_failures();
    hardware_translation();
    model_operations();
    assert(boot_pgdir_va[511] == direct_map && current == idleproc && initproc != NULL);
    cprintf("[A result] PASS\n");
    sbi_shutdown();
    while (1) asm volatile("wfi");
}
