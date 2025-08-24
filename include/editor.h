#ifndef EDITOR_H
#define EDITOR_H

void editor_open(const char *fname, int *mode);
/* editor_handle_key receives a pointer to mode so it can request exit */
void editor_handle_key(int key, int *mode);
void editor_draw(void);

#endif
#define EDITOR_MOUSE_SUPPORT

/* Function declarations */
void editor_draw(void);
void editor_open(const char *fname, int *mode);
void editor_handle_key(int key, int *mode);

#ifdef EDITOR_MOUSE_SUPPORT
void editor_set_cursor_pos(int x, int y);
#endif
