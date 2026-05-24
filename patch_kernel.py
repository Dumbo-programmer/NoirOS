import sys

with open('src/kernel.c', 'r', encoding='utf-8') as f:
    lines = f.readlines()

new_lines = []
skip = False
for line in lines:
    if 'snake_init(' in line or 'snake_update(' in line or 'snake_draw(' in line or 'snake_handle_key(' in line:
        continue # remove old fwd declarations
    
    if skip:
        if '/* Short delay to keep input responsive but prevent tearing/super fast games */' in line:
            skip = False
            new_lines.append(line)
        continue

    if 'if (current_mode == MODE_BROWSER) {' in line and '/* Delegate browser/key handling to shell_loop' in lines[lines.index(line)+1]:
        skip = True
        new_lines.append("""        sys_app_t* act = app_get_active();
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

""")
    else:
        new_lines.append(line)

with open('src/kernel.c', 'w', encoding='utf-8') as f:
    f.writelines(new_lines)
print("done kernel")
