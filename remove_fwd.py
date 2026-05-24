import sys
with open('src/kernel.c', 'r', encoding='utf-8') as f:
    lines = f.readlines()
new_lines = [l for l in lines if not ('snake_init(' in l or 'snake_update(' in l or 'snake_draw(' in l or 'snake_handle_key(' in l)]
with open('src/kernel.c', 'w', encoding='utf-8') as f:
    f.writelines(new_lines)
print("Removed snake forward declarations")
