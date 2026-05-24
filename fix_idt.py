with open('include/idt.h', 'r', encoding='utf-8') as f:
    text = f.read()

# isr_dispatch_c signature shouldn't be in idt.h anyway, it's external to assembly.
# let's check if it exists in idt.h or just in idt.c

with open('src/idt.c', 'r', encoding='utf-8') as f:
    text = f.read()

if 'void isr_dispatch_c(isr_frame_t* frame)' in text:
    old_func = '''void isr_dispatch_c(isr_frame_t* frame) {
    if (!frame) return;

    if (frame->int_no >= IRQ_BASE && frame->int_no < IRQ_BASE + 16) {
        u8 irq = (u8)(frame->int_no - IRQ_BASE);
        if (irq_handlers[irq]) irq_handlers[irq](frame);
        pic_send_eoi(irq);
        return;
    }

    if (frame->int_no < 32) {
        serial_puts("Kernel exception\\n");
        for (;;) {
            io_hlt();
        }
    }
}'''

    new_func = '''#include "../include/task.h"

u32 isr_dispatch_c(u32 esp) {
    isr_frame_t* frame = (isr_frame_t*)esp;
    if (!frame) return esp;

    if (frame->int_no >= IRQ_BASE && frame->int_no < IRQ_BASE + 16) {
        u8 irq = (u8)(frame->int_no - IRQ_BASE);
        if (irq_handlers[irq]) irq_handlers[irq](frame);
        pic_send_eoi(irq);
        
        /* If it's the PIT timer, do context switch */
        if (irq == 0) {
            return task_switch(esp);
        }
        return esp;
    }

    if (frame->int_no < 32) {
        serial_puts("Kernel exception\\n");
        for (;;) {
            io_hlt();
        }
    }
    return esp;
}'''
    text = text.replace(old_func, new_func)

# Add irq0_stub definition and IDT mapping
text = text.replace('extern void irq1_stub(void);', 'extern void irq0_stub(void);\nextern void irq1_stub(void);')

text = text.replace('idt_set_gate(IRQ_BASE + 1, (u32)irq1_stub, kernel_cs, 0x8E);', 'idt_set_gate(IRQ_BASE + 0, (u32)irq0_stub, kernel_cs, 0x8E);\n    idt_set_gate(IRQ_BASE + 1, (u32)irq1_stub, kernel_cs, 0x8E);')

# Clear irq 0 mask
text = text.replace('pic_set_mask(i);', '''pic_set_mask(i);
    }
    pic_clear_mask(0); /* Enable PIT IRQ0 */
    for (int i=0; i<0; ++i) { /* stub to absorb closing parenthesis if any */''')

with open('src/idt.c', 'w', encoding='utf-8') as f:
    f.write(text)

