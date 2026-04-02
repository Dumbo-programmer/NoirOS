#ifndef IDT_H
#define IDT_H

#include "common.h"

typedef struct isr_frame {
    u32 edi;
    u32 esi;
    u32 ebp;
    u32 esp;
    u32 ebx;
    u32 edx;
    u32 ecx;
    u32 eax;
    u32 int_no;
    u32 err_code;
    u32 eip;
    u32 cs;
    u32 eflags;
} isr_frame_t;

typedef void (*irq_handler_t)(isr_frame_t* frame);

void idt_init(void);
void irq_register_handler(u8 irq, irq_handler_t handler);
void pic_set_mask(u8 irq);
void pic_clear_mask(u8 irq);

#endif
