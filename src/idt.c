#include "../include/idt.h"
#include "../include/io.h"
#include "../include/serial.h"

#define IDT_ENTRIES 256
#define IRQ_BASE    32

typedef struct {
    u16 base_lo;
    u16 sel;
    u8  always0;
    u8  flags;
    u16 base_hi;
} __attribute__((packed)) idt_entry_t;

typedef struct {
    u16 limit;
    u32 base;
} __attribute__((packed)) idt_ptr_t;

extern void isr_stub_default(void);
extern void irq1_stub(void);
extern void irq12_stub(void);

static idt_entry_t idt[IDT_ENTRIES];
static idt_ptr_t idtp;
static irq_handler_t irq_handlers[16];

static void idt_set_gate(u8 num, u32 base, u16 sel, u8 flags) {
    idt[num].base_lo = (u16)(base & 0xFFFF);
    idt[num].sel = sel;
    idt[num].always0 = 0;
    idt[num].flags = flags;
    idt[num].base_hi = (u16)((base >> 16) & 0xFFFF);
}

static void lidt(const idt_ptr_t* p) {
    __asm__ volatile ("lidtl (%0)" :: "r"(p));
}

static void pic_remap(void) {
    u8 a1 = io_in8(0x21);
    u8 a2 = io_in8(0xA1);

    io_out8(0x20, 0x11);
    io_wait();
    io_out8(0xA0, 0x11);
    io_wait();

    io_out8(0x21, IRQ_BASE);
    io_wait();
    io_out8(0xA1, IRQ_BASE + 8);
    io_wait();

    io_out8(0x21, 0x04);
    io_wait();
    io_out8(0xA1, 0x02);
    io_wait();

    io_out8(0x21, 0x01);
    io_wait();
    io_out8(0xA1, 0x01);
    io_wait();

    io_out8(0x21, a1);
    io_out8(0xA1, a2);
}

void pic_set_mask(u8 irq) {
    u16 port = (irq < 8) ? 0x21 : 0xA1;
    u8 bit = (u8)(irq % 8);
    u8 val = io_in8(port) | (u8)(1U << bit);
    io_out8(port, val);
}

void pic_clear_mask(u8 irq) {
    u16 port = (irq < 8) ? 0x21 : 0xA1;
    u8 bit = (u8)(irq % 8);
    u8 val = io_in8(port) & (u8)~(1U << bit);
    io_out8(port, val);
}

void irq_register_handler(u8 irq, irq_handler_t handler) {
    if (irq < 16) irq_handlers[irq] = handler;
}

static void pic_send_eoi(u8 irq) {
    if (irq >= 8) io_out8(0xA0, 0x20);
    io_out8(0x20, 0x20);
}

void isr_dispatch_c(isr_frame_t* frame) {
    if (!frame) return;

    if (frame->int_no >= IRQ_BASE && frame->int_no < IRQ_BASE + 16) {
        u8 irq = (u8)(frame->int_no - IRQ_BASE);
        if (irq_handlers[irq]) irq_handlers[irq](frame);
        pic_send_eoi(irq);
        return;
    }

    if (frame->int_no < 32) {
        serial_puts("Kernel exception\n");
        for (;;) {
            io_hlt();
        }
    }
}

void idt_init(void) {
    io_cli();

    u16 kernel_cs;
    __asm__ volatile ("mov %%cs, %0" : "=r"(kernel_cs));

    for (int i = 0; i < IDT_ENTRIES; ++i) {
        idt_set_gate((u8)i, (u32)isr_stub_default, kernel_cs, 0x8E);
    }

    idt_set_gate(IRQ_BASE + 1, (u32)irq1_stub, kernel_cs, 0x8E);
    idt_set_gate(IRQ_BASE + 12, (u32)irq12_stub, kernel_cs, 0x8E);

    idtp.limit = (u16)(sizeof(idt_entry_t) * IDT_ENTRIES - 1);
    idtp.base = (u32)&idt;

    lidt(&idtp);
    pic_remap();

    for (u8 i = 0; i < 16; ++i) pic_set_mask(i);

    io_sti();
}
