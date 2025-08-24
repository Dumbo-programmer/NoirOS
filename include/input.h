#ifndef INPUT_H
#define INPUT_H

#include "common.h"

/* Special key codes */
#define K_ESC        27
#define K_ARROW_UP   256
#define K_ARROW_DOWN 257
#define K_ARROW_LEFT 258
#define K_ARROW_RIGHT 259
#define K_PAGE_UP    260
#define K_PAGE_DOWN  261
u8 kb_read_scancode(void);

/* Function keys */
#define K_F1         262
#define K_F2         263
#define K_F3         264

/* Function declarations */
int read_key(void);

/* Modifier state functions */
int input_readline(char *buf, int max);
int is_shift_pressed(void);
int is_ctrl_pressed(void);
int is_alt_pressed(void);
int is_caps_lock_on(void);

#endif 