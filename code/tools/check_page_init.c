/* Run the real static page_init with a host buffer replacing the direct map.
 * The DTB getters and allocator registration are the only mocked interfaces. */
#include <pmm.h>
#include <stdio.h>

char test_kernel_end[2 * 1024 * 1024] __attribute__((aligned(PGSIZE)));
#define TEST_END_PA (DRAM_BASE + 0x201000)
#undef PHYSICAL_MEMORY_OFFSET
#define PHYSICAL_MEMORY_OFFSET ((uintptr_t)test_kernel_end - TEST_END_PA)
#undef KERNBASE
#define KERNBASE (KERNEL_BEGIN_PADDR + PHYSICAL_MEMORY_OFFSET)
#define end test_kernel_end
#include "../kern/mm/pmm.c"
#undef end

static uint64_t test_base = DRAM_BASE, test_size = 128 * 1024 * 1024;
static size_t calls, registered;
static struct Page *registered_base;
char boot_page_table_sv39[PGSIZE];
uint64_t get_memory_base(void) { return test_base; }
uint64_t get_memory_size(void) { return test_size; }
static void register_pages(struct Page *base, size_t n) {
    calls++;
    registered = n;
    registered_base = base;
    assert(n > 0);
    for (size_t i = 0; i < n; i++) {
        assert(PageReserved(base + i));
    }
}
const struct pmm_manager best_fit_pmm_manager = {.init_memmap = register_pages};
const struct pmm_manager default_pmm_manager = {.init_memmap = register_pages};

int main(int argc, char **argv) {
    char mode = argc > 1 ? argv[1][0] : 'v';
    if (mode == 'z') { test_size = 0; }
    if (mode == 'b') { test_base += PGSIZE; }
    if (mode == 'o') { test_size = (uint64_t)-1; }
    if (mode == 'k') { test_size = 0x100000; }
    if (mode == 'd') { test_size = 0x201000; }
    if (mode == 's') { test_size = 16 * 1024 * 1024 + 123; }
    if (mode == 'c') { test_size = 256 * 1024 * 1024; }
    if (mode == 'e') {
        // Find the first bound which just fits the kernel and descriptors.
        for (test_size = 0x201000; ; test_size += PGSIZE) {
            size_t count = test_size / PGSIZE;
            uintptr_t limit = DRAM_BASE + test_size;
            if (ROUNDUP(DRAM_BASE + 0x201000 + count * sizeof(struct Page),
                        PGSIZE) == limit) { break; }
        }
    }
    pmm_manager = &best_fit_pmm_manager;
    page_init();
    assert(mode == 'v' || mode == 's' || mode == 'c' || mode == 'e');
    uintptr_t limit = ROUNDDOWN(DRAM_BASE + test_size, PGSIZE);
    if (limit > PHYSICAL_MEMORY_END) { limit = PHYSICAL_MEMORY_END; }
    assert(va_pa_offset == PHYSICAL_MEMORY_OFFSET);
    assert(npage == limit / PGSIZE);
    assert((uintptr_t)pages == ROUNDUP((uintptr_t)test_kernel_end, PGSIZE));
    uintptr_t free_pa = ROUNDUP(DRAM_BASE + 0x201000 +
                               (npage - nbase) * sizeof(struct Page), PGSIZE);
    assert(registered == (limit - free_pa) / PGSIZE);
    assert(calls == (free_pa < limit));
    if (calls) { assert(page2pa(registered_base) == free_pa); }
    for (size_t i = 0; i < npage - nbase; i++) {
        assert(PageReserved(pages + i));
        assert(pages[i].ref == 0 && pages[i].property == 0);
        assert(list_empty(&pages[i].page_link));
    }
    cprintf("page_init host case %c: PASS\n", mode);
    return 0;
}
