---
layout: default
title: Editor Module
---

# Editor (`src/editor.c`)

Key functions and behavior:

- `get_line_start(line)` — Return buffer offset for the start of logical line `line`.
- `get_line_length_at_off(off)` — Return number of characters on the line starting at buffer offset `off` (excludes newline).
- `cursor_to_offset()` — Convert `editor_cursor_x`/`editor_cursor_y` to a buffer offset.
- `insert_char_at(off, ch)` — Insert `ch` at buffer offset `off`, shifting contents and updating state.
- `delete_char_before(off)` — Delete character before offset `off` (backspace semantics).
- `editor_draw()` — Redraw title bar, visible text window, cursor and status bar.
- `editor_open(fname, mode)` — Load `fname` into the editor buffer and switch to editor mode.
- `editor_handle_key(key, mode)` — Handle navigation, editing, and save/exit shortcuts (Ctrl+S/Ctrl+X).
- `editor_set_cursor_pos(x, y)` — (optional, `EDITOR_MOUSE_SUPPORT`) Map a mouse click to editor cursor coordinates and clamp safely.

Source: [src/editor.c](../src/editor.c)
