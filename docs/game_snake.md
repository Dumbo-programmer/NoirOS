---
layout: default
title: Snake Game Module
---

# Game: Snake (`src/game_snake.c`)

Functions:

- `snake_init()` — Initialize game state, starting length/direction, score, and place food.
- `snake_update()` — Advance the game by one tick: move the snake, detect wall/self collisions, handle food collection and growth.
- `snake_draw()` — Render the playfield, snake, food, and score to the screen.
- `snake_handle_key(k)` — Handle key input during the game; prevents direct reversal and returns 1 on ESC to exit.

Source: [src/game_snake.c](../src/game_snake.c)
