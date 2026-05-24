import sys

with open('src/kernel.c', 'r', encoding='utf-8') as f:
    text = f.read()

text = text.replace('    init_filesystem();', '    init_filesystem();\n    app_register(&app_editor);\n    app_register(&app_snake);\n')

with open('src/kernel.c', 'w', encoding='utf-8') as f:
    f.write(text)

