import sys

# shell.c
with open('src/shell.c', 'r', encoding='utf-8') as f:
    sh = f.read()

sh = sh.replace('editor_open(args, mode);', 'app_launch("editor", args);')
sh = sh.replace('static int cmd_snake(const char* args, int* mode, int* explorer_sel) {\n    (void)args; (void)explorer_sel;\n    *mode = MODE_GAME;\n    snake_init();\n    snake_draw();\n    return 1;\n}', 'static int cmd_snake(const char* args, int* mode, int* explorer_sel) {\n    (void)args; (void)explorer_sel; (void)mode;\n    app_launch("snake", 0);\n    return 1;\n}')
sh = sh.replace('static int cmd_edit(const char* args, int* mode, int* explorer_sel) {\n    (void)explorer_sel;', 'static int cmd_edit(const char* args, int* mode, int* explorer_sel) {\n    (void)explorer_sel; (void)mode;')

with open('src/shell.c', 'w', encoding='utf-8') as f:
    f.write(sh)


# editor.c
with open('src/editor.c', 'r', encoding='utf-8') as f:
    ed = f.read()

ed = ed.replace('void editor_open(const char* fname, int* mode)', 'void editor_open(const char* fname)')
ed = ed.replace('    *mode = MODE_EDITOR;\n    editor_draw();\n}', '    editor_draw();\n}\n\nstatic void app_editor_init(const char* args) {\n    editor_open(args);\n}\n')
ed = ed.replace('void editor_handle_key(int key, int* mode)', 'int editor_handle_key(int key)')

idx1 = ed.find('int editor_handle_key(int key)')
idx2 = ed.find('#ifdef EDITOR_MOUSE_SUPPORT')

ed_body = ed[idx1:idx2]
ed_body = ed_body.replace('return;', 'return APP_STATUS_RUNNING;')
ed_body = ed_body.replace('*mode = MODE_BROWSER;\n            return APP_STATUS_RUNNING;', 'return APP_STATUS_EXIT;')

last_brace = ed_body.rfind('}')
ed_body = ed_body[:last_brace] + "    return APP_STATUS_RUNNING;\n}\n" + ed_body[last_brace+1:]

ed = ed[:idx1] + ed_body + ed[idx2:]
ed += "\nsys_app_t app_editor = { \"editor\", app_editor_init, 0, editor_draw, editor_handle_key };\n"

with open('src/editor.c', 'w', encoding='utf-8') as f:
    f.write(ed)

print("python script complete")
