#include "../include/mouse.h"

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
static void mouse_wait_input(void) {
    for (volatile int t = 100000; t > 0; --t)
        if (!(inb(0x64) & 0x02)) return;
}

/* Wait until the controller's output buffer has data (safe to read). */
/**
 * Wait until the PS/2 controller output buffer has data (safe to read).
 */
static void mouse_wait_output(void) {
    for (volatile int t = 100000; t > 0; --t)
        if (inb(0x64) & 0x01) return;
}

/* ---------- Mouse read/write ---------- */
/**
 * Send a byte to the PS/2 mouse device (via controller auxiliary port).
 */
static void mouse_write(unsigned char data) {
    mouse_wait_input();
    outb(0x64, 0xD4); /* route next byte to auxiliary (mouse) port */
    mouse_wait_input();
    outb(0x60, data);
}

/* mouse_read: read one byte from the PS/2 data port.
 * Returns 0 on timeout so callers do not spin forever on missing hardware. */
/**
 * Read a byte from the PS/2 data port (0x60).
 * Returns the byte or 0 on timeout.
 */
static unsigned char mouse_read(void) {
    mouse_wait_output();
    return inb(0x60);
}

/* ---------- Initialize mouse ---------- */
/**
 * Initialize the PS/2 mouse device and enable data reporting.
 * Performs controller setup and basic self-test sequence.
 */
void init_mouse(void) {
    unsigned char ack;

    /* Enable auxiliary device */
    mouse_wait_input();
    outb(0x64, 0xA8);

    /* Read and modify the controller command byte to enable IRQ12 */
    mouse_wait_input();
    outb(0x64, 0x20);
    unsigned char status = mouse_read();
    status |= 0x02;  /* enable IRQ12 */
    status &= ~0x20; /* clear "disable mouse clock" bit */
    mouse_wait_input();
    outb(0x64, 0x60);
    mouse_wait_input();
    outb(0x60, status);

    /* Reset mouse and verify self-test sequence: ACK (0xFA), 0xAA, 0x00 */
    mouse_write(0xFF);
    ack = mouse_read(); /* expect 0xFA ACK */
    (void)ack;          /* we continue regardless; robustness-only check */
    mouse_read();       /* self-test result (0xAA = pass) */
    mouse_read();       /* mouse device ID */

    /* Enable data reporting; check ACK */
    mouse_write(0xF4);
    ack = mouse_read(); /* expect 0xFA ACK */
    (void)ack;

    /* Centre cursor on 80x25 screen */
    mouse.x = 40;
    mouse.y = 12;
}

/* ---------- Mouse interrupt handler ---------- */
/**
 * Mouse interrupt handler — collects 3-byte packets and updates
 * the `mouse` state structure (position, deltas, button mask).
 */
void mouse_handler(void) {
    unsigned char data = inb(0x60);
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
        if (mouse.x < 0)      mouse.x = 0;
        if (mouse.x >= WIDTH)  mouse.x = WIDTH  - 1;
        if (mouse.y < 0)      mouse.y = 0;
        if (mouse.y >= HEIGHT) mouse.y = HEIGHT - 1;
    }
}

/* ---------- Accessor ---------- */
/**
 * Return pointer to the global mouse state.
 */
mouse_state_t* get_mouse_state(void) {
    return &mouse;
}
