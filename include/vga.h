#ifndef VGA_H
#define VGA_H
#include "common.h"

void vga_putcell(int x, int y, char ch, u8 attr);
void vga_clear(void);
void term_putc(char c);
void term_write(const char* s);
void draw_box(int x, int y, int w, int h, const char* title, u8 title_attr, u8 border_attr, u8 bg_attr);
void draw_text_in_win(int x, int y, int w, int h, int wx, int wy, const char* text, u8 attr);

#endif
