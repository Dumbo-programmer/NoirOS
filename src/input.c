#include "../include/input.h"
#include "../include/common.h"
#include "../include/idt.h"
#include "../include/io.h"
#include "../include/mouse.h"

#define KBD_QUEUE_SIZE 128

static volatile u8 kbd_queue[KBD_QUEUE_SIZE];
static volatile u8 kbd_head = 0;
static volatile u8 kbd_tail = 0;

/* Debug state */
static int input_debug = 0;
static int last_scancode = 0;
static int last_key = 0;

/* Keyboard state tracking */
static struct {
    u8 shift_pressed : 1;
    u8 ctrl_pressed  : 1;
    u8 alt_pressed   : 1;
    u8 caps_lock     : 1;
} kb_state = {0};

/* Scancode Set 1 maps (US QWERTY) */
static const char scancode_map_lower[128] = {
    0,   27,  '1', '2', '3', '4', '5', '6',
    '7', '8', '9', '0', '-', '=', '\b', '\t',
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i',
    'o', 'p', '[', ']', '\n', 0,  'a', 's',
    'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',
    '\'','`',  0,  '\\','z', 'x', 'c', 'v',
    'b', 'n', 'm', ',', '.', '/', 0,   '*',
    0,   ' ', 0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,
};

static const char scancode_map_upper[128] = {
    0,   27,  '!', '@', '#', '$', '%', '^',
    '&', '*', '(', ')', '_', '+', '\b', '\t',
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I',
    'O', 'P', '{', '}', '\n', 0,  'A', 'S',
    'D', 'F', 'G', 'H', 'J', 'K', 'L', ':',
    '"', '~',  0,  '|', 'Z', 'X', 'C', 'V',
    'B', 'N', 'M', '<', '>', '?', 0,   '*',
    0,   ' ', 0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,
};

/* Scancode Set 1 definitions */
#define SC_LSHIFT    0x2A
#define SC_RSHIFT    0x36
#define SC_LCTRL     0x1D
#define SC_LALT      0x38
#define SC_CAPS_LOCK 0x3A
#define SC_SPACE     0x39

static u8 e0_pending = 0;

static void kbd_push(u8 sc) {
    u8 next = (u8)((kbd_head + 1) % KBD_QUEUE_SIZE);
    if (next == kbd_tail) {
        /* Drop newest key when queue is full. */
        return;
    }
    kbd_queue[kbd_head] = sc;
    kbd_head = next;
}

static int kbd_pop(u8* out) {
    int ok = 0;
    io_cli();
    if (kbd_tail != kbd_head) {
        *out = kbd_queue[kbd_tail];
        kbd_tail = (u8)((kbd_tail + 1) % KBD_QUEUE_SIZE);
        ok = 1;
    }
    io_sti();
    return ok;
}

static void irq_keyboard(isr_frame_t* frame) {
    (void)frame;
    u8 sc = io_in8(0x60);
    last_scancode = sc;
    kbd_push(sc);
}

static void irq_mouse(isr_frame_t* frame) {
    (void)frame;
    u8 data = io_in8(0x60);
    mouse_irq_byte(data);
}

void input_init(void) {
    kbd_head = 0;
    kbd_tail = 0;
    e0_pending = 0;
    kb_state.shift_pressed = 0;
    kb_state.ctrl_pressed = 0;
    kb_state.alt_pressed = 0;
    kb_state.caps_lock = 0;

    irq_register_handler(1, irq_keyboard);
    irq_register_handler(12, irq_mouse);

    pic_clear_mask(1);
    pic_set_mask(12);
}

void input_enable_mouse_irq(void) {
    pic_clear_mask(12);
}

u8 kb_read_scancode(void) {
    u8 sc;
    if (kbd_pop(&sc)) return sc;
    return 0;
}

