#ifndef READLINE_APP_H
#define READLINE_APP_H
typedef void (*uart_cb_t)(const char *buf, size_t len);

extern void input_setup(int bufsize, const char *prompt);
extern void input_setup_receive(uart_cb_t uart_on_data_cb, int data_len, char end_char, bool run_input);
extern void input_setecho (bool flag);
extern void input_setprompt (const char *prompt);

#endif /* READLINE_APP_H */
