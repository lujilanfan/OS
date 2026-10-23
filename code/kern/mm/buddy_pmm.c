#include <buddy_pmm.h>
#include <list.h>
#include <stdio.h>

// Orders 0..30 cover blocks of up to 2^30 pages.
#define BUDDY_ORDERS 31
#define PG_buddy_alloc 2
#define BuddyAllocated(page) (((page)->flags >> PG_buddy_alloc) & 1)

static list_entry_t free_lists[BUDDY_ORDERS];
static size_t free_pages_count;

static size_t
block_pages(unsigned int order) {
    return (size_t)1 << order;
}

static void
buddy_init(void) {
    for (unsigned int order = 0; order < BUDDY_ORDERS; order ++) {
        list_init(&free_lists[order]);
    }
    free_pages_count = 0;
}

// A free head has property == its whole block size. Keep each order sorted.
static void
add_block(struct Page *base, unsigned int order) {
    size_t size = block_pages(order);
    assert(!PageReserved(base) && !PageProperty(base));
    assert((page2ppn(base) & (size - 1)) == 0);
    base->property = size;
    SetPageProperty(base);
    list_entry_t *le = list_next(&free_lists[order]);
    while (le != &free_lists[order] && le2page(le, page_link) < base) {
        le = list_next(le);
    }
    list_add_before(le, &base->page_link);
    free_pages_count += size;
}

static void
remove_block(struct Page *base, unsigned int order) {
    assert(PageProperty(base) && base->property == block_pages(order));
    list_del(&base->page_link);
    ClearPageProperty(base);
    base->property = 0;
    free_pages_count -= block_pages(order);
}

// Only an equally sized, free, aligned XOR partner can be coalesced.
static void
release_block(struct Page *base, unsigned int order) {
    while (order + 1 < BUDDY_ORDERS) {
        size_t size = block_pages(order);
        size_t partner_ppn = page2ppn(base) ^ size;
        if (partner_ppn < nbase || partner_ppn >= npage ||
            size > npage - partner_ppn) {
            break;
        }
        struct Page *partner = &pages[partner_ppn - nbase];
        if (PageReserved(partner) || !PageProperty(partner) ||
            partner->property != size) {
            break;
        }
        remove_block(partner, order);
        if (partner < base) {
            base = partner;
        }
        order ++;
    }
    add_block(base, order);
}

static void
buddy_init_memmap(struct Page *base, size_t n) {
    assert(n > 0 && base >= pages && n <= (size_t)(pages + npage - nbase - base));
    for (struct Page *p = base; p < base + n; p ++) {
        assert(PageReserved(p));
        p->flags = 0;
        p->property = 0;
        set_page_ref(p, 0);
    }
    while (n != 0) {
        unsigned int order = BUDDY_ORDERS - 1;
        while (block_pages(order) > n ||
               (page2ppn(base) & (block_pages(order) - 1)) != 0) {
            order --;
        }
        size_t size = block_pages(order);
        release_block(base, order);
        base += size;
        n -= size;
    }
}

static struct Page *
buddy_alloc_pages(size_t n) {
    assert(n > 0);
    unsigned int order = 0;
    while (order + 1 < BUDDY_ORDERS && block_pages(order) < n) {
        order ++;
    }
    if (block_pages(order) < n || block_pages(order) > free_pages_count) {
        return NULL;
    }
    unsigned int found = order;
    while (found < BUDDY_ORDERS && list_empty(&free_lists[found])) {
        found ++;
    }
    if (found == BUDDY_ORDERS) {
        return NULL;
    }
    struct Page *base = le2page(list_next(&free_lists[found]), page_link);
    remove_block(base, found);
    while (found > order) {
        found --;
        add_block(base + block_pages(found), found);
    }
    base->property = block_pages(order);
    base->flags |= (1UL << PG_buddy_alloc);
    return base;
}

static void
buddy_free_pages(struct Page *base, size_t n) {
    assert(base != NULL && n > 0 && BuddyAllocated(base));
    unsigned int order = 0;
    while (order + 1 < BUDDY_ORDERS && block_pages(order) < n) {
        order ++;
    }
    size_t size = block_pages(order);
    assert(size >= n && base->property == size);
    assert((page2ppn(base) & (size - 1)) == 0);
    for (struct Page *p = base; p < base + size; p ++) {
        assert(!PageReserved(p) && !PageProperty(p));
        p->flags = 0;
        p->property = 0;
        set_page_ref(p, 0);
    }
    release_block(base, order);
}

static size_t
buddy_nr_free_pages(void) {
    return free_pages_count;
}

static void
buddy_check(void) {
    size_t original = nr_free_pages();
    struct Page *pool = alloc_pages(8);
    assert(pool != NULL && nr_free_pages() == original - 8);

    // Isolate an aligned eight-page block for deterministic split/merge tests.
    list_entry_t saved[BUDDY_ORDERS];
    for (unsigned int i = 0; i < BUDDY_ORDERS; i ++) {
        saved[i] = free_lists[i];
        list_init(&free_lists[i]);
    }
    size_t saved_count = free_pages_count;
    free_pages_count = 0;
    free_pages(pool, 8);
    assert(nr_free_pages() == 8);
    assert(alloc_pages(9) == NULL);

    struct Page *a = alloc_pages(3);
    struct Page *b = alloc_page();
    struct Page *c = alloc_page();
    assert(a == pool && b == pool + 4 && c == pool + 5);
    assert(nr_free_pages() == 2 && alloc_pages(3) == NULL);
    free_pages(a, 3);
    free_page(b);
    assert(nr_free_pages() == 7 && alloc_pages(8) == NULL);
    free_page(c);
    assert(nr_free_pages() == 8);
    assert((a = alloc_pages(8)) == pool);
    assert(nr_free_pages() == 0);

    for (unsigned int i = 0; i < BUDDY_ORDERS; i ++) {
        assert(list_empty(&free_lists[i]));
        free_lists[i] = saved[i];
    }
    free_pages_count = saved_count;
    free_pages(a, 8);
    assert(nr_free_pages() == original);

    // An unaligned, non-power-of-two range must remain within its bounds.
    pool = alloc_pages(16);
    assert(pool != NULL);
    for (unsigned int i = 0; i < BUDDY_ORDERS; i ++) {
        saved[i] = free_lists[i];
        list_init(&free_lists[i]);
    }
    saved_count = free_pages_count;
    free_pages_count = 0;
    for (struct Page *p = pool; p < pool + 16; p ++) {
        p->flags = 0;
        p->property = 0;
        SetPageReserved(p);
    }
    buddy_init_memmap(pool + 1, 14);
    assert(nr_free_pages() == 14 && alloc_pages(8) == NULL);
    for (int i = 0; i < 14; i ++) {
        struct Page *p = alloc_page();
        assert(p > pool && p < pool + 15);
    }
    assert(nr_free_pages() == 0 && alloc_page() == NULL);
    for (struct Page *p = pool; p < pool + 16; p ++) {
        p->flags = 0;
        p->property = 0;
    }
    pool->property = 16;
    pool->flags |= (1UL << PG_buddy_alloc);
    for (unsigned int i = 0; i < BUDDY_ORDERS; i ++) {
        assert(list_empty(&free_lists[i]));
        free_lists[i] = saved[i];
    }
    free_pages_count = saved_count;
    free_pages(pool, 16);
    assert(nr_free_pages() == original);
    cprintf("buddy_check() succeeded!\n");
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
