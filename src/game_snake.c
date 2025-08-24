#include "../include/game_snake.h"
#include "../include/vga.h"
#include "../include/util.h"
#include "../include/ui.h"     /* Window type if needed */
#include "../include/input.h" /* For K_ARROW_UP, K_ARROW_DOWN, etc */

#define SNAKE_MAX_LEN 100
#define GAME_W 40
#define GAME_H 18

struct SnakeGame {
    struct { int x, y; } body[SNAKE_MAX_LEN];
    int length;
    int dx, dy;
    struct { int x, y; } food;
    int score;
    int game_over;
} snake_game;

void snake_init(void) {
    snake_game.length = 3;
    snake_game.body[0].x = GAME_W / 2;
    snake_game.body[0].y = GAME_H / 2;
    snake_game.body[1].x = snake_game.body[0].x - 1;
    snake_game.body[1].y = snake_game.body[0].y;
    snake_game.body[2].x = snake_game.body[1].x - 1;
    snake_game.body[2].y = snake_game.body[1].y;
    snake_game.dx = 1; snake_game.dy = 0;
    snake_game.food.x = 10; snake_game.food.y = 10;
    snake_game.score = 0;
    snake_game.game_over = 0;
}

void snake_update(void) {
    if (snake_game.game_over) return;

    for (int i = snake_game.length - 1; i > 0; --i) {
        snake_game.body[i] = snake_game.body[i - 1];
    }
    snake_game.body[0].x += snake_game.dx;
    snake_game.body[0].y += snake_game.dy;

    if (snake_game.body[0].x < 0 || snake_game.body[0].x >= GAME_W ||
        snake_game.body[0].y < 0 || snake_game.body[0].y >= GAME_H) {
        snake_game.game_over = 1; return;
    }

    for (int i = 1; i < snake_game.length; ++i) {
        if (snake_game.body[0].x == snake_game.body[i].x &&
            snake_game.body[0].y == snake_game.body[i].y) {
            snake_game.game_over = 1; return;
        }
    }

    if (snake_game.body[0].x == snake_game.food.x &&
        snake_game.body[0].y == snake_game.food.y) {
        if (snake_game.length < SNAKE_MAX_LEN-1) snake_game.length++;
        snake_game.score += 10;
        snake_game.food.x = ((snake_game.score / 10) * 7) % GAME_W;
        snake_game.food.y = ((snake_game.score / 10) * 3) % GAME_H;
    }
}

void snake_draw(void) {
    vga_clear();

    for (int x = 0; x < WIDTH; ++x) vga_putcell(x, 0, ' ', 0x2F);
    const char* title = "NoirOS Snake Game - Use arrows to move, ESC to exit";
    for (int i = 0; title[i] && i < WIDTH - 2; ++i) vga_putcell(1 + i, 0, title[i], 0x2F);

    Window game_win = {20, 2, GAME_W + 2, GAME_H + 2, ""};
    draw_box(game_win.x, game_win.y, game_win.w, game_win.h, game_win.title, 0x0E, 0x80, 0x00);

    for (int i = 0; i < snake_game.length; ++i) {
        char ch = (i == 0) ? 'O' : 'o';
        u8 attr = (i == 0) ? 0x0A : 0x02;
        vga_putcell(game_win.x + 1 + snake_game.body[i].x,
                   game_win.y + 1 + snake_game.body[i].y, ch, attr);
    }

    vga_putcell(game_win.x + 1 + snake_game.food.x,
               game_win.y + 1 + snake_game.food.y, '*', 0x0C);

    char score_text[40];
    const char* s1 = "Score: ";
    int pos = 0;
    for (int i = 0; s1[i]; ++i) score_text[pos++] = s1[i];
    int score = snake_game.score;
    if (score == 0) score_text[pos++] = '0';
    else {
        char digs[10]; int d=0;
        while (score>0) { digs[d++] = '0' + (score%10); score/=10; }
        for (int i = d-1; i >= 0; --i) score_text[pos++] = digs[i];
    }
    score_text[pos]=0;
    for (int i = 0; score_text[i]; ++i) vga_putcell(1 + i, HEIGHT - 1, score_text[i], 0x0F);

    if (snake_game.game_over) {
        const char* msg = "GAME OVER! Press ESC to exit";
        int start_x = (WIDTH - kstrlen(msg)) / 2;
        for (int i = 0; msg[i]; ++i) vga_putcell(start_x + i, HEIGHT / 2, msg[i], 0x4F);
    }
}

void snake_handle_key(int k) {
    if (snake_game.game_over) return;
    if (k == K_ARROW_UP && snake_game.dy == 0) { snake_game.dx = 0; snake_game.dy = -1; }
    else if (k == K_ARROW_DOWN && snake_game.dy == 0) { snake_game.dx = 0; snake_game.dy = 1; }
    else if (k == K_ARROW_LEFT && snake_game.dx == 0) { snake_game.dx = -1; snake_game.dy = 0; }
    else if (k == K_ARROW_RIGHT && snake_game.dx == 0) { snake_game.dx = 1; snake_game.dy = 0; }
}
