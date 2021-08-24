#ifndef _LINPUT_H_
#define _LINPUT_H_

#include <stdbool.h>
#include <stddef.h>

void input_setup(int bufsize, const char *prompt);
void input_setprompt (const char *prompt);

unsigned feed_lua_input(const char *buf, size_t n);

extern bool input_echo;
extern bool run_input;

#endif /* READLINE_APP_H */
