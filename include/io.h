#ifndef IO_H
#define IO_H

#include "common.h"

static inline u8 io_in8(u16 port) {
    u8 val;
    __asm__ volatile ("inb %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

static inline void io_out8(u16 port, u8 val) {
    __asm__ volatile ("outb %0, %1" :: "a"(val), "Nd"(port));
}

static inline u16 io_in16(u16 port) {
    u16 val;
    __asm__ volatile ("inw %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

static inline void io_out16(u16 port, u16 val) {
    __asm__ volatile ("outw %0, %1" :: "a"(val), "Nd"(port));
}

static inline void io_wait(void) {
    __asm__ volatile ("outb %%al, $0x80" :: "a"(0));
}

static inline void io_cli(void) {
    __asm__ volatile ("cli");
}

static inline void io_sti(void) {
    __asm__ volatile ("sti");
}

static inline void io_hlt(void) {
    __asm__ volatile ("hlt");
}

#endif
