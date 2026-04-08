---
layout: default
title: Kernel Module
---

# Kernel (`src/kernel.c`)

## What `kernel_main()` Does

`kernel_main()` is the orchestrator of NoirOS runtime behavior.

### Startup Sequence

1. Initialize serial, memory, IDT, input, mouse, and filesystem.
2. Compute UI layout.
3. Show boot loader splash.
4. Draw the main desktop UI.

### Main Event Loop

- Reads keyboard input (`read_key()`).
- Handles global browser hotkeys (`Tab`, `c/C`, `Esc`).
- Routes to module-specific handlers based on current mode:
	- Browser mode -> `shell_loop(...)`
	- Editor mode -> `editor_handle_key(...)`
	- Game mode -> snake update/draw + restart/exit handling

### Game Timing

- Snake uses a tick counter (`game_timer`) and `game_speed` threshold.
- Rendering is flushed regularly through VGA backbuffer logic.

### Debug Support

- Optional input debug overlay displays last scancode/key.
- Debug info is mirrored to serial output for headless troubleshooting.

## Mode Model

- `MODE_BROWSER`: file browser + command panel access
- `MODE_EDITOR`: text editing session
- `MODE_GAME`: snake runtime loop

The kernel returns to browser mode on mode exit or `Esc` paths.

Source: [kernel.c on GitHub](https://github.com/Dumbo-programmer/NoirOS/blob/main/src/kernel.c)
