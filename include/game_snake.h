#ifndef GAME_SNAKE_H
#define GAME_SNAKE_H

void snake_init(void);
void snake_update(void);
void snake_draw(void);
/* Returns 1 if ESC pressed (caller should switch mode to MODE_BROWSER). */
int snake_handle_key(int k);

#endif
