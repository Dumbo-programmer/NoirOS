import sys

with open('src/editor.c', 'r', encoding='utf-8') as f:
    text = f.read()

text = text.replace('\n#ifdef EDITOR_MOUSE_SUPPORT', '\n    return APP_STATUS_RUNNING;\n}\n\n#ifdef EDITOR_MOUSE_SUPPORT')

with open('src/editor.c', 'w', encoding='utf-8') as f:
    f.write(text)

with open('src/shell.c', 'r', encoding='utf-8') as f:
    text = f.read()

text = text.replace('static int cmd_edit(const char* args, int* mode, int* explorer_sel) {', 'static int cmd_edit(const char* args, int* mode, int* explorer_sel) {\n    (void)mode;\n')

with open('src/shell.c', 'w', encoding='utf-8') as f:
    f.write(text)

print("done")
