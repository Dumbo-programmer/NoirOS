#ifndef GAME_SNAKE_H
#define GAME_SNAKE_H

#include "apps.h"

void snake_init(void);
void snake_update(void);
void snake_draw(void);
/* Returns APP_STATUS_* */
int snake_handle_key(int k);

extern sys_app_t app_snake;

#endif
