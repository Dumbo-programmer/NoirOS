---
layout: default
title: Mouse Module
---

# Mouse (`src/mouse.c`)

Functions and behavior:

- `inb(port)`, `outb(port, val)` — Low-level port I/O helpers used by the mouse code.
- `mouse_wait_input()` / `mouse_wait_output()` — Wait for PS/2 controller readiness (timeout-protected to avoid hangs).
- `mouse_write(data)`, `mouse_read()` — Send/read bytes to/from the PS/2 mouse (returns 0 on timeout).
- `init_mouse()` — Initialize the PS/2 mouse, enable data reporting, and centre the cursor on screen.
- `mouse_handler()` — Interrupt handler that assembles 3-byte packets, updates `mouse` state, and clamps position to screen bounds.
- `get_mouse_state()` — Return pointer to global `mouse_state_t`.

Source: [src/mouse.c](../src/mouse.c)
