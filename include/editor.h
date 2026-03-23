#ifndef EDITOR_H
#define EDITOR_H

/* editor_open: open a file for editing; mode is passed so the editor
 * can request a return to MODE_BROWSER on ESC/save. */
void editor_open(const char* fname, int* mode);

/* editor_handle_key: process one keypress while in editor mode. */
void editor_handle_key(int key, int* mode);

/* editor_draw: redraw the editor contents. */
void editor_draw(void);

/* Optional mouse support — enabled when EDITOR_MOUSE_SUPPORT is defined. */
#define EDITOR_MOUSE_SUPPORT

#ifdef EDITOR_MOUSE_SUPPORT
void editor_set_cursor_pos(int x, int y);
#endif

#endif /* EDITOR_H */
