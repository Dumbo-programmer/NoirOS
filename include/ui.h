#ifndef UI_H
#define UI_H
#include "common.h"
#include "fs.h"

typedef struct {
    int x,y,w,h;
    const char* title;
} Window;

/* Button structure for clickable elements */
typedef struct {
    int x, y, w, h;           /* Position and size */
    const char* text;         /* Button text */
    unsigned char normal_attr; /* Normal color */
    unsigned char pressed_attr; /* Pressed color */
    int pressed;              /* Pressed state */
    void (*callback)(void);   /* Click callback */
} Button;

void ui_draw(void);
void ui_set_selected(int sel);
int ui_get_selected(void);
void ui_scroll_viewer(int delta);
void ui_clear(void);
int ui_selected_file_index(void);

/* Explorer scroll reset (used after cd) */
void ui_reset_explorer_scroll(void);

/* Panel focus */
int  ui_get_active_panel(void);
void ui_toggle_active_panel(void);

/* Page sizes for kernel navigation */
int ui_explorer_page_size(void);
int ui_viewer_page_size(void);

/* Button callback setters */
void ui_set_restart_callback(void (*cb)(void));
void ui_set_shutdown_callback(void (*cb)(void));
void ui_set_sleep_callback(void (*cb)(void));

/* Key and mouse handling */
void ui_handle_key(int key);
void ui_handle_mouse_click(int x, int y, int button);

/* Power screen functions */
void show_restart_screen(void);
void show_shutdown_screen(void);
void show_sleep_screen(void);

/* Recompute window layout after a screen size change */
void ui_relayout(void);

#endif
