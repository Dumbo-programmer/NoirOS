#include "../include/memory.h"

#define HEAP_SIZE (128 * 1024)
#define ALIGN8(x) (((x) + 7U) & ~7U)

typedef struct block_header {
    u32 size;
    u32 used;
    struct block_header* next;
} block_header_t;

static u8 heap_area[HEAP_SIZE];
static block_header_t* heap_head = 0;

static u32 page_directory[1024] __attribute__((aligned(4096)));
static u32 page_tables[4][1024] __attribute__((aligned(4096)));

void paging_enable_identity(void) {
    /* Identity-map the first 16 MiB so large .bss regions remain accessible
     * after paging is enabled. */
    for (u32 i = 0; i < 1024; ++i) {
        page_directory[i] = 0x2;
    }

    for (u32 pde = 0; pde < 4; ++pde) {
        for (u32 pte = 0; pte < 1024; ++pte) {
            u32 frame = ((pde * 1024U) + pte) * 0x1000U;
            page_tables[pde][pte] = frame | 0x3;
        }
        page_directory[pde] = ((u32)page_tables[pde]) | 0x3;
    }

    __asm__ volatile ("mov %0, %%cr3" :: "r"(page_directory));

    u32 cr0;
    __asm__ volatile ("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000U;
    __asm__ volatile ("mov %0, %%cr0" :: "r"(cr0));
}

void memory_init(void) {
    heap_head = (block_header_t*)heap_area;
    heap_head->size = HEAP_SIZE - (u32)sizeof(block_header_t);
    heap_head->used = 0;
    heap_head->next = 0;

    paging_enable_identity();
}

void* kmalloc(u32 size) {
    if (!heap_head || size == 0) return 0;

    u32 need = ALIGN8(size);
    block_header_t* cur = heap_head;

    while (cur) {
        if (!cur->used && cur->size >= need) {
            u32 remain = cur->size - need;
            if (remain > sizeof(block_header_t) + 8U) {
                block_header_t* nxt = (block_header_t*)((u8*)(cur + 1) + need);
                nxt->size = remain - (u32)sizeof(block_header_t);
                nxt->used = 0;
                nxt->next = cur->next;
                cur->next = nxt;
                cur->size = need;
            }
            cur->used = 1;
            return (void*)(cur + 1);
        }
        cur = cur->next;
    }

    return 0;
}

static void merge_free_blocks(void) {
    block_header_t* cur = heap_head;
    while (cur && cur->next) {
        if (!cur->used && !cur->next->used) {
            cur->size += (u32)sizeof(block_header_t) + cur->next->size;
            cur->next = cur->next->next;
        } else {
            cur = cur->next;
        }
    }
}

void kfree(void* ptr) {
    if (!ptr) return;
    block_header_t* blk = ((block_header_t*)ptr) - 1;
    blk->used = 0;
    merge_free_blocks();
}

u32 memory_heap_used(void) {
    u32 used = 0;
    block_header_t* cur = heap_head;
    while (cur) {
        if (cur->used) used += cur->size;
        cur = cur->next;
    }
    return used;
}
