/* lab2: 2413575. Independent bitmap oracle against actual Buddy/SLUB sources. */
#include <pmm.h>
#include <buddy_pmm.h>
#include <best_fit_pmm.h>
#include <default_pmm.h>
#include <slub.h>
#include <stdio.h>
#include <string.h>

#define NPAGES 256
static struct Page storage[NPAGES];
static unsigned char memory[NPAGES * PGSIZE] __attribute__((aligned(PGSIZE)));
struct Page *pages = storage;
size_t npage = DRAM_BASE / PGSIZE + NPAGES;
const size_t nbase = DRAM_BASE / PGSIZE;
uint64_t va_pa_offset;
const struct pmm_manager *pmm_manager;

struct Page *alloc_pages(size_t n) { return pmm_manager->alloc_pages(n); }
void free_pages(struct Page *p, size_t n) { pmm_manager->free_pages(p, n); }
size_t nr_free_pages(void) { return pmm_manager->nr_free_pages(); }

static unsigned rng = 2413575;
static unsigned next_random(void) {
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return rng;
}

static void reset(void) {
    pmm_manager->init();
    memset(storage, 0, sizeof(storage));
    for (size_t i = 0; i < NPAGES; i++) {
        SetPageReserved(storage + i);
        list_init(&storage[i].page_link);
    }
}

static size_t rounded(size_t n) {
    size_t span = 1;
    while (span < n) {
        span *= 2;
    }
    return span;
}

static void buddy_edges(void) {
    reset();
    pmm_manager->init_memmap(storage + 1, 63);
    struct Page *p = alloc_pages(32);
    assert(p == storage + 32 && nr_free_pages() == 31);
    assert(alloc_pages(32) == NULL && nr_free_pages() == 31);
    free_pages(p, 32);
    struct Page *single[63];
    for (size_t i = 0; i < 63; i++) {
        single[i] = alloc_page();
        assert(single[i] > storage && single[i] < storage + 64);
        for (size_t j = 0; j < i; j++) {
            assert(single[i] != single[j]);
        }
    }
    assert(nr_free_pages() == 0 && alloc_page() == NULL);
    for (size_t i = 63; i > 0; i--) {
        free_page(single[i - 1]);
    }
    assert(nr_free_pages() == 63 && PageReserved(storage));
    assert(PageReserved(storage + 64));
    cprintf("[Buddy host] unaligned 63-page arena/exhaustion: PASS\n");

    reset();
    pmm_manager->init_memmap(storage + 64, 8);
    p = alloc_pages(3);
    struct Page *q = alloc_pages(4);
    assert(p == storage + 64 && q == storage + 68);
    assert(nr_free_pages() == 0);
    free_pages(p, 3);
    assert(alloc_pages(8) == NULL && nr_free_pages() == 4);
    free_pages(q, 4);
    assert(alloc_pages(8) == storage + 64);
    free_pages(storage + 64, 8);

    reset();
    pmm_manager->init_memmap(storage, 16);
    struct Page *four[4];
    for (size_t i = 0; i < 4; i++) {
        four[i] = alloc_pages(4);
        assert(four[i] == storage + i * 4);
    }
    free_pages(four[1], 4);
    free_pages(four[2], 4);
    assert(nr_free_pages() == 8 && alloc_pages(8) == NULL);
    free_pages(four[0], 4);
    free_pages(four[3], 4);
    assert(alloc_pages(16) == storage);
    free_pages(storage, 16);
    cprintf("[Buddy host] rounded release/multi-level merge/non-buddies: PASS\n");

    reset();
    pmm_manager->init_memmap(storage + 8, 8);
    pmm_manager->init_memmap(storage, 8);
    assert(alloc_pages(16) == storage);
    free_pages(storage, 16);
    cprintf("[Buddy host] out-of-order adjacent bank registration: PASS\n");
}

