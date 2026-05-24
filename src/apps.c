#include "../include/apps.h"
#include "../include/util.h"

#define MAX_APPS 16
static sys_app_t* g_apps[MAX_APPS];
static int g_num_apps = 0;

static sys_app_t* g_active_app = 0;

int app_register(sys_app_t* app) {
    if (g_num_apps >= MAX_APPS || !app) return 0;
    g_apps[g_num_apps++] = app;
    return 1;
}

int app_launch(const char* name, const char* args) {
    for (int i = 0; i < g_num_apps; i++) {
        if (kstrcmp(g_apps[i]->name, name) == 0) {
            g_active_app = g_apps[i];
            if (g_active_app->init) g_active_app->init(args);
            return 1;
        }
    }
    return 0; // Not found
}

void app_active_update(void) {
    if (g_active_app && g_active_app->update) {
        g_active_app->update();
    }
}

void app_active_draw(void) {
    if (g_active_app && g_active_app->draw) {
        g_active_app->draw();
    }
}

int app_active_handle_key(int key) {
    if (g_active_app && g_active_app->handle_key) {
        return g_active_app->handle_key(key);
    }
    return APP_STATUS_EXIT; // Exit by default if no handler
}

sys_app_t* app_get_active(void) {
    return g_active_app;
}
