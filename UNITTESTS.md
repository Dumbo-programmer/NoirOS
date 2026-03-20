# NoirOS Unit Test Plan

## Overview
This file describes basic unit tests for core NoirOS modules. Since NoirOS is a freestanding OS, tests are written as C functions to be run in a test mode or via a test harness.

## Test Functions

### File: src/fs.c
- `test_fs_init()`: Verify root directory and sample files are created.
- `test_fs_create_file()`: Create a file, check name/content, verify count.
- `test_fs_delete_file()`: Delete a file, verify count decreases.
- `test_fs_create_dir()`: Create a directory, check name, verify count.
- `test_fs_delete_dir()`: Delete a directory, verify count decreases.

### File: src/util.c
- `test_kstrncpy()`: Copy string, check NUL termination.
- `test_kstrcmp()`: Compare strings, check result.
- `test_kstrlen()`: Check length for various strings.

### File: src/ui.c
- `test_ui_set_selected()`: Set selection, verify bounds.
- `test_ui_scroll_viewer()`: Scroll, verify limits.

### File: src/kernel.c
- `test_handle_command_input()`: Simulate command input, verify mode changes and explorer selection.

## Example Test Function (C)
```c
void test_kstrncpy() {
    char dest[10];
    kstrncpy(dest, "abc", 10);
    assert(dest[0] == 'a' && dest[1] == 'b' && dest[2] == 'c' && dest[3] == '\0');
}
```

## Running Tests
- Add a test mode to kernel.c (e.g., `#ifdef TEST_MODE`) to call test functions at boot.
- Print results to VGA or serial output.
- Use assert macros for validation.

## Notes
- Tests should not interfere with normal OS operation.
- Keep tests simple and self-contained.
- Expand test coverage as modules grow.
