#include "../include/game_snake.h"
#include "../include/vga.h"
#include "../include/util.h"
#include "../include/ui.h"
#include "../include/input.h"

#define SNAKE_MAX_LEN 100
#define GAME_W 40
#define GAME_H 18

/* Game-area origin on the VGA screen (top-left of playfield, inside border). */
#define GAME_ORIGIN_X 21
#define GAME_ORIGIN_Y  3

struct SnakeGame {
    struct { int x, y; } body[SNAKE_MAX_LEN];
    int length;
    int dx, dy;
    struct { int x, y; } food;
    int score;
    int game_over;
} snake_game;

/* ---- Food placement ---- */

/* Check whether position (fx, fy) overlaps any snake body segment. */
static int food_on_snake(int fx, int fy) {
    for (int i = 0; i < snake_game.length; ++i)
        if (snake_game.body[i].x == fx && snake_game.body[i].y == fy) return 1;
    return 0;
}

/* Place food at a position that is not occupied by the snake body.
 * Uses a deterministic sequence seeded from score+tries to spread food around.
 * Falls back to a linear scan if the pseudo-random positions are all occupied
 * (which only happens when the snake is very long). */
static void place_food(void) {
    int seed = snake_game.score;
    for (int t = 0; t < GAME_W * GAME_H; ++t) {
        int fx = ((seed * 7 + t * 13 + 3) & 0x7FFFFFFF) % GAME_W;
        int fy = ((seed * 3 + t * 11 + 7) & 0x7FFFFFFF) % GAME_H;
        if (!food_on_snake(fx, fy)) {
            snake_game.food.x = fx;
            snake_game.food.y = fy;
            return;
        }
    }
    /* Exhaustive fallback (snake fills nearly the whole board) */
    for (int fy = 0; fy < GAME_H; ++fy)
        for (int fx = 0; fx < GAME_W; ++fx)
            if (!food_on_snake(fx, fy)) {
                snake_game.food.x = fx;
                snake_game.food.y = fy;
                return;
            }
    /* No free cell — game is effectively won; keep food where it is */
}

/* ---- Public API ---- */

void snake_init(void) {
    snake_game.length    = 3;
    snake_game.body[0].x = GAME_W / 2;
    snake_game.body[0].y = GAME_H / 2;
    snake_game.body[1].x = snake_game.body[0].x - 1;
    snake_game.body[1].y = snake_game.body[0].y;
    snake_game.body[2].x = snake_game.body[1].x - 1;
    snake_game.body[2].y = snake_game.body[1].y;
    snake_game.dx        = 1;
    snake_game.dy        = 0;
    snake_game.score     = 0;
    snake_game.game_over = 0;
    place_food();
}

void snake_update(void) {
    if (snake_game.game_over) return;

    /* Shift body segments back */
    for (int i = snake_game.length - 1; i > 0; --i)
        snake_game.body[i] = snake_game.body[i - 1];

    /* Advance head */
    snake_game.body[0].x += snake_game.dx;
    snake_game.body[0].y += snake_game.dy;

    /* Wall collision */
    if (snake_game.body[0].x < 0 || snake_game.body[0].x >= GAME_W ||
        snake_game.body[0].y < 0 || snake_game.body[0].y >= GAME_H) {
        snake_game.game_over = 1;
        return;
    }

    /* Self collision */
    for (int i = 1; i < snake_game.length; ++i) {
        if (snake_game.body[0].x == snake_game.body[i].x &&
            snake_game.body[0].y == snake_game.body[i].y) {
            snake_game.game_over = 1;
            return;
        }
    }

    /* Food collection */
    if (snake_game.body[0].x == snake_game.food.x &&
        snake_game.body[0].y == snake_game.food.y) {
        if (snake_game.length < SNAKE_MAX_LEN - 1) snake_game.length++;
        snake_game.score += 10;
        /* Place food at a new unoccupied position */
        place_food();
    }
}

void snake_draw(void) {
    vga_clear();

    /* Title bar */
    for (int x = 0; x < WIDTH; ++x) vga_putcell(x, 0, ' ', 0x2F);
    const char* title = "NoirOS Snake  Arrow keys to move  ESC to exit";
    for (int i = 0; title[i] && i < WIDTH - 2; ++i)
        vga_putcell(1 + i, 0, title[i], 0x2F);

    /* Game border — positioned using named constants, not bare magic numbers */
    draw_box(GAME_ORIGIN_X - 1, GAME_ORIGIN_Y - 1,
             GAME_W + 2, GAME_H + 2, "", 0x0E, 0x80, 0x00);

    /* Snake body */
    for (int i = 0; i < snake_game.length; ++i) {
        char ch   = (i == 0) ? 'O' : 'o';
        u8   attr = (i == 0) ? 0x0A : 0x02;
        vga_putcell(GAME_ORIGIN_X + snake_game.body[i].x,
                    GAME_ORIGIN_Y + snake_game.body[i].y, ch, attr);
    }

    /* Food */
    vga_putcell(GAME_ORIGIN_X + snake_game.food.x,
                GAME_ORIGIN_Y + snake_game.food.y, '*', 0x0C);

    /* Score */
    char score_text[32];
    const char* s1 = "Score: ";
    int pos = 0;
    for (int i = 0; s1[i]; ++i) score_text[pos++] = s1[i];
    int score = snake_game.score;
    if (score == 0) {
        score_text[pos++] = '0';
    } else {
        char digs[12];
        int d = 0;
        while (score > 0) { digs[d++] = '0' + (score % 10); score /= 10; }
        for (int i = d - 1; i >= 0; --i) score_text[pos++] = digs[i];
    }
    score_text[pos] = '\0';
    for (int i = 0; score_text[i]; ++i)
        vga_putcell(1 + i, HEIGHT - 1, score_text[i], 0x0F);

    /* Game over overlay */
    if (snake_game.game_over) {
        const char* msg = "GAME OVER!  Press ESC to exit";
        int start_x = (WIDTH - kstrlen(msg)) / 2;
        for (int i = 0; msg[i]; ++i)
            vga_putcell(start_x + i, HEIGHT / 2, msg[i], 0x4F);
    }
}

/* Returns 1 if ESC was pressed (caller should switch back to MODE_BROWSER). */
int snake_handle_key(int k) {
    if (k == K_ESC) return 1;   /* signal caller to exit game mode */
    if (snake_game.game_over) return 0;
    /* Prevent reversing directly into the body */
    if (k == K_ARROW_UP    && snake_game.dy == 0) { snake_game.dx = 0; snake_game.dy = -1; }
    else if (k == K_ARROW_DOWN  && snake_game.dy == 0) { snake_game.dx = 0; snake_game.dy =  1; }
    else if (k == K_ARROW_LEFT  && snake_game.dx == 0) { snake_game.dx = -1; snake_game.dy = 0; }
    else if (k == K_ARROW_RIGHT && snake_game.dx == 0) { snake_game.dx =  1; snake_game.dy = 0; }
    return 0;
}
