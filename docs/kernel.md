---
layout: default
title: Kernel Module
---

# Kernel (`src/kernel.c`)

Key functions:

- `handle_command_input(explorer_sel, mode)` — Read a command line from the status prompt and execute simple commands. May change `mode` to enter the editor or game and returns the updated selection index.
- `kernel_main()` — Kernel entry and main loop: initialize subsystems, draw UI, and process input/events across browser, editor, and game modes.

Source: [src/kernel.c](../src/kernel.c)
