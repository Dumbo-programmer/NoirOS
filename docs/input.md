---
layout: default
title: Input Module
---

# Input (`src/input.c`)

Functions and documentation extracted from source:

- `inb(port)` — Read a byte from an I/O port. Returns the data byte read from the port.

- `kb_read_scancode()` — Read one scancode from the keyboard controller with a timeout; returns 0 on timeout to avoid blocking when hardware is absent.

- `read_key()` — Translate raw scancodes into logical key values. Handles E0-prefixed extended scancodes, key releases, and modifier state (shift/ctrl/alt/caps). Returns ASCII, control codes, or special constants (K_ARROW_*, K_F1..).

- `is_shift_pressed()`, `is_ctrl_pressed()`, `is_alt_pressed()`, `is_caps_lock_on()` — Query modifier and lock states.

- `input_readline(buf, max)` — Read a line of text into `buf` (NUL-terminated). Handles backspace and simple editing; blocks until Enter.

Source: [src/input.c](../src/input.c)
