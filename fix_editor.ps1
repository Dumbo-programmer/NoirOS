$content = Get-Content -Raw "src/editor.c"

$content = $content -replace '        return;
    }', "        return APP_STATUS_RUNNING;
    }"

$content = $content -replace '\*mode = MODE_BROWSER;\s*return APP_STATUS_RUNNING;', "return APP_STATUS_EXIT;"

$content += @"

sys_app_t app_editor = {
    "editor",
    app_editor_init,
    0,
    editor_draw,
    editor_handle_key
};
