---
layout: default
title: API Reference
---

# NoirOS API Reference

This page collects the Doxygen-style function documentation extracted from the source tree. Each section links to the implementation file for quick navigation.

- Source: [src/input.c](src/input.c)
- Source: [src/editor.c](src/editor.c)
- Source: [src/fs.c](src/fs.c)
- Source: [src/game_snake.c](src/game_snake.c)
- Source: [src/kernel.c](src/kernel.c)
- Source: [src/mouse.c](src/mouse.c)
- Source: [src/util.c](src/util.c)
- Source: [src/vga.c](src/vga.c)

---

## Input (`src/input.c`)

- `inb(port)` — Read a byte from an I/O port. Returns the data byte read from the port.
- `kb_read_scancode()` — Read one scancode from the keyboard controller with timeout; returns 0 on timeout.
- `read_key()` — Translate raw scancodes into logical key values, handling modifiers and extended scancodes.
- `is_shift_pressed()`, `is_ctrl_pressed()`, `is_alt_pressed()`, `is_caps_lock_on()` — Query modifier/lock states.
- `input_readline(buf, max)` — Read a line of text into `buf` (NUL-terminated); handles basic editing and returns length.

## Editor (`src/editor.c`)

- `get_line_start(line)` — Return buffer offset of the start of logical line `line`.
- `get_line_length_at_off(off)` — Return number of characters on the line starting at buffer offset `off`.
- `cursor_to_offset()` — Convert cursor (x,y) to buffer offset.
- `insert_char_at(off, ch)` — Insert character at buffer offset `off`.
- `delete_char_before(off)` — Delete the character before offset `off`.
- `editor_draw()` — Redraw the editor UI (title, text window, cursor, status).
- `editor_open(fname, mode)` — Open `fname` into the editor buffer and switch to editor mode.
- `editor_handle_key(key, mode)` — Handle a single keypress in editor mode (navigation, editing, save/exit).
- `editor_set_cursor_pos(x, y)` — (optional, `EDITOR_MOUSE_SUPPORT`) Set editor cursor from mouse click coordinates.

## Filesystem (`src/fs.c`)

- `name_invalid(n)` — Validate a filename component; returns non-zero if invalid.
- `dir_find_child(d, name)` — Find a child directory by name.
- `dir_is_empty(d)` — Check whether a directory has no files/subdirs.
- `pool_index_of(d)` — Return index in directory pool for pointer `d`, or -1.
- `init_filesystem()` — Initialize the in-memory filesystem and preload sample files.
- `fs_root()` — Return pointer to the filesystem root directory.
- `fs_cwd()` — Return pointer to current working directory.
- `fs_pwd(out, out_len)` — Write current working directory path into `out`.
- `fs_mkdir(name)` — Create a new directory in CWD; returns `FS_OK` or error.
- `fs_chdir(name)` — Change current working directory to `name`/`..`/`/`.
- `fs_rmdir(name)` — Remove an empty subdirectory and free its pool slot.
- `fs_dir_count()`, `fs_dir_get(idx)`, `fs_find_dir(name)` — Directory listing helpers.
- `fs_count()`, `fs_get(idx)`, `fs_find(name)` — File listing helpers.
- `fs_create(name, type)`, `fs_delete(name)` — Create and delete files in CWD.
- `fs_write(name, data)`, `fs_append(name, data)` — Write/append file contents.
- `fs_list_counts(out_dirs, out_files)` — Return dir/file counts for CWD.

## Game: Snake (`src/game_snake.c`)

- `snake_init()` — Initialize snake game state to starting values and place food.
- `place_food()` — (static) Place food at an unoccupied position; deterministic sequence, fallback scan.
- `snake_update()` — Advance game state by one tick: move snake, check collisions, handle food.
- `snake_draw()` — Render the current snake game state to the screen.
- `snake_handle_key(k)` — Handle key events in game; returns 1 if ESC pressed to exit.

## Kernel (`src/kernel.c`)

- `handle_command_input(explorer_sel, mode)` — Read a command line from the status prompt and execute simple commands; may switch modes.
- `kernel_main()` — Kernel main loop: initialize systems and process events (browser/editor/game modes).

## Mouse (`src/mouse.c`)

- `inb(port)`, `outb(port, val)` — Port I/O helpers used by the mouse subsystem.
- `mouse_wait_input()` — Wait for PS/2 controller input buffer to be clear (timeout-protected).
- `mouse_wait_output()` — Wait for PS/2 controller output buffer to have data (timeout-protected).
- `mouse_write(data)` — Send a byte to the PS/2 mouse device.
- `mouse_read()` — Read a byte from PS/2 data port (returns 0 on timeout).
- `init_mouse()` — Initialize the PS/2 mouse device and enable data reporting; centers cursor on screen.
- `mouse_handler()` — Interrupt handler: collect 3-byte packets and update global `mouse` state; clamps to screen bounds.
- `get_mouse_state()` — Return pointer to global `mouse_state_t`.

## Utilities (`src/util.c`)

- `kstrcmp(a,b)` — Lexicographic string comparison.
- `kstrncmp(a,b,n)` — Compare up to `n` characters.
- `kstrcpy(dst,src)` — Unbounded copy (use only when `dst` is large enough).
- `kstrncpy(dest,src,n)` — Copy at most `n-1` bytes and NUL-terminate; no-op if `n<=0`.
- `kstrlen(s)` — Return length of NUL-terminated string.

## VGA / Drawing (`src/vga.c`)

- `vga_putcell(x,y,ch,attr)` — Write a character cell to VGA text buffer; bounds-checked.
- `vga_clear()` — Clear the VGA text screen and reset cursor.
- `term_putc(c)` — Terminal character output with newline/tab/scroll handling.
- `term_write(s)` — Write a NUL-terminated string to the terminal.
- `draw_box(x,y,w,h,title,...)` — Draw a framed box with optional title and background attr.
- `draw_text_in_win(x,y,w,h,wx,wy,text,attr)` — Draw text within a window region with wrapping.
- `vga_getcell_char(x,y)`, `vga_getcell_attr(x,y)` — Read character/attribute from a screen cell.

---

This page is a compact API summary. The full per-module pages link to their source and include each function's documentation.
