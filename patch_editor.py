import sys

with open('src/editor.c', 'r', encoding='utf-8') as f:
    text = f.read()

# Replace open
text = text.replace('void editor_open(const char* fname, int* mode)', 'void editor_open(const char* fname)')
text = text.replace('    *mode = MODE_EDITOR;\n    editor_draw();\n}', '    editor_draw();\n}\n\nstatic void app_editor_init(const char* args) {\n    editor_open(args);\n}\n')

# replace handle_key signature
text = text.replace('void editor_handle_key(int key, int* mode)', 'int editor_handle_key(int key)')

# Find the start and end of editor_handle_key
idx1 = text.find('int editor_handle_key(int key)')
idx2 = text.find('#ifdef EDITOR_MOUSE_SUPPORT')

handle_key_text = text[idx1:idx2]

# Replace inside handle_key
handle_key_text = handle_key_text.replace('return;', 'return APP_STATUS_RUNNING;')
handle_key_text = handle_key_text.replace('*mode = MODE_BROWSER;\n            return APP_STATUS_RUNNING;', 'return APP_STATUS_EXIT;')

text = text[:idx1] + handle_key_text + text[idx2:]

text += "\n\nsys_app_t app_editor = { \"editor\", app_editor_init, 0, editor_draw, editor_handle_key };\n"

with open('src/editor.c', 'w', encoding='utf-8') as f:
    f.write(text)

print("Editor patched")
