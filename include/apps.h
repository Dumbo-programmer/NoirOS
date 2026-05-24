#ifndef APPS_H
#define APPS_H
#include "common.h"

enum {
    APP_STATUS_RUNNING = 0,
    APP_STATUS_EXIT    = 1,
    APP_STATUS_RESTART = 2
};

typedef struct {
    const char* name;
    void (*init)(const char* args);
    void (*update)(void);
    void (*draw)(void);
    int (*handle_key)(int key); /* Returns APP_STATUS_* */
} sys_app_t;

/* Global App Registry API */
int app_register(sys_app_t* app);
int app_launch(const char* name, const char* args);
void app_active_update(void);
void app_active_draw(void);
int app_active_handle_key(int key);
sys_app_t* app_get_active(void);

#endif
