#include "../include/vga.h"
#include "../include/common.h"
#include "../include/util.h"

volatile u16* const vga = (u16*)VGA_ADDR;
static int cursor_x = 0, cursor_y = 0;
static u8 default_attr = 0x07;

void vga_putcell(int x, int y, char ch, u8 attr) {
    if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT) return;
    vga[y * WIDTH + x] = ((u16)attr << 8) | (u8)ch;
}
void vga_clear(void) {
    for (int y = 0; y < HEIGHT; ++y)
        for (int x = 0; x < WIDTH; ++x)
            vga_putcell(x, y, ' ', default_attr);
    cursor_x = cursor_y = 0;
}
void term_putc(char c) {
    if (c == '\n') { cursor_x = 0; cursor_y++; return; }
    if (c == '\r') { cursor_x = 0; return; }
    if (c == '\t') { int spaces = 4 - (cursor_x % 4); while (spaces--) term_putc(' '); return; }
    vga_putcell(cursor_x, cursor_y, c, default_attr);
    cursor_x++;
    if (cursor_x >= WIDTH) { cursor_x = 0; cursor_y++; }
    if (cursor_y >= HEIGHT) cursor_y = HEIGHT - 1;
}
void term_write(const char* s) { while (*s) term_putc(*s++); }

void draw_box(int x, int y, int w, int h, const char* title, u8 title_attr, u8 border_attr, u8 bg_attr) {
    for (int i = 0; i < w; ++i) vga_putcell(x + i, y, ' ', border_attr);
    for (int i = 0; i < w; ++i) vga_putcell(x + i, y + h - 1, ' ', border_attr);
    for (int i = 0; i < h; ++i) vga_putcell(x, y + i, ' ', border_attr);
    for (int i = 0; i < h; ++i) vga_putcell(x + w - 1, y + i, ' ', border_attr);

    for (int yy = y + 1; yy < y + h - 1; ++yy)
        for (int xx = x + 1; xx < x + w - 1; ++xx)
            vga_putcell(xx, yy, ' ', bg_attr);

    if (title) {
        int len = kstrlen(title);
        int start = x + 2;
        for (int i = 0; i < len && start + i < x + w - 2; ++i)
            vga_putcell(start + i, y, title[i], title_attr);
    }
}

void draw_text_in_win(int x, int y, int w, int h, int wx, int wy, const char* text, u8 attr) {
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
/* Get character at screen position */
char vga_getcell_char(int x, int y) {
    if (x < 0 || x >= 80 || y < 0 || y >= 25) return ' ';
    volatile char* video = (volatile char*)0xB8000;
    return video[(y * 80 + x) * 2];
}

/* Get attribute at screen position */
unsigned char vga_getcell_attr(int x, int y) {
    if (x < 0 || x >= 80 || y < 0 || y >= 25) return 0x07;
    volatile char* video = (volatile char*)0xB8000;
    return video[(y * 80 + x) * 2 + 1];
}


/*
char vga_getcell_char(int x, int y);
unsigned char vga_getcell_attr(int x, int y);
*/
