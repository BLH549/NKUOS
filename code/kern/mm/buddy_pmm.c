/* lab2: 2413575, Ke Yunchao. Binary buddy allocation on physical page numbers. */
#include <buddy_pmm.h>
#include <stdio.h>

#define BUDDY_ALLOC (1UL << 2)
#define BUDDY_TAIL  (1UL << 3)

static list_entry_t free_lists[BUDDY_MAX_ORDER + 1];
static size_t free_count;

static unsigned order_for(size_t n) {
    unsigned order = 0;
    while (((size_t)1 << order) < n && order < BUDDY_MAX_ORDER) {
        order++;
    }
    return order;
}

static void buddy_init(void) {
    for (unsigned i = 0; i <= BUDDY_MAX_ORDER; i++) {
        list_init(&free_lists[i]);
    }
    free_count = 0;
}

static void add_block(struct Page *base, unsigned order) {
    while (order < BUDDY_MAX_ORDER) {
        size_t buddy_ppn = page2ppn(base) ^ ((size_t)1 << order);
        size_t span = (size_t)1 << order;
        if (buddy_ppn < nbase || buddy_ppn >= npage ||
            span > npage - buddy_ppn) {
            break;
        }
        struct Page *other = pages + buddy_ppn - nbase;
        if (!PageProperty(other) || other->property != order) {
            break;
        }
        list_del_init(&other->page_link);
        ClearPageProperty(other);
        other->property = 0;
        if (other < base) {
            base = other;
        }
        order++;
    }
    base->property = order;
    SetPageProperty(base);
    list_add(&free_lists[order], &base->page_link);
}

static void buddy_init_memmap(struct Page *base, size_t n) {
    assert(n > 0 && base >= pages && base < pages + npage - nbase);
    assert(n <= (size_t)(pages + npage - nbase - base));
    for (size_t i = 0; i < n; i++) {
        assert(PageReserved(base + i));
    }
    for (size_t i = 0; i < n; i++) {
        base[i].flags = base[i].property = 0;
        set_page_ref(base + i, 0);
        list_init(&base[i].page_link);
    }
    free_count += n;
    while (n != 0) {
        unsigned order = BUDDY_MAX_ORDER;
        while (((size_t)1 << order) > n ||
               (page2ppn(base) & (((size_t)1 << order) - 1)) != 0) {
            order--;
        }
        add_block(base, order);
        size_t span = (size_t)1 << order;
        base += span;
        n -= span;
    }
}

static struct Page *buddy_alloc_pages(size_t n) {
    if (n == 0 || n > ((size_t)1 << BUDDY_MAX_ORDER)) {
        return NULL;
    }
    unsigned wanted = order_for(n), order = wanted;
    while (order <= BUDDY_MAX_ORDER && list_empty(&free_lists[order])) {
        order++;
    }
    if (order > BUDDY_MAX_ORDER) {
        return NULL;
    }
    struct Page *base = le2page(list_next(&free_lists[order]), page_link);
    list_del_init(&base->page_link);
    ClearPageProperty(base);
    while (order > wanted) {
        order--;
        struct Page *right = base + ((size_t)1 << order);
        right->property = order;
        SetPageProperty(right);
        list_add(&free_lists[order], &right->page_link);
    }
    size_t span = (size_t)1 << wanted;
    for (size_t i = 0; i < span; i++) {
        base[i].flags = BUDDY_TAIL;
        base[i].property = 0;
        set_page_ref(base + i, 0);
    }
    base->flags = BUDDY_ALLOC;
    base->property = n;
    free_count -= span;
    return base;
}

static void buddy_free_pages(struct Page *base, size_t n) {
    assert(n > 0 && n <= ((size_t)1 << BUDDY_MAX_ORDER));
    assert(base >= pages && base < pages + npage - nbase);
    assert(base->flags == BUDDY_ALLOC && base->property == n);
    unsigned order = order_for(n);
    size_t span = (size_t)1 << order;
    assert(span <= (size_t)(pages + npage - nbase - base));
    assert((page2ppn(base) & (span - 1)) == 0);
    for (size_t i = 1; i < span; i++) {
        assert(base[i].flags == BUDDY_TAIL);
    }
    for (size_t i = 0; i < span; i++) {
        base[i].flags = base[i].property = 0;
        set_page_ref(base + i, 0);
    }
    free_count += span;
    add_block(base, order);
}

static size_t buddy_nr_free_pages(void) {
    return free_count;
}

static void buddy_check(void) {
    size_t before = nr_free_pages();
    const size_t requests[] = {1, 2, 3, 5, 8, 17};
    struct Page *blocks[6];
    size_t consumed = 0;
    for (size_t i = 0; i < 6; i++) {
        size_t span = (size_t)1 << order_for(requests[i]);
        blocks[i] = alloc_pages(requests[i]);
        assert(blocks[i] != NULL);
        assert((page2ppn(blocks[i]) & (span - 1)) == 0);
        for (size_t j = 0; j < i; j++) {
            size_t other_span = (size_t)1 << order_for(requests[j]);
            assert(blocks[i] + span <= blocks[j] ||
                   blocks[j] + other_span <= blocks[i]);
        }
        consumed += span;
        assert(nr_free_pages() == before - consumed);
    }
    assert(alloc_pages(0) == NULL);
    assert(alloc_pages(((size_t)1 << BUDDY_MAX_ORDER) + 1) == NULL);
    assert(alloc_pages((size_t)-1) == NULL);
    assert(nr_free_pages() == before - consumed);
    const unsigned release_order[] = {2, 0, 5, 1, 4, 3};
    for (size_t i = 0; i < 6; i++) {
        unsigned j = release_order[i];
        free_pages(blocks[j], requests[j]);
    }
    assert(nr_free_pages() == before);
    cprintf("[Buddy] rounding/alignment/split/merge/count: PASS\n");
}

const struct pmm_manager buddy_pmm_manager = {
    .name = "buddy_pmm_manager",
    .init = buddy_init,
    .init_memmap = buddy_init_memmap,
    .alloc_pages = buddy_alloc_pages,
    .free_pages = buddy_free_pages,
    .nr_free_pages = buddy_nr_free_pages,
    .check = buddy_check,
};
