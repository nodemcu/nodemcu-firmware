#ifndef _OVERRIDE_STDIO_H_
#define _OVERRIDE_STDIO_H_

#include_next "stdio.h"

#ifdef __BUFSIZ__
# define BUFSIZ __BUFSIZ__
#else
# define BUFSIZ 1024
#endif

extern void output_redirect(const char *str, size_t l);
#define puts(s)  output_redirect((s), strlen(s))

#define printf(...) do { \
  char __printf_buf[BUFSIZ]; \
  snprintf(__printf_buf, BUFSIZ, __VA_ARGS__); \
  puts(__printf_buf); \
} while(0)


#endif
