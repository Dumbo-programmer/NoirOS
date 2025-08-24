#include "mouse.h"

static mouse_state_t mouse = {0};
static unsigned char mouse_packet[3];
static unsigned char packet_index = 0;

/* ---------- Port I/O helpers ---------- */
static inline unsigned char inb(unsigned short port) {
    unsigned char val;
    __asm volatile ("inb %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

static inline void outb(unsigned short port, unsigned char val) {
    __asm volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}


/* ---------- Wait helpers ---------- */
static void mouse_wait_input(void) {
    while (inb(0x64) & 0x02);  // wait until input buffer empty
}

static void mouse_wait_output(void) {
    while (!(inb(0x64) & 0x01));  // wait until output buffer full
}

/* ---------- Mouse read/write ---------- */
static void mouse_write(unsigned char data) {
    mouse_wait_input();
    outb(0x64, 0xD4);  // tell controller next byte is for mouse
    mouse_wait_input();
    outb(0x60, data);  // send byte to mouse
}

static unsigned char mouse_read(void) {
    mouse_wait_output();
    return inb(0x60);
}

/* ---------- Initialize mouse ---------- */
void init_mouse(void) {
    // Enable auxiliary device
    mouse_wait_input();
    outb(0x64, 0xA8);

    // Enable interrupts
    mouse_wait_input();
    outb(0x64, 0x20);
    unsigned char status = mouse_read();
    status |= 0x02;  // enable IRQ12
    mouse_wait_input();
    outb(0x64, 0x60);
    mouse_wait_input();
    outb(0x60, status);

    // Reset mouse
    mouse_write(0xFF);
    mouse_read();  // ACK
    mouse_read();  // self-test passed
    mouse_read();  // mouse ID

    // Enable data reporting
    mouse_write(0xF4);
    mouse_read();  // ACK

    // Initialize position
    mouse.x = 40;  // 80x25 center
    mouse.y = 12;
}

/* ---------- Mouse interrupt handler ---------- */
void mouse_handler(void) {
    unsigned char data = inb(0x60);
    mouse_packet[packet_index++] = data;

    if (packet_index == 3) {
        packet_index = 0;

        unsigned char flags = mouse_packet[0];
        signed char delta_x = (signed char)mouse_packet[1];
        signed char delta_y = (signed char)mouse_packet[2];

        if (!(flags & 0x08)) return;  // invalid packet

        // Update button states
        mouse.buttons = flags & 0x07;

        // Update position (invert Y axis)
        mouse.delta_x = delta_x;
        mouse.delta_y = -delta_y;

        mouse.x += delta_x / 4;  // reduce sensitivity
        mouse.y += delta_y / 4;

        // Bounds checking
        if (mouse.x < 0) mouse.x = 0;
        if (mouse.x >= 80) mouse.x = 79;
        if (mouse.y < 0) mouse.y = 0;
        if (mouse.y >= 25) mouse.y = 24;
    }
}

/* ---------- Getter for other modules ---------- */
mouse_state_t* get_mouse_state(void) {
    return &mouse;
}
