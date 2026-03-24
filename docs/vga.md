---
layout: default
title: VGA / Drawing Module
---

# VGA / Drawing (`src/vga.c`)

Functions:

- `vga_putcell(x,y,ch,attr)` — Write a character cell to VGA text buffer with bounds checking.
- `vga_clear()` — Clear the screen and reset cursor.
- `term_putc(c)` — Terminal character output with newline/tab handling and automatic scrolling.
- `term_write(s)` — Write a NUL-terminated string to the terminal.
- `draw_box(x,y,w,h,title,...)` — Draw a framed box with optional title and background attribute.
- `draw_text_in_win(x,y,w,h,wx,wy,text,attr)` — Draw text inside a window region with wrapping and newline/tab handling.
- `vga_getcell_char(x,y)`, `vga_getcell_attr(x,y)` — Read character and attribute at a screen cell.

Source: [src/vga.c](../src/vga.c)
