/* lab2 Challenge: 2413575, Ke Yunchao. Single-hart, single-page SLUB model. */
#include <slub.h>
#include <pmm.h>
#include <stdio.h>
#include <string.h>

#define CACHE_COUNT 8
#define BITMAP_WORDS 4

struct slab {
    list_entry_t link;
    struct Page *page;
    uint64_t used[BITMAP_WORDS];
    unsigned cache, inuse, capacity;
};

struct object_cache {
    list_entry_t partial, full;
};

static struct object_cache caches[CACHE_COUNT];
static bool initialized;

static size_t object_size(unsigned cache) {
    return (size_t)SLUB_ALIGNMENT << cache;
}

static size_t data_offset(void) {
    return ROUNDUP(sizeof(struct slab), SLUB_ALIGNMENT);
}

void slub_init(void) {
    assert(!initialized);
    for (unsigned i = 0; i < CACHE_COUNT; i++) {
        list_init(&caches[i].partial);
        list_init(&caches[i].full);
    }
    initialized = 1;
}

static struct slab *new_slab(unsigned cache) {
    struct Page *page = alloc_page();
    if (page == NULL) {
        return NULL;
    }
    struct slab *slab = (void *)(uintptr_t)(page2pa(page) + va_pa_offset);
    memset(slab, 0, sizeof(*slab));
    slab->page = page;
    slab->cache = cache;
    slab->capacity = (PGSIZE - data_offset()) / object_size(cache);
    assert(slab->capacity > 0 && slab->capacity <= BITMAP_WORDS * 64);
    list_add(&caches[cache].partial, &slab->link);
    return slab;
}

void *slub_alloc(size_t size) {
    assert(initialized);
    if (size == 0 || size > SLUB_MAX_SIZE) {
        return NULL;
    }
    unsigned cache = 0;
    while (object_size(cache) < size) {
        cache++;
    }
    struct object_cache *pool = &caches[cache];
    struct slab *slab = list_empty(&pool->partial) ? new_slab(cache) :
        to_struct(list_next(&pool->partial), struct slab, link);
    if (slab == NULL) {
        return NULL;
    }
    for (unsigned i = 0; i < slab->capacity; i++) {
        uint64_t mask = (uint64_t)1 << (i % 64);
        if (!(slab->used[i / 64] & mask)) {
            slab->used[i / 64] |= mask;
            slab->inuse++;
            if (slab->inuse == slab->capacity) {
                list_del(&slab->link);
                list_add(&pool->full, &slab->link);
            }
            return (char *)slab + data_offset() + i * object_size(cache);
        }
    }
    panic("slub: partial list contains a full slab");
}

bool slub_free(void *object) {
    assert(initialized);
    if (object == NULL) {
        return 1;
    }
    uintptr_t address = (uintptr_t)object;
    uintptr_t page_address = ROUNDDOWN(address, PGSIZE);
    /* Search owned pages before dereferencing an untrusted object address. */
    for (unsigned cache = 0; cache < CACHE_COUNT; cache++) {
        list_entry_t *heads[] = {&caches[cache].partial, &caches[cache].full};
        for (unsigned h = 0; h < 2; h++) {
            list_entry_t *head = heads[h];
            for (list_entry_t *le = list_next(head); le != head; le = list_next(le)) {
                struct slab *slab = to_struct(le, struct slab, link);
                if ((uintptr_t)slab != page_address) {
                    continue;
                }
                uintptr_t start = page_address + data_offset();
                size_t size = object_size(cache);
                if (address < start || (address - start) % size != 0) {
                    return 0;
                }
                size_t index = (address - start) / size;
                if (index >= slab->capacity) {
                    return 0;
                }
                uint64_t mask = (uint64_t)1 << (index % 64);
                if (!(slab->used[index / 64] & mask)) {
                    return 0;
                }
                slab->used[index / 64] &= ~mask;
                bool was_full = slab->inuse == slab->capacity;
                slab->inuse--;
                if (slab->inuse == 0) {
                    struct Page *page = slab->page;
                    list_del(&slab->link);
                    free_page(page);
                } else if (was_full) {
                    list_del(&slab->link);
                    list_add(&caches[cache].partial, &slab->link);
                }
                return 1;
            }
        }
    }
    return 0;
}

void slub_check(void) {
    const size_t sizes[] = {1, 15, 16, 17, 31, 32, 33, 63, 64, 65,
        127, 128, 129, 255, 256, 257, 511, 512, 513, 1023, 1024,
        1025, 2047, 2048};
    void *objects[260];
    size_t before = nr_free_pages();
    for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++) {
        objects[i] = slub_alloc(sizes[i]);
        assert(objects[i] != NULL);
        assert((uintptr_t)objects[i] % SLUB_ALIGNMENT == 0);
        memset(objects[i], (int)i + 1, sizes[i]);
    }
    for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++) {
        for (size_t j = 0; j < sizes[i]; j++) {
            assert(((unsigned char *)objects[i])[j] == i + 1);
        }
    }
    for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++) {
        assert(slub_free(objects[i]));
    }
    assert(nr_free_pages() == before);
    cprintf("[SLUB] classes/alignment/data independence: PASS\n");

    for (size_t i = 0; i < 260; i++) {
        objects[i] = slub_alloc(16);
        assert(objects[i] != NULL);
        memset(objects[i], (int)(i % 251), 16);
    }
    assert(before - nr_free_pages() >= 2);
    assert(!slub_free((char *)objects[0] + 1));
    assert(!slub_free((void *)(uintptr_t)1));
    assert(slub_free(objects[0]));
    assert(!slub_free(objects[0]));
    void *reused = slub_alloc(16);
    assert(reused == objects[0]);
    objects[0] = reused;
    memset(reused, 0, 16);
    for (size_t i = 0; i < 260; i++) {
        for (size_t j = 0; j < 16; j++) {
            assert(((unsigned char *)objects[i])[j] == i % 251);
        }
    }
    for (size_t parity = 0; parity < 2; parity++) {
        for (size_t i = parity; i < 260; i += 2) {
            assert(slub_free(objects[i]));
        }
    }
    assert(slub_alloc(0) == NULL && slub_alloc(SLUB_MAX_SIZE + 1) == NULL);
    assert(slub_alloc((size_t)-1) == NULL && slub_free(NULL));
    assert(!slub_free(objects[0]));
    assert(nr_free_pages() == before);
    cprintf("[SLUB] multi-slab/reuse/invalid input/page return: PASS\n");
    cprintf("[SLUB] all checks passed; free pages restored: %lu\n", before);
}
