#include "../include/vga.h"
#include "../include/common.h"
#include "../include/util.h"

/* Single authoritative VGA pointer declared volatile as required for
 * MMIO.  All accesses through this pointer go through the volatile path. */
volatile u16* const vga = (volatile u16*)VGA_ADDR;

/* Runtime screen size (start with compile-time defaults) */
int SCREEN_W = WIDTH;
int SCREEN_H = HEIGHT;

static int cursor_x = 0, cursor_y = 0;
static u8 default_attr = 0x07;

/**
 * Write a character cell to the VGA text buffer at (x,y).
 * @param x column (0..WIDTH-1)
 * @param y row (0..HEIGHT-1)
 * @param ch ASCII character to draw
 * @param attr attribute byte (foreground/background)
 */
void vga_putcell(int x, int y, char ch, u8 attr) {
    /* Use WIDTH/HEIGHT constants — never hardcode 80 or 25 here. */
    if (x < 0 || x >= SCREEN_W || y < 0 || y >= SCREEN_H) return;
    vga[y * SCREEN_W + x] = ((u16)attr << 8) | (u8)ch;
}

/**
 * Clear the entire VGA text screen and reset cursor position.
 */
void vga_clear(void) {
    for (int y = 0; y < SCREEN_H; ++y)
        for (int x = 0; x < SCREEN_W; ++x)
            vga_putcell(x, y, ' ', default_attr);
    cursor_x = cursor_y = 0;
}

/**
 * Put a single character to the text terminal stream, handling
 * newline, carriage return, tab and automatic scrolling.
 */
void term_putc(char c) {
    if (c == '\n') { cursor_x = 0; cursor_y++; goto check_scroll; }
    if (c == '\r') { cursor_x = 0; return; }
    if (c == '\t') {
        int spaces = 4 - (cursor_x % 4);
        while (spaces--) term_putc(' ');
        return;
    }
    vga_putcell(cursor_x, cursor_y, c, default_attr);
    cursor_x++;
    if (cursor_x >= SCREEN_W) { cursor_x = 0; cursor_y++; }
check_scroll:
    /* Scroll up one line if we fall off the bottom instead of clamping.
     * Clamping silently overwrites the last line — scrolling is correct. */
    if (cursor_y >= SCREEN_H) {
        /* Shift all rows up by one */
        for (int y = 1; y < SCREEN_H; ++y)
            for (int x = 0; x < SCREEN_W; ++x)
                vga[((y - 1) * SCREEN_W) + x] = vga[(y * SCREEN_W) + x];
        /* Clear the newly exposed bottom row */
        for (int x = 0; x < SCREEN_W; ++x)
            vga_putcell(x, SCREEN_H - 1, ' ', default_attr);
        cursor_y = SCREEN_H - 1;
    }
}

/**
 * Write a NUL-terminated string to the terminal.
 */
void term_write(const char* s) { while (*s) term_putc(*s++); }

/**
 * Draw a framed box at (x,y) with width w and height h.
 * The interior is filled with `bg_attr` and the top row may show `title`.
 */
void draw_box(int x, int y, int w, int h,
              const char* title, u8 title_attr, u8 border_attr, u8 bg_attr) {
    for (int i = 0; i < w; ++i) vga_putcell(x + i, y,         ' ', border_attr);
    for (int i = 0; i < w; ++i) vga_putcell(x + i, y + h - 1, ' ', border_attr);
    for (int i = 0; i < h; ++i) vga_putcell(x,         y + i, ' ', border_attr);
    for (int i = 0; i < h; ++i) vga_putcell(x + w - 1, y + i, ' ', border_attr);

    for (int yy = y + 1; yy < y + h - 1; ++yy)
        for (int xx = x + 1; xx < x + w - 1; ++xx)
            vga_putcell(xx, yy, ' ', bg_attr);

    if (title) {
        int len   = kstrlen(title);
        int start = x + 2;
        for (int i = 0; i < len && start + i < x + w - 2; ++i)
            vga_putcell(start + i, y, title[i], title_attr);
    }
}

/**
 * Draw `text` into a window region defined by (x,y,w,h) with
 * local window offset (wx,wy). Text wraps and respects newline/tab.
 */
void draw_text_in_win(int x, int y, int w, int h,
                      int wx, int wy, const char* text, u8 attr) {
    int sx = x + 1 + wx;
    int sy = y + 1 + wy;
    int cx = sx, cy = sy;
    const char* p = text;
    while (*p && cy < y + h - 1) {
        if (cx >= x + w - 1) { cx = x + 1; cy++; if (cy >= y + h - 1) break; }
        if (*p == '\n') { cx = x + 1; cy++; p++; continue; }
        if (*p == '\t') { cx = ((cx - x - 1) / 4 + 1) * 4 + x + 1; p++; continue; }
        if (cx >= x + 1 && cx < x + w - 1) vga_putcell(cx, cy, *p, attr);
        cx++; p++;
    }
}

/* Get character/attribute at screen position.
 * Use VGA_ADDR and WIDTH/HEIGHT constants — never hardcode 0xB8000, 80, or 25. */
/**
 * Get the character stored at screen cell (x,y).
 * @return ASCII character or space if out of bounds
 */
char vga_getcell_char(int x, int y) {
    if (x < 0 || x >= SCREEN_W || y < 0 || y >= SCREEN_H) return ' ';
    return (char)(vga[y * SCREEN_W + x] & 0xFF);
}

/**
 * Get the attribute byte stored at screen cell (x,y).
 * @return attribute or 0x07 if out of bounds
 */
unsigned char vga_getcell_attr(int x, int y) {
    if (x < 0 || x >= SCREEN_W || y < 0 || y >= SCREEN_H) return 0x07;
    return (unsigned char)((vga[y * SCREEN_W + x] >> 8) & 0xFF);
}

/* Set runtime screen mode; clears screen and clamps cursor */
void vga_set_mode(int w, int h) {
    if (w <= 0 || h <= 0) return;
    SCREEN_W = w;
    SCREEN_H = h;
    if (cursor_x >= SCREEN_W) cursor_x = SCREEN_W - 1;
    if (cursor_y >= SCREEN_H) cursor_y = SCREEN_H - 1;
    vga_clear();
}

int vga_get_width(void) { return SCREEN_W; }
int vga_get_height(void) { return SCREEN_H; }
