with open('src/shell.c', 'r') as f:
    text = f.read()

old_edit = '''static int cmd_edit(const char* args, int* mode, int* explorer_sel) {
    (void)explorer_sel;
    if (!args || !args[0]) { show_error("Usage: edit <filename>"); return 0; }  
    editor_open(args, mode);
    return 1;
}'''

new_edit = '''static int cmd_edit(const char* args, int* mode, int* explorer_sel) {
    (void)explorer_sel;
    if (!args || !args[0]) { show_error("Usage: edit <filename>"); return 0; }  
    struct File* f = fs_find(args);
    if (f && f->readonly) {
        show_error("Read-only files cannot be opened");
        return 0;
    }
    editor_open(args, mode);
    return 1;
}'''

text = text.replace(old_edit, new_edit)
with open('src/shell.c', 'w') as f:
    f.write(text)
