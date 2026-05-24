import sys

with open('docs/kernel.md', 'r', encoding='utf-8') as f:
    text = f.read()

text = text.replace('- Routes to module-specific handlers based on current mode:\n        - Browser mode -> shell_loop(...)\n        - Editor mode -> editor_handle_key(...)\n        - Game mode -> snake update/draw + restart/exit handling', '- Routes to module-specific handlers based on current mode:\n        - App Context -> Routes to active app via pp_active_handle_key\n        - Browser mode -> shell_loop(...)')
text = text.replace('### Game Timing\n\n- Snake uses a tick counter (game_timer) and game_speed threshold.', '### App Timing\n\n- Active apps use a tick counter (game_timer) and game_speed threshold, calling pp_active_update() and pp_active_draw().')
text = text.replace('## Mode Model\n\n- MODE_BROWSER: file browser + command panel access\n- MODE_EDITOR: text editing session\n- MODE_GAME: snake runtime loop\n\nThe kernel returns to browser mode on mode exit or Esc paths.', '## Application Model\n\n- NoirOS now uses an app registry sys_app_t to dynamically launch apps like editor and snake.\n- MODE_BROWSER acts as the desktop idle state.\n\nThe kernel returns to browser mode whenever the active app returns APP_STATUS_EXIT.')

with open('docs/kernel.md', 'w', encoding='utf-8') as f:
    f.write(text)

