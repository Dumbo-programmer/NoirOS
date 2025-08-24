#include "../include/apps.h"
#include "../include/game_snake.h"
#include "../include/editor.h"

void app_editor_open(const char* filename) {
    (void)filename;
    /* app wrappers are no-ops now, kernel/shell call editor_open() */
}

void app_snake_launch(void) {
    snake_init();
}
