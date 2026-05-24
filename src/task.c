#include "../include/task.h"
#include "../include/memory.h"
#include "../include/serial.h"
#include "../include/io.h"

static task_t tasks[MAX_TASKS];
static int current_task = -1;
static int tasking_enabled = 0;

void tasking_init(void) {
    for (int i = 0; i < MAX_TASKS; i++) {
        tasks[i].state = TASK_STATE_FREE;
        tasks[i].id = i;
    }
    /* Main kernel process implicitly becomes task 0 */
    tasks[0].state = TASK_STATE_RUNNABLE;
    current_task = 0;
    tasking_enabled = 1;
}

int task_create(void (*entry)(void)) {
    io_cli();
    int tid = -1;
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].state == TASK_STATE_FREE) {
            tid = i;
            break;
        }
    }

    if (tid == -1) {
        io_sti();
        return -1;
    }

    task_t* t = &tasks[tid];
    
    u32* stack = &t->kernel_stack[KERNEL_STACK_SIZE / 4];
    
    /* Hardware IRET frame */
    *(--stack) = 0x202; /* EFLAGS (interrupts enabled) */
    *(--stack) = 0x08;  /* CS (Kernel Code) */
    *(--stack) = (u32)entry; /* EIP */
    
    /* Software ISR frame mapping */
    *(--stack) = 0; /* Error Code */
    *(--stack) = 0; /* Int Number */
    
    /* Pusha */
    *(--stack) = 0; /* EAX */
    *(--stack) = 0; /* ECX */
    *(--stack) = 0; /* EDX */
    *(--stack) = 0; /* EBX */
    *(--stack) = 0; /* ESP (ignored) */
    *(--stack) = 0; /* EBP */
    *(--stack) = 0; /* ESI */
    *(--stack) = 0; /* EDI */

    t->esp = (u32)stack;
    t->state = TASK_STATE_RUNNABLE;
    t->sleep_ticks = 0;

    io_sti();
    return tid;
}

void task_exit(void) {
    io_cli();
    if (current_task >= 0 && current_task < MAX_TASKS) {
        tasks[current_task].state = TASK_STATE_FREE;
    }
    while (1) {
        io_hlt(); /* Wait for next IRQ0 to preempt us permanently */
    }
}

void task_sleep(u32 ticks) {
    io_cli();
    if (current_task >= 0 && current_task < MAX_TASKS) {
        tasks[current_task].state = TASK_STATE_SLEEPING;
        tasks[current_task].sleep_ticks = ticks;
    }
    io_sti();
    /* Force wait until ticked back to RUNNABLE */
    while (tasks[current_task].state == TASK_STATE_SLEEPING) {
        io_hlt();
    }
}

u32 task_switch(u32 current_esp) {
    if (!tasking_enabled) return current_esp;
    if (current_task == -1) return current_esp;

    /* Tick sleeping tasks */
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].state == TASK_STATE_SLEEPING) {
            if (tasks[i].sleep_ticks > 0) {
                tasks[i].sleep_ticks--;
                if (tasks[i].sleep_ticks == 0) {
                    tasks[i].state = TASK_STATE_RUNNABLE;
                }
            }
        }
    }

    /* Save old ESP if it's still legitimate */
    if (tasks[current_task].state != TASK_STATE_FREE) {
        tasks[current_task].esp = current_esp;
    }

    /* Round-robin to next runnable task */
    int next_task = current_task;
    for (int i = 0; i < MAX_TASKS; i++) {
        next_task = (next_task + 1) % MAX_TASKS;
        if (tasks[next_task].state == TASK_STATE_RUNNABLE) {
            current_task = next_task;
            return tasks[current_task].esp;
        }
    }

    /* Fallback directly to current if no other tasks runnable (i.e. kernel idle) */
    return current_esp;
}
