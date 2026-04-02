with open('src/editor.c', 'r') as f:
    text = f.read()

text = text.replace(
    '''        for (int x = 0; x < VIEW_W; ++x) vga_putcell(1 + x, 1 + ln, ' ', 0x07);''',
    '''        for (int x = 0; x < VIEW_W; ++x) vga_putcell(1 + x, 1 + ln, ' ', 0x0F);'''
)
text = text.replace(
    '''        draw_text_in_win(0, 1, WIDTH, HEIGHT - 1, 0, ln, linebuf, 0x07);''',
    '''        draw_text_in_win(0, 1, WIDTH, HEIGHT - 1, 0, ln, linebuf, 0x0F);'''
)
text = text.replace(
    '''        vga_putcell(1 + screen_col, 1 + cursor_screen_line, cur_ch, 0x70);''',
    '''        vga_putcell(1 + screen_col, 1 + cursor_screen_line, cur_ch, 0xF0);'''
)

old_save = '''    if (key == CTRL_S) {
        if (editor_file_index != -1) {
            struct File *f = fs_get(editor_file_index);
            fs_write(f->name, editor_buffer);
            editor_modified = 0;
            /* Briefly show "Saved!" on the status bar (no blocking read). */   
            const char *msg = "Saved!";
            for (int x = 0; x < WIDTH; ++x) vga_putcell(x, HEIGHT - 1, ' ', 0x07);
            for (int i = 0; msg[i]; ++i)
                vga_putcell(1 + i, HEIGHT - 1, msg[i], 0x0A);
        }
        return;
    }'''

new_save = '''    if (key == CTRL_S) {
        if (editor_file_index != -1) {
            struct File *f = fs_get(editor_file_index);
            int res = fs_write(f->name, editor_buffer);
            if (res == FS_ERR_RDONLY) {
                const char *msg = "Error: System files are read-only!";
                for (int x = 0; x < WIDTH; ++x) vga_putcell(x, HEIGHT - 1, ' ', 0x07);
                for (int i = 0; msg[i]; ++i)
                    vga_putcell(1 + i, HEIGHT - 1, msg[i], 0x0C);
            } else {
                editor_modified = 0;
                /* Briefly show "Saved!" on the status bar (no blocking read). */   
                const char *msg = "Saved!";
                for (int x = 0; x < WIDTH; ++x) vga_putcell(x, HEIGHT - 1, ' ', 0x07);
                for (int i = 0; msg[i]; ++i)
                    vga_putcell(1 + i, HEIGHT - 1, msg[i], 0x0A);
            }
        }
        return;
    }'''

text = text.replace(old_save, new_save)

with open('src/editor.c', 'w') as f:
    f.write(text)
