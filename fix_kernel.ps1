$c = Get-Content -Raw src/kernel.c; $i = $c.IndexOf('        if (current_mode == MODE_BROWSER) {
            /* Delegate browser/key handling'); $e = $c.IndexOf('/* Short delay to keep input responsive but prevent tearing/super fast games */'); $n = $c.Substring(0, $i) + "        sys_app_t* act = app_get_active();
        if (act) {
            if (k) {
                int status = app_active_handle_key(k);
                if (status == APP_STATUS_EXIT) {
                    input_reset_modifiers();
                    ui_draw();
                    continue;
                } else if (status == APP_STATUS_RESTART) {
                    game_timer = 0;
                }
            }
            game_timer++;
            if (game_timer >= game_speed) {
                game_timer = 0;
                app_active_update();
                app_active_draw();
            }
        } else {
            if (current_mode == MODE_BROWSER) {
                explorer_sel = shell_loop(explorer_sel, &current_mode, k);
            }
        }

        " + $c.Substring($e); Set-Content src/kernel.c -Value $n