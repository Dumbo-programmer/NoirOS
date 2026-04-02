import re

with open('src/shell.c', 'r') as f:
    text = f.read()

pattern = r'static int cmd_edit\(const char\* args, int\* mode, int\* explorer_sel\) \{.*?\n\}'
new_code = '''static int cmd_edit(const char* args, int* mode, int* explorer_sel) {
    (void)explorer_sel;
    if (!args || !args[0]) { show_error("Usage: edit <filename>"); return 0; }  
    struct File* f = fs_find(args);
    if (f && f->readonly) {
        show_error("File is read-only and cannot be opened");
        return 0;
    }
    editor_open(args, mode);
    return 1;
}'''

text = re.sub(pattern, new_code, text, flags=re.DOTALL)

with open('src/shell.c', 'w') as f:
    f.write(text)
