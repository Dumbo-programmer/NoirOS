#include "../include/mouse.h"
#include "../include/common.h"
#include "../include/vga.h"

static mouse_state_t mouse     = {0};
static unsigned char mouse_packet[3];
static unsigned char packet_index = 0;

/* ---------- Port I/O helpers ---------- */
/**
 * Read a byte from an I/O port.
 */
static inline unsigned char inb(unsigned short port) {
    unsigned char val;
    __asm volatile ("inb %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

/**
 * Write a byte to an I/O port.
 */
static inline void outb(unsigned short port, unsigned char val) {
    __asm volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

/* ---------- Wait helpers ----------
 * Bit 1 of the status byte (0x64) is the "input buffer full" flag:
 *   1 = controller is busy, do NOT write yet.
 *   0 = controller is ready to accept a byte.
 * Bit 0 is the "output buffer full" flag:
 *   1 = a byte is waiting to be read from 0x60.
 *   0 = nothing to read yet.
 * Previous code had correct logic; comments clarify intent. */

/* Wait until the controller's input buffer is empty (safe to write). */
/**
 * Wait until the PS/2 controller input buffer is clear (safe to write).
 * Uses a timeout to avoid infinite blocking when hardware is absent.
 */
static int mouse_wait_input(void) {
    for (volatile int t = 100000; t > 0; --t)
        if (!(inb(0x64) & 0x02)) return 1;
    return 0;
}

/* Wait until the controller's output buffer has data (safe to read). */
/**
 * Wait until the PS/2 controller output buffer has data (safe to read).
 */
static int mouse_wait_output(void) {
    for (volatile int t = 100000; t > 0; --t)
        if (inb(0x64) & 0x01) return 1;
    return 0;
}

/* ---------- Mouse read/write ---------- */
/**
 * Send a byte to the PS/2 mouse device (via controller auxiliary port).
 */
static int mouse_write(unsigned char data) {
    if (!mouse_wait_input()) return 0;
    outb(0x64, 0xD4); /* route next byte to auxiliary (mouse) port */
    if (!mouse_wait_input()) return 0;
    outb(0x60, data);
    return 1;
}

/* mouse_read: read one byte from the PS/2 data port.
 * Returns 0 on timeout so callers do not spin forever on missing hardware. */
/**
 * Read a byte from the PS/2 data port (0x60).
 * Returns the byte or 0 on timeout.
 */
static int mouse_read(unsigned char* out) {
    if (!mouse_wait_output()) return 0;
    *out = inb(0x60);
    return 1;
}

static int mouse_expect_ack(void) {
    unsigned char b;
    if (!mouse_read(&b)) return 0;
    return b == 0xFA;
}

static int mouse_drain_output(void) {
    for (int i = 0; i < 32; ++i) {
        if (!(inb(0x64) & 0x01)) return 1;
        (void)inb(0x60);
    }
    return 1;
}

/* mouse_read: read one byte from the PS/2 data port.
 * Returns 0 on timeout so callers do not spin forever on missing hardware. */
/**
 * Read a byte from the PS/2 data port (0x60).
 * Returns the byte or 0 on timeout.
 */
static unsigned char mouse_read_compat(void) {
    unsigned char v = 0;
    if (mouse_read(&v)) return v;
    return inb(0x60);
}

/* ---------- Initialize mouse ---------- */
/**
 * Initialize the PS/2 mouse device and enable data reporting.
 * Performs controller setup and basic self-test sequence.
 */
int init_mouse(void) {
    unsigned char ack;
    unsigned char tmp;

    mouse_drain_output();

    /* Enable auxiliary device */
    if (!mouse_wait_input()) return 0;
    outb(0x64, 0xA8);

    /* Read and modify the controller command byte to enable IRQ12 */
    if (!mouse_wait_input()) return 0;
    outb(0x64, 0x20);
    if (!mouse_read(&tmp)) return 0;
    unsigned char status = tmp;
    status |= 0x02;  /* enable IRQ12 */
    status &= ~0x20; /* clear "disable mouse clock" bit */
    if (!mouse_wait_input()) return 0;
    outb(0x64, 0x60);
    if (!mouse_wait_input()) return 0;
    outb(0x60, status);

    /* Set defaults and enable data reporting with ACK checks. */
    if (!mouse_write(0xF6)) return 0;
    if (!mouse_expect_ack()) return 0;

    if (!mouse_write(0xF4)) return 0;
    if (!mouse_expect_ack()) return 0;
    ack = 0xFA;
    (void)ack;

    /* Centre cursor on the runtime screen size */
    mouse.x = SCREEN_W / 2;
    mouse.y = SCREEN_H / 2;
    mouse.delta_x = 0;
    mouse.delta_y = 0;
    mouse.buttons = 0;
    packet_index = 0;
    return 1;
}

/* ---------- Mouse interrupt handler ---------- */
/**
 * Consume one raw PS/2 mouse byte and update packet state.
 */
void mouse_irq_byte(unsigned char data) {
    mouse_packet[packet_index++] = data;

    if (packet_index == 3) {
        packet_index = 0;

        unsigned char  flags  = mouse_packet[0];
        signed   char  delta_x = (signed char)mouse_packet[1];
        signed   char  delta_y = (signed char)mouse_packet[2];

        /* Bit 3 of the flags byte must always be 1 in a valid packet. */
        if (!(flags & 0x08)) return;

        mouse.buttons = flags & 0x07;
        mouse.delta_x = delta_x;
        mouse.delta_y = -delta_y; /* Y axis is inverted relative to screen */

        mouse.x += delta_x / 4;
        mouse.y -= delta_y / 4;  /* subtract because delta_y is already negated */

        /* Clamp to screen bounds */
        if (mouse.x < 0)          mouse.x = 0;
        if (mouse.x >= SCREEN_W)  mouse.x = SCREEN_W - 1;
        if (mouse.y < 0)          mouse.y = 0;
        if (mouse.y >= SCREEN_H)  mouse.y = SCREEN_H - 1;
    }
}

/* Legacy compatibility wrapper used by older polling paths. */
void mouse_handler(void) {
    mouse_irq_byte(mouse_read_compat());
}

/* ---------- Accessor ---------- */
/**
 * Return pointer to the global mouse state.
 */
mouse_state_t* get_mouse_state(void) {
    return &mouse;
}
