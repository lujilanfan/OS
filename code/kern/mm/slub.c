#include <slub.h>
#include <list.h>
#include <pmm.h>
#include <stdio.h>

#define SLUB_ALIGN 16
#define SLUB_SLAB_MAGIC  0x534c5542534c4142UL
#define SLUB_LARGE_MAGIC 0x534c55424c415247UL
#define SLUB_BITMAP_WORDS 4

static const size_t size_classes[] = {16, 32, 64, 128, 256, 512, 1024, 2048};
#define SLUB_CLASS_COUNT (sizeof(size_classes) / sizeof(size_classes[0]))

struct Slab {
    uint64_t magic;
    list_entry_t link;
    struct Page *page;
    void *free_head;
    size_t stride;
    size_t slot_offset;
    unsigned int capacity;
    unsigned int free_count;
    uint64_t used[SLUB_BITMAP_WORDS];
};

struct LargeObject {
    uint64_t magic;
    struct Page *page;
    size_t page_count;
    size_t requested_size;
};

static list_entry_t cache_lists[SLUB_CLASS_COUNT];
static bool initialized;

static void *
page_address(struct Page *page) {
    return (void *)(page2pa(page) + va_pa_offset);
}

void
slub_init(void) {
    for (size_t i = 0; i < SLUB_CLASS_COUNT; i ++) {
        list_init(&cache_lists[i]);
    }
    initialized = 1;
}

static struct Slab *
new_slab(size_t class_index) {
    struct Page *page = alloc_page();
    if (page == NULL) {
        return NULL;
    }
    struct Slab *slab = page_address(page);
    slab->magic = SLUB_SLAB_MAGIC;
    slab->page = page;
    slab->stride = size_classes[class_index];
    slab->slot_offset = ROUNDUP(sizeof(struct Slab), SLUB_ALIGN);
    slab->capacity = (PGSIZE - slab->slot_offset) / slab->stride;
    slab->free_count = slab->capacity;
    assert(slab->capacity > 0 && slab->capacity <= SLUB_BITMAP_WORDS * 64);
    for (size_t i = 0; i < SLUB_BITMAP_WORDS; i ++) {
        slab->used[i] = 0;
    }
    char *start = (char *)slab + slab->slot_offset;
    for (unsigned int i = 0; i < slab->capacity; i ++) {
        char *slot = start + i * slab->stride;
        *(void **)slot = (i + 1 < slab->capacity) ? slot + slab->stride : NULL;
    }
    slab->free_head = start;
    list_add_before(&cache_lists[class_index], &slab->link);
    return slab;
}

static void *
alloc_small(size_t class_index) {
    list_entry_t *head = &cache_lists[class_index];
    list_entry_t *le = list_next(head);
    while (le != head && ((struct Slab *)to_struct(le, struct Slab, link))->free_count == 0) {
        le = list_next(le);
    }
    struct Slab *slab = (le == head) ? new_slab(class_index)
                                     : to_struct(le, struct Slab, link);
    if (slab == NULL) {
        return NULL;
    }
    void *slot = slab->free_head;
    assert(slot != NULL && slab->free_count > 0);
    slab->free_head = *(void **)slot;
    size_t index = ((uintptr_t)slot - ((uintptr_t)slab + slab->slot_offset)) /
                   slab->stride;
    uint64_t mask = (uint64_t)1 << (index % 64);
    assert(index < slab->capacity && !(slab->used[index / 64] & mask));
    slab->used[index / 64] |= mask;
    slab->free_count --;
    return slot;
}

static void *
alloc_large(size_t size) {
    size_t offset = ROUNDUP(sizeof(struct LargeObject), SLUB_ALIGN);
    if (size > ~(size_t)0 - offset - (PGSIZE - 1)) {
        return NULL;
    }
    size_t count = (size + offset + PGSIZE - 1) / PGSIZE;
    struct Page *page = alloc_pages(count);
    if (page == NULL) {
        return NULL;
    }
    struct LargeObject *large = page_address(page);
    large->magic = SLUB_LARGE_MAGIC;
    large->page = page;
    large->page_count = count;
    large->requested_size = size;
    return (char *)large + offset;
}

