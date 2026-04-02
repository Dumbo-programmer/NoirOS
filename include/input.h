#ifndef INPUT_H
#define INPUT_H

#include "common.h"

/* Special key codes */
#define K_ESC        27
#define K_TAB        9
#define K_ARROW_UP   256
#define K_ARROW_DOWN 257
#define K_ARROW_LEFT 258
#define K_ARROW_RIGHT 259
#define K_PAGE_UP    260
#define K_PAGE_DOWN  261
#define K_DEL        262
u8 kb_read_scancode(void);

/* Function keys */
#define K_F1         263
#define K_F2         264
#define K_F3         265
/* Additional keys */
#define K_HOME       266
#define K_END        267

/* Function declarations */
void input_init(void);
void input_enable_mouse_irq(void);
int read_key(void);
int wait_key(void);

/* Debug helpers: toggle overlay showing last scancode/key */
void input_toggle_debug(void);
int  input_debug_enabled(void);
int  input_get_last_scancode(void);
int  input_get_last_key(void);

/* Modifier state functions */
int input_readline(char *buf, int max);
int is_shift_pressed(void);
int is_ctrl_pressed(void);
int is_alt_pressed(void);
int is_caps_lock_on(void);
/* Reset modifier state (shift/ctrl/alt) to avoid stuck modifiers after modal reads */
void input_reset_modifiers(void);

#endif 