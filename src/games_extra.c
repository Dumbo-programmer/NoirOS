#include "../include/games_extra.h"
#include "../include/vga.h"
#include "../include/input.h"
#include "../include/common.h"
#include "../include/util.h"

static unsigned int g_rng = 0xC0FFEEU;

static int rnd_int(int max) {
    g_rng = g_rng * 1664525U + 1013904223U;
    if (max <= 0) return 0;
    return (int)(g_rng % (unsigned int)max);
}

static void draw_title(const char* title, int score, int extra, const char* extra_label) {
    for (int x = 0; x < WIDTH; ++x) vga_putcell(x, 0, ' ', ATTR_TITLE);
    for (int i = 0; title[i] && i < WIDTH - 1; ++i) vga_putcell(i, 0, title[i], ATTR_TITLE);

    char sbuf[24];
    int_to_dec(sbuf, score);
    int col = WIDTH - 24;
    if (col < 0) col = 0;

    const char* s = "Score:";
    for (int i = 0; s[i] && col + i < WIDTH; ++i) vga_putcell(col + i, 0, s[i], ATTR_PROMPT);
    int c2 = col + 7;
    for (int i = 0; sbuf[i] && c2 + i < WIDTH; ++i) vga_putcell(c2 + i, 0, sbuf[i], ATTR_PROMPT);

    if (extra_label) {
        char ebuf[24];
        int_to_dec(ebuf, extra);
        int p = 0;
        while (extra_label[p] && col + p < WIDTH) {
            vga_putcell(col + p, 1, extra_label[p], ATTR_PROMPT);
            p++;
        }
        int q = p;
        while (ebuf[q - p] && col + q < WIDTH) {
            vga_putcell(col + q, 1, ebuf[q - p], ATTR_PROMPT);
            q++;
        }
    }
}

static void draw_footer(const char* msg) {
    for (int x = 0; x < WIDTH; ++x) vga_putcell(x, HEIGHT - 1, ' ', ATTR_VIEWER_TITLE);
    for (int i = 0; msg[i] && i < WIDTH - 1; ++i) vga_putcell(i, HEIGHT - 1, msg[i], ATTR_VIEWER_TITLE);
}

static void frame_delay(int speed) {
    for (volatile int i = 0; i < speed; ++i) {
        __asm__ volatile("nop");
    }
}

static int frame_wait_key(int total_delay) {
    const int chunk_size = 50000;
    int last_key = 0;
    int done = 0;

    while (done < total_delay) {
        int k = read_key();
        if (k) last_key = k;

        int chunk = chunk_size;
        if (done + chunk > total_delay) chunk = total_delay - done;
        frame_delay(chunk);
        done += chunk;
    }

    return last_key;
}

static void draw_center_message(const char* msg, unsigned char attr) {
    int len = kstrlen(msg);
    int x = (WIDTH - len) / 2;
    if (x < 0) x = 0;
    int y = HEIGHT / 2;
    for (int i = 0; i < len && x + i < WIDTH; ++i) {
        vga_putcell(x + i, y, msg[i], attr);
    }
}

#define PONG_FRAME_DELAY  90000000
#define DODGE_FRAME_DELAY 75000000
#define CATCH_FRAME_DELAY 80000000
#define DODGE_MAX_HITS    3
#define DODGE_HIT_RADIUS  1

void game_pong_run(void) {
restart_pong:
    int paddle_w = 12;
    int paddle_x = WIDTH / 2 - paddle_w / 2;
    int paddle_y = HEIGHT - 3;

    int ball_x = WIDTH / 2;
    int ball_y = HEIGHT / 2;
    int dx = 1;
    int dy = 1;

    int score = 0;
    int lives = 3;
    int game_over = 0;
    int k = 0;

    while (1) {
        if (k == K_ESC) break;
        if (k == 'r' || k == 'R') goto restart_pong;

        if (!game_over) {
            if (k == K_ARROW_LEFT || k == 'a' || k == 'A') paddle_x--;
            if (k == K_ARROW_RIGHT || k == 'd' || k == 'D') paddle_x++;

            if (paddle_x < 1) paddle_x = 1;
            if (paddle_x + paddle_w > WIDTH - 1) paddle_x = WIDTH - 1 - paddle_w;

            ball_x += dx;
            ball_y += dy;

            if (ball_x <= 1) { ball_x = 1; dx = 1; }
            if (ball_x >= WIDTH - 2) { ball_x = WIDTH - 2; dx = -1; }
            if (ball_y <= 2) { ball_y = 2; dy = 1; }

            if (ball_y == paddle_y - 1 && ball_x >= paddle_x && ball_x < paddle_x + paddle_w) {
                dy = -1;
                score += 10;
                if (ball_x < paddle_x + paddle_w / 3) dx = -1;
                else if (ball_x > paddle_x + (2 * paddle_w) / 3) dx = 1;
            }

            if (ball_y >= HEIGHT - 2) {
                lives--;
                if (lives <= 0) {
                    game_over = 1;
                } else {
                    ball_x = WIDTH / 2;
                    ball_y = HEIGHT / 2;
                    dx = (rnd_int(2) == 0) ? -1 : 1;
                    dy = -1;
                }
            }
        }

        vga_clear();
        draw_title("PONG  (A/D or arrows, R restart, ESC quit)", score, lives, "Lives:");

        for (int x = 0; x < WIDTH; ++x) {
            vga_putcell(x, 1, '-', ATTR_BORDER);
            vga_putcell(x, HEIGHT - 2, '-', ATTR_BORDER);
        }

        for (int i = 0; i < paddle_w; ++i) vga_putcell(paddle_x + i, paddle_y, '=', ATTR_PROMPT);
        vga_putcell(ball_x, ball_y, 'O', ATTR_SUCCESS);

        if (game_over) {
            draw_center_message("GAME OVER!  R to restart  ESC to exit", ATTR_ERROR);
            draw_footer("Out of lives. R restart / ESC return.");
        } else {
            draw_footer("Keep the ball alive. R restart / ESC return.");
        }
        vga_flush();
        k = frame_wait_key(PONG_FRAME_DELAY);
    }
}

void game_dodge_run(void) {
restart_dodge:
    #define MAX_OBS 28
    int ox[MAX_OBS];
    int oy[MAX_OBS];
    for (int i = 0; i < MAX_OBS; ++i) { ox[i] = 0; oy[i] = -1; }

    int px = WIDTH / 2;
    int py = HEIGHT - 3;
    int score = 0;
    int hits = 0;
    int spawn_tick = 0;
    int game_over = 0;
    int k = 0;

    while (1) {
        if (k == K_ESC) break;
        if (k == 'r' || k == 'R') goto restart_dodge;

        if (!game_over) {
            if (k == K_ARROW_LEFT || k == 'a' || k == 'A') px--;
            if (k == K_ARROW_RIGHT || k == 'd' || k == 'D') px++;
            if (px < 1) px = 1;
            if (px > WIDTH - 2) px = WIDTH - 2;

            spawn_tick++;
            if (spawn_tick >= 6) {
                spawn_tick = 0;
                for (int i = 0; i < MAX_OBS; ++i) {
                    if (oy[i] < 0) {
                        ox[i] = 1 + rnd_int(WIDTH - 2);
                        oy[i] = 2;
                        break;
                    }
                }
            }

            for (int i = 0; i < MAX_OBS; ++i) {
                if (oy[i] >= 0) {
                    oy[i]++;
                    if (oy[i] > HEIGHT - 2) oy[i] = -1;
                    if (oy[i] == py &&
                        ox[i] >= px - DODGE_HIT_RADIUS &&
                        ox[i] <= px + DODGE_HIT_RADIUS) {
                        hits++;
                        oy[i] = -1;
                    }
                }
            }

            if (hits >= DODGE_MAX_HITS) game_over = 1;
            if (!game_over) score++;
        }

        vga_clear();
        draw_title("DODGE  (A/D or arrows, R restart, ESC quit)", score, DODGE_MAX_HITS - hits, "HP:");
        for (int x = 0; x < WIDTH; ++x) {
            vga_putcell(x, 1, '-', ATTR_BORDER);
            vga_putcell(x, HEIGHT - 2, '-', ATTR_BORDER);
        }

        for (int i = 0; i < MAX_OBS; ++i) {
            if (oy[i] >= 2 && oy[i] < HEIGHT - 2) vga_putcell(ox[i], oy[i], '*', ATTR_ERROR);
        }

        vga_putcell(px, py, 'A', ATTR_PROMPT);
        if (game_over) {
            draw_center_message("GAME OVER!  R to restart  ESC to exit", ATTR_ERROR);
            draw_footer("Hit limit reached. R restart / ESC return.");
        } else {
            draw_footer("Avoid falling stars. Hit limit: 3. R restart / ESC return.");
        }
        vga_flush();
        k = frame_wait_key(DODGE_FRAME_DELAY);
    }
}

void game_catch_run(void) {
restart_catch:
    int basket_w = 9;
    int bx = WIDTH / 2 - basket_w / 2;
    int by = HEIGHT - 3;

    int fx = 1 + rnd_int(WIDTH - 2);
    int fy = 2;

    int score = 0;
    int misses = 0;
    int game_over = 0;
    int k = 0;

    while (1) {
        if (k == K_ESC) break;
        if (k == 'r' || k == 'R') goto restart_catch;

        if (!game_over) {
            if (k == K_ARROW_LEFT || k == 'a' || k == 'A') bx--;
            if (k == K_ARROW_RIGHT || k == 'd' || k == 'D') bx++;
            if (bx < 1) bx = 1;
            if (bx + basket_w > WIDTH - 1) bx = WIDTH - 1 - basket_w;

            fy++;
            if (fy >= by) {
                if (fx >= bx && fx < bx + basket_w) score += 5;
                else misses++;
                fx = 1 + rnd_int(WIDTH - 2);
                fy = 2;
            }

            if (misses >= 5) game_over = 1;
        }

        vga_clear();
        draw_title("CATCH  (A/D or arrows, R restart, ESC quit)", score, 5 - misses, "Miss:");
        for (int x = 0; x < WIDTH; ++x) {
            vga_putcell(x, 1, '-', ATTR_BORDER);
            vga_putcell(x, HEIGHT - 2, '-', ATTR_BORDER);
        }

        vga_putcell(fx, fy, '$', ATTR_SUCCESS);
        for (int i = 0; i < basket_w; ++i) vga_putcell(bx + i, by, '_', ATTR_PROMPT);
        if (game_over) {
            draw_center_message("GAME OVER!  R to restart  ESC to exit", ATTR_ERROR);
            draw_footer("5 misses reached. R restart / ESC return.");
        } else {
            draw_footer("Catch the coins. R restart / ESC return.");
        }
        vga_flush();
        k = frame_wait_key(CATCH_FRAME_DELAY);
    }
}