int read_key(void) {
    u8 sc = kb_read_scancode();
    if (!sc) return 0;

    last_scancode = sc;

    if (e0_pending) {
        e0_pending = 0;
        if (sc & 0x80) {
            u8 rel = sc & 0x7F;
            if (rel == 0x1D) kb_state.ctrl_pressed = 0;
            if (rel == 0x38) kb_state.alt_pressed  = 0;
            return 0;
        }
        if (sc == 0x48) return K_ARROW_UP;
        if (sc == 0x50) return K_ARROW_DOWN;
        if (sc == 0x4B) return K_ARROW_LEFT;
        if (sc == 0x4D) return K_ARROW_RIGHT;
        if (sc == 0x49) return K_PAGE_UP;
        if (sc == 0x51) return K_PAGE_DOWN;
        if (sc == 0x47) return K_HOME;
        if (sc == 0x4F) return K_END;
        if (sc == 0x53) return K_DEL;
        if (sc == 0x1D) { kb_state.ctrl_pressed = 1; return 0; }
        if (sc == 0x38) { kb_state.alt_pressed  = 1; return 0; }
        return 0;
    }

    if (sc == 0xE0) {
        e0_pending = 1;
        return 0;
    }

    if (sc & 0x80) {
        sc &= 0x7F;
        if (sc == SC_LSHIFT || sc == SC_RSHIFT) kb_state.shift_pressed = 0;
        if (sc == SC_LCTRL)                      kb_state.ctrl_pressed  = 0;
        if (sc == SC_LALT)                       kb_state.alt_pressed   = 0;
        return 0;
    }

    if (sc == SC_LSHIFT || sc == SC_RSHIFT) { kb_state.shift_pressed = 1; return 0; }
    if (sc == SC_LCTRL)                      { kb_state.ctrl_pressed  = 1; return 0; }
    if (sc == SC_LALT)                       { kb_state.alt_pressed   = 1; return 0; }
    if (sc == SC_CAPS_LOCK)                  { kb_state.caps_lock = !kb_state.caps_lock; return 0; }

    if (sc == 0x01)     return K_ESC;
    if (sc == SC_SPACE) return ' ';

    if (sc == 0x3B) return K_F1;
    if (sc == 0x3C) return K_F2;
    if (sc == 0x3D) return K_F3;

    if (sc < 128) {
        int use_upper = kb_state.shift_pressed;
        if ((sc >= 0x10 && sc <= 0x19) ||
            (sc >= 0x1E && sc <= 0x26) ||
            (sc >= 0x2C && sc <= 0x32)) {
            use_upper ^= kb_state.caps_lock;
        }

        char ch = use_upper ? scancode_map_upper[sc] : scancode_map_lower[sc];

        if (kb_state.ctrl_pressed) {
            if (ch >= 'a' && ch <= 'z') return ch - 'a' + 1;
            if (ch >= 'A' && ch <= 'Z') return ch - 'A' + 1;
        }

        last_key = (int)ch;
        return ch;
    }

    last_key = 0;
    return 0;
}

void input_toggle_debug(void) { input_debug = !input_debug; }
int input_debug_enabled(void) { return input_debug; }
int input_get_last_scancode(void) { return last_scancode; }
int input_get_last_key(void) { return last_key; }

int is_shift_pressed(void) { return kb_state.shift_pressed; }
int is_ctrl_pressed(void)  { return kb_state.ctrl_pressed;  }
int is_alt_pressed(void)   { return kb_state.alt_pressed;   }
int is_caps_lock_on(void)  { return kb_state.caps_lock;     }

int input_readline(char* buf, int max) {
    int len = 0;
    for (;;) {
        int ch = read_key();
        if (!ch) { io_hlt(); continue; }
        if (ch == '\n' || ch == '\r') { buf[len] = 0; return len; }
        if (ch == '\b' || ch == K_DEL) { if (len > 0) len--; continue; }
        if (ch >= 32 && ch <= 126 && len < max - 1) buf[len++] = (char)ch;
    }
}

int wait_key(void) {
    int key;
    do {
        key = read_key();
        if (!key) io_hlt();
    } while (!key);
    return key;
}

void input_reset_modifiers(void) {
    kb_state.shift_pressed = 0;
    kb_state.ctrl_pressed  = 0;
    kb_state.alt_pressed   = 0;
    kb_state.caps_lock = 0;
}