void *
slub_alloc(size_t size) {
    assert(initialized);
    if (size == 0) {
        return NULL;
    }
    for (size_t i = 0; i < SLUB_CLASS_COUNT; i ++) {
        if (size <= size_classes[i]) {
            return alloc_small(i);
        }
    }
    return alloc_large(size);
}

void
slub_free(void *ptr) {
    if (ptr == NULL) {
        return;
    }
    assert(initialized && (uintptr_t)ptr >= KERNBASE &&
           (uintptr_t)ptr < KERNTOP);
    uintptr_t address = ROUNDDOWN((uintptr_t)ptr, PGSIZE);
    struct Slab *slab = (struct Slab *)address;
    if (slab->magic == SLUB_SLAB_MAGIC) {
        uintptr_t start = address + slab->slot_offset;
        uintptr_t value = (uintptr_t)ptr;
        assert(value >= start &&
               value < start + slab->capacity * slab->stride &&
               (value - start) % slab->stride == 0);
        size_t index = (value - start) / slab->stride;
        uint64_t mask = (uint64_t)1 << (index % 64);
        assert(slab->used[index / 64] & mask);
        slab->used[index / 64] &= ~mask;
        *(void **)ptr = slab->free_head;
        slab->free_head = ptr;
        slab->free_count ++;
        assert(slab->free_count <= slab->capacity);
        if (slab->free_count == slab->capacity) {
            struct Page *page = slab->page;
            list_del(&slab->link);
            slab->magic = 0;
            free_page(page);
        }
        return;
    }
    struct LargeObject *large = (struct LargeObject *)address;
    assert(large->magic == SLUB_LARGE_MAGIC &&
           ptr == (void *)(address + ROUNDUP(sizeof(struct LargeObject), SLUB_ALIGN)));
    struct Page *page = large->page;
    size_t count = large->page_count;
    large->magic = 0;
    free_pages(page, count);
}

void
slub_check(void) {
    size_t before = nr_free_pages();
    assert(slub_alloc(0) == NULL);
    slub_free(NULL);

    // Different requests select distinct size classes and large-page fallback.
    size_t sizes[] = {1, 16, 17, 63, 128, 513, 2048, 2049, 4096, 8192};
    void *objects[sizeof(sizes) / sizeof(sizes[0])];
    for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i ++) {
        objects[i] = slub_alloc(sizes[i]);
        assert(objects[i] != NULL && ((uintptr_t)objects[i] % SLUB_ALIGN) == 0);
        char *bytes = objects[i];
        bytes[0] = (char)(i + 1);
        if (sizes[i] > 1) {
            bytes[sizes[i] - 1] = (char)(i + 21);
        }
    }
    for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i ++) {
        char *bytes = objects[i];
        assert(bytes[0] == (char)(i + 1));
        if (sizes[i] > 1) {
            assert(bytes[sizes[i] - 1] == (char)(i + 21));
        }
    }
    for (size_t i = sizeof(sizes) / sizeof(sizes[0]); i > 0; i --) {
        slub_free(objects[i - 1]);
    }
    assert(nr_free_pages() == before);

    // More 16-byte objects than fit in one page exercise slab growth and reuse.
    void *many[260];
    for (size_t i = 0; i < 260; i ++) {
        many[i] = slub_alloc(16);
        assert(many[i] != NULL);
    }
    assert(nr_free_pages() == before - 2);
    for (size_t i = 0; i < 260; i += 2) {
        slub_free(many[i]);
    }
    assert(nr_free_pages() == before - 2);
    for (size_t i = 0; i < 260; i += 2) {
        many[i] = slub_alloc(16);
        assert(many[i] != NULL);
    }
    assert(nr_free_pages() == before - 2);
    for (size_t i = 1; i < 260; i += 2) {
        slub_free(many[i]);
    }
    for (size_t i = 0; i < 260; i += 2) {
        slub_free(many[i]);
    }
    assert(nr_free_pages() == before);
    cprintf("slub_check() succeeded!\n");
}
