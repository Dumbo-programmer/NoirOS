#ifndef TASK_H
#define TASK_H

#include "common.h"
#include "idt.h"

#define KERNEL_STACK_SIZE 8192
#define MAX_TASKS 16

typedef enum {
    TASK_STATE_FREE,
    TASK_STATE_RUNNABLE,
    TASK_STATE_SLEEPING,
    TASK_STATE_DEAD
} task_state_t;

typedef struct {
    u32 id;
    task_state_t state;
    u32 esp;
    u32 kernel_stack[KERNEL_STACK_SIZE / 4];
    u32 sleep_ticks;
} task_t;

void tasking_init(void);
int task_create(void (*entry)(void));
u32 task_switch(u32 current_esp);
void task_exit(void);
void task_sleep(u32 ticks);

#endif
