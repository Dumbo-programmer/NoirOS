#ifndef COMMON_H
#define COMMON_H

typedef unsigned char  u8;
typedef unsigned short u16;
typedef unsigned int   u32;
typedef int            s32;
typedef unsigned long  uintptr;

#define VGA_ADDR 0xB8000
#define WIDTH 80
#define HEIGHT 25

/* Runtime screen dimensions (can be changed at runtime via vga_set_mode)
 * Default values are the same as WIDTH/HEIGHT macros. */
extern int SCREEN_W;
extern int SCREEN_H;

/* VGA 16-color palette indices */
#define COL_BLACK        0x0
#define COL_BLUE         0x1
#define COL_GREEN        0x2
#define COL_CYAN         0x3
#define COL_RED          0x4
#define COL_MAGENTA      0x5
#define COL_BROWN        0x6
#define COL_LIGHT_GREY   0x7
#define COL_DARK_GREY    0x8
#define COL_LIGHT_BLUE   0x9
#define COL_LIGHT_GREEN  0xA
#define COL_LIGHT_CYAN   0xB
#define COL_LIGHT_RED    0xC
#define COL_LIGHT_MAGENTA 0xD
#define COL_YELLOW       0xE
#define COL_WHITE        0xF

/* Build a VGA attribute byte from foreground and background color indices */
#define VGA_ATTR(fg, bg) (u8)(((bg) << 4) | ((fg) & 0xF))

/* Named theme attributes used throughout the OS */
#define ATTR_NORMAL       VGA_ATTR(COL_LIGHT_GREY,   COL_BLACK)
#define ATTR_TITLE        VGA_ATTR(COL_WHITE,         COL_BLUE)
#define ATTR_SELECTED     VGA_ATTR(COL_BLACK,         COL_LIGHT_CYAN)
#define ATTR_BORDER       VGA_ATTR(COL_LIGHT_CYAN,    COL_BLACK)
#define ATTR_STATUS       VGA_ATTR(COL_BLACK,         COL_LIGHT_GREY)
#define ATTR_ERROR        VGA_ATTR(COL_LIGHT_RED,     COL_BLACK)
#define ATTR_SUCCESS      VGA_ATTR(COL_LIGHT_GREEN,   COL_BLACK)
#define ATTR_PROMPT       VGA_ATTR(COL_YELLOW,        COL_BLACK)
#define ATTR_DIR          VGA_ATTR(COL_LIGHT_BLUE,    COL_BLACK)
#define ATTR_FILE_TEXT    VGA_ATTR(COL_LIGHT_GREY,    COL_BLACK)
#define ATTR_FILE_MD      VGA_ATTR(COL_LIGHT_GREEN,   COL_BLACK)
#define ATTR_FILE_HTML    VGA_ATTR(COL_LIGHT_CYAN,    COL_BLACK)
#define ATTR_FILE_NOIRC   VGA_ATTR(COL_YELLOW,        COL_BLACK)
#define ATTR_FILE_EXE     VGA_ATTR(COL_LIGHT_RED,     COL_BLACK)
#define ATTR_VIEWER_BG    VGA_ATTR(COL_LIGHT_GREY,    COL_BLACK)
#define ATTR_VIEWER_TITLE VGA_ATTR(COL_WHITE,         COL_BLUE)
#define ATTR_NOIRC_OUTPUT VGA_ATTR(COL_LIGHT_GREEN,   COL_BLACK)
#define ATTR_NOIRC_ERROR  VGA_ATTR(COL_LIGHT_RED,     COL_BLACK)
#define ATTR_NOIRC_PROMPT VGA_ATTR(COL_YELLOW,        COL_BLACK)

#endif
