with open('src/input.c', 'a') as f:
    f.write('''
int wait_key(void) {
    int key;
    do {
        key = read_key();
        for (volatile int nop = 0; nop < 500000; nop++);
    } while (!key);
    return key;
}

void input_reset_modifiers(void) {
    kb_state.shift_pressed = 0;
    kb_state.ctrl_pressed  = 0;
    kb_state.alt_pressed   = 0;
    kb_state.caps_lock_active = 0;
}
''')
