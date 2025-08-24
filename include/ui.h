#ifndef UI_H
#define UI_H
#include "common.h"
#include "fs.h"

typedef struct {
    int x,y,w,h;
    const char* title;
} Window;

void ui_draw(void);
void ui_set_selected(int sel);
int ui_get_selected(void);
void ui_scroll_viewer(int delta);
void ui_clear(void);   // this

#endif
