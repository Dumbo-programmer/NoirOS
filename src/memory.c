#include "../include/memory.h"
#include "../include/multiboot.h"
#include "../include/serial.h"

#define ALIGN8(x) (((x) + 7U) & ~7U)

typedef struct block_header {
    u32 size;
    u32 used;
    struct block_header* next;
} block_header_t;

/* Using dynamically discovered memory chunk instead of a static array */
static u8* heap_base = 0;
static u32 heap_total_size = 0;
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

void memory_init(u32 mboot_addr) {
    struct multiboot_info* mbi = (struct multiboot_info*)mboot_addr;
    
    u32 max_len = 0;
    u32 max_addr = 0;
    
    if (mbi->flags & (1 << 6)) {
        u32 mmap_addr = mbi->mmap_addr;
        u32 mmap_length = mbi->mmap_length;
        
        serial_puts("Multiboot mmap parsed:\n");
        for (u32 i = 0; i < mmap_length; ) {
            struct multiboot_mmap_entry* entry = (struct multiboot_mmap_entry*)(mmap_addr + i);
            /* Type 1 means available RAM */
            if (entry->type == 1) {
                if (entry->addr_high == 0 && entry->len_high == 0) {
                    /* Only grab RAM above 2MiB to avoid Kernel bss & structures */
                    if (entry->addr_low >= 0x200000 && entry->len_low > max_len) {
                        max_addr = entry->addr_low;
                        max_len = entry->len_low;
                    }
                }
            }
            i += entry->size + 4;
        }
    }
    
    /* Fallback if no multiboot mmap above 2M was found */
    if (max_len == 0) {
        serial_puts("No upper multiboot map found, falling back to 1MB heap at 0x400000.\n");
        max_addr = 0x400000;
        max_len = 1024 * 1024;
    }

    heap_base = (u8*)max_addr;
    heap_total_size = max_len;
    
    heap_head = (block_header_t*)heap_base;
    heap_head->size = heap_total_size - (u32)sizeof(block_header_t);
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
