#ifndef EDITOR_H
#define EDITOR_H

#include "apps.h"

/* editor_open: open a file for editing */
void editor_open(const char* fname);

/* editor_handle_key: process one keypress while in editor mode. Returns APP_STATUS_* */
int editor_handle_key(int key);

/* editor_draw: redraw the editor contents. */
void editor_draw(void);

/* Optional mouse support — enabled when EDITOR_MOUSE_SUPPORT is defined. */
#define EDITOR_MOUSE_SUPPORT

#ifdef EDITOR_MOUSE_SUPPORT
void editor_set_cursor_pos(int x, int y);
#endif

extern sys_app_t app_editor;

#endif /* EDITOR_H */
