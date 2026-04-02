#ifndef MEMORY_H
#define MEMORY_H

#include "common.h"

void memory_init(void);
void paging_enable_identity(void);
void* kmalloc(u32 size);
void  kfree(void* ptr);
u32   memory_heap_used(void);

#endif
