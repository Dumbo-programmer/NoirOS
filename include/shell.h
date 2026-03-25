#ifndef SHELL_H
#define SHELL_H

/* Called every kernel loop while in browser mode.
 * explorer_sel: current selected index in the explorer
 * mode pointer to current_mode (so shell can switch to MODE_EDITOR / MODE_GAME)
 * Returns updated explorer_sel.
 */
int shell_loop(int explorer_sel, int *mode, int first_key);
/* Open the command prompt (blocking) and execute the entered command. */
int shell_open_prompt(int explorer_sel, int *mode);

#endif 