static void buddy_random(void) {
    reset();
    pmm_manager->init_memmap(storage + 112, 143);
    pmm_manager->init_memmap(storage + 1, 95);
    unsigned char used[NPAGES] = {0};
    size_t requests[NPAGES] = {0};
    for (size_t i = 0; i < NPAGES; i++) {
        used[i] = i == 0 || (i >= 96 && i < 112) || i == 255;
    }
    for (size_t step = 0; step < 20000; step++) {
        size_t index = next_random() % NPAGES;
        if ((next_random() & 1) && requests[index]) {
            size_t span = rounded(requests[index]);
            free_pages(storage + index, requests[index]);
            for (size_t i = index; i < index + span; i++) {
                assert(used[i]);
                used[i] = 0;
            }
            requests[index] = 0;
        } else {
            size_t n = next_random() % 33 + 1, span = rounded(n);
            bool possible = 0;
            for (size_t i = 0; i + span <= NPAGES; i++) {
                if ((nbase + i) % span != 0) {
                    continue;
                }
                bool available = 1;
                for (size_t j = i; j < i + span; j++) {
                    if (used[j]) { available = 0; break; }
                }
                possible |= available;
            }
            struct Page *p = alloc_pages(n);
            assert((p != NULL) == possible);
            if (p != NULL) {
                size_t start = p - storage;
                assert(start + span <= NPAGES && (nbase + start) % span == 0);
                requests[start] = n;
                for (size_t i = start; i < start + span; i++) {
                    assert(!used[i] && !PageReserved(storage + i));
                    used[i] = 1;
                }
            }
        }
        size_t count = 0;
        for (size_t i = 0; i < NPAGES; i++) {
            count += !used[i];
            assert(PageReserved(storage + i) ==
                   (i == 0 || (i >= 96 && i < 112) || i == 255));
        }
        assert(nr_free_pages() == count);
    }
    for (size_t i = 0; i < NPAGES; i++) {
        if (requests[i]) { free_pages(storage + i, requests[i]); }
    }
    assert(nr_free_pages() == 238);
    cprintf("[Buddy host] 20000 independent bitmap-model operations: PASS\n");
}

static void verify_object(void *p, size_t n, unsigned char pattern) {
    for (size_t i = 0; i < n; i++) {
        assert(((unsigned char *)p)[i] == pattern);
    }
}

static void slub_random(void) {
    void *objects[64] = {0};
    size_t sizes[64] = {0};
    unsigned char patterns[64] = {0};
    size_t before = nr_free_pages();
    for (size_t step = 0; step < 5000; step++) {
        size_t index = next_random() % 64;
        if (objects[index]) {
            verify_object(objects[index], sizes[index], patterns[index]);
            assert(slub_free(objects[index]));
            objects[index] = NULL;
        } else {
            sizes[index] = next_random() % SLUB_MAX_SIZE + 1;
            patterns[index] = (unsigned char)next_random();
            objects[index] = slub_alloc(sizes[index]);
            assert(objects[index] != NULL);
            for (size_t j = 0; j < 64; j++) {
                if (j != index && objects[j]) {
                    uintptr_t a = (uintptr_t)objects[index], b = (uintptr_t)objects[j];
                    assert(a + sizes[index] <= b || b + sizes[j] <= a);
                }
            }
            memset(objects[index], patterns[index], sizes[index]);
        }
        if (step % 127 == 0) {
            for (size_t j = 0; j < 64; j++) {
                if (objects[j]) { verify_object(objects[j], sizes[j], patterns[j]); }
            }
        }
    }
    for (size_t j = 0; j < 64; j++) {
        if (objects[j]) {
            verify_object(objects[j], sizes[j], patterns[j]);
            assert(slub_free(objects[j]));
        }
    }
    assert(nr_free_pages() == before);
    struct Page *held[NPAGES];
    size_t count = 0;
    while (count < NPAGES && (held[count] = alloc_page()) != NULL) { count++; }
    assert(nr_free_pages() == 0 && alloc_page() == NULL);
    assert(slub_alloc(16) == NULL && slub_alloc(2048) == NULL);
    assert(nr_free_pages() == 0);
    while (count > 0) { free_page(held[--count]); }
    assert(nr_free_pages() == before);
    cprintf("[SLUB host] 5000 object operations/data/overlap/OOM/recovery: PASS\n");
}

int main(int argc, char **argv) {
    pmm_manager = &buddy_pmm_manager;
    if (argc > 1 && argv[1][0] == 'f') { pmm_manager = &default_pmm_manager; }
    if (argc > 1 && argv[1][0] == 'b' && argv[1][1] == 'e') {
        pmm_manager = &best_fit_pmm_manager;
    }
    va_pa_offset = (uintptr_t)memory - DRAM_BASE;
    reset();
    pmm_manager->init_memmap(storage, NPAGES);
    if (argc > 2) {
        struct Page *p = alloc_pages(3);
        if (argv[2][0] == 's') { free_pages(p, 4); }
        if (argv[2][0] == 't') { free_page(p + 1); }
        if (argv[2][0] == 'd') { free_pages(p, 3); free_pages(p, 3); }
        return 1;
    }
    pmm_manager->check();
    if (pmm_manager == &buddy_pmm_manager) {
        buddy_edges();
        buddy_random();
        reset();
        pmm_manager->init_memmap(storage, NPAGES);
    }
    slub_init();
    slub_check();
    slub_random();
    assert(nr_free_pages() == NPAGES);
    cprintf("%s + SLUB: PASS (ASan + UBSan)\n", pmm_manager->name);
    return 0;
}
