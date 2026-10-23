/* Host-side checks against the actual allocator implementations. */
#include <pmm.h>
#include <best_fit_pmm.h>
#include <default_pmm.h>
#include <stdio.h>

#define NPAGES 256
static struct Page storage[NPAGES];
struct Page *pages = storage;
size_t npage;
const size_t nbase = DRAM_BASE / PGSIZE;
uint64_t va_pa_offset;
const struct pmm_manager *pmm_manager;

struct Page *alloc_pages(size_t n) { return pmm_manager->alloc_pages(n); }
void free_pages(struct Page *p, size_t n) { pmm_manager->free_pages(p, n); }
size_t nr_free_pages(void) { return pmm_manager->nr_free_pages(); }

static unsigned int rng = 2412235;
static unsigned int next_random(void) {
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return rng;
}

static void reset(void) {
    pmm_manager->init();
    for (size_t i = 0; i < NPAGES; i++) {
        storage[i].flags = 0;
        SetPageReserved(&storage[i]);
        storage[i].property = 0;
        storage[i].ref = 0;
    }
}

int main(int argc, char **argv) {
    bool best_fit = argc == 1 || argv[1][0] != 'f';
    pmm_manager = best_fit ? &best_fit_pmm_manager : &default_pmm_manager;
    npage = nbase + NPAGES;
    reset();
    pmm_manager->init_memmap(storage, NPAGES);
    pmm_manager->check();
    assert(nr_free_pages() == NPAGES);

    // Six free pages in two separated three-page runs cannot satisfy four.
    reset();
    pmm_manager->init_memmap(storage, 12);
    assert(alloc_pages(12) == storage);
    free_pages(storage, 3);
    free_pages(storage + 6, 3);
    assert(nr_free_pages() == 6 && alloc_pages(4) == NULL);
    assert(nr_free_pages() == 6);
    free_pages(storage + 3, 3);
    free_pages(storage + 9, 3);
    assert(alloc_pages(12) == storage);
    free_pages(storage, 12);

    reset();
    // Out-of-order RAM registration with a reserved hole in the middle.
    pmm_manager->init_memmap(storage + 112, NPAGES - 112);
    pmm_manager->init_memmap(storage, 48);
    if (best_fit) {
        pmm_manager->init_memmap(storage + 48, 48);
        assert(storage[0].property == 96); // adjacent init ranges coalesce
    } else {
        // First-Fit's original init_memmap does not coalesce adjacent banks.
        reset();
        pmm_manager->init_memmap(storage + 112, NPAGES - 112);
        pmm_manager->init_memmap(storage, 96);
    }

    unsigned int used[NPAGES] = {0};
    size_t lengths[NPAGES] = {0};
    for (size_t i = 96; i < 112; i++) {
        used[i] = 1;
    }
    for (size_t step = 0; step < 10000; step++) {
        size_t index = next_random() % NPAGES;
        if ((next_random() & 1) && lengths[index] != 0) {
            size_t n = lengths[index];
            free_pages(storage + index, n);
            for (size_t i = index; i < index + n; i++) {
                assert(used[i]);
                used[i] = 0;
            }
            lengths[index] = 0;
        } else {
            size_t n = next_random() % 32 + 1;
            size_t expected = NPAGES, best_size = NPAGES + 1;
            for (size_t i = 0; i < NPAGES;) {
                if (used[i]) { i++; continue; }
                size_t start = i;
                while (i < NPAGES && !used[i]) { i++; }
                size_t run = i - start;
                if (run >= n && run < best_size) {
                    expected = start;
                    best_size = run;
                    if (!best_fit) { break; }
                }
            }
            struct Page *p = alloc_pages(n);
            if (expected == NPAGES) {
                assert(p == NULL);
            } else {
                assert(p == storage + expected);
                lengths[expected] = n;
                for (size_t i = expected; i < expected + n; i++) {
                    assert(!used[i] && !PageReserved(&storage[i]));
                    used[i] = 1;
                }
            }
        }
        size_t free_count = 0;
        for (size_t i = 0; i < NPAGES; i++) {
            free_count += !used[i];
            assert(PageReserved(&storage[i]) == (i >= 96 && i < 112));
        }
        assert(nr_free_pages() == free_count);
    }
    for (size_t i = 0; i < NPAGES; i++) {
        if (lengths[i]) { free_pages(storage + i, lengths[i]); }
    }
    assert(nr_free_pages() == 240);
    assert(alloc_pages(145) == NULL);
    assert(alloc_pages(144) == storage + 112);
    assert(alloc_pages(96) == storage);
    assert(alloc_page() == NULL && nr_free_pages() == 0);
    free_pages(storage + 112, 144);
    free_pages(storage, 96);
    assert(nr_free_pages() == 240);
    cprintf("%s: framework checks + 10000 bitmap-model operations PASS\n",
            pmm_manager->name);
    return 0;
}
