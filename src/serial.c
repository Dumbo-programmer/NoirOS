#include "../include/serial.h"

static inline void outb(u16 port, u8 val) {
    __asm__ volatile ("outb %0, %1" :: "a"(val), "Nd"(port));
}

static inline u8 inb(u16 port) {
    u8 val;
    __asm__ volatile ("inb %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

/* Initialize COM1 (0x3F8) for simple byte output at 38400 baud.
 * This is minimal and good enough for debug logging to host when
 * QEMU is run with `-serial stdio`. */
void serial_init(void) {
    const u16 port = 0x3F8;
    outb(port + 1, 0x00);    /* Disable interrupts */
    outb(port + 3, 0x80);    /* Enable DLAB */
    outb(port + 0, 0x03);    /* Divisor low byte (38400) */
    outb(port + 1, 0x00);    /* Divisor high byte */
    outb(port + 3, 0x03);    /* 8 bits, no parity, one stop bit */
    outb(port + 2, 0xC7);    /* Enable FIFO, clear them, with 14-byte threshold */
    outb(port + 4, 0x0B);    /* IRQs enabled, RTS/DSR set */
}

/* Wait until transmitter holding register is empty. */
static void serial_wait(void) {
    const u16 port = 0x3F8;
    while ((inb(port + 5) & 0x20) == 0) { /* wait THR empty */ }
}

void serial_putc(char c) {
    const u16 port = 0x3F8;
    serial_wait();
    outb(port, (u8)c);
}

void serial_puts(const char* s) {
    for (int i = 0; s[i]; ++i) serial_putc(s[i]);
}
