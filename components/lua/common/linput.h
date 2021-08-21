#ifndef READLINE_APP_H
#define READLINE_APP_H

extern void input_setup(int bufsize, const char *prompt);
extern void input_setprompt (const char *prompt);

void lua_handle_input(void);

#endif /* READLINE_APP_H */
