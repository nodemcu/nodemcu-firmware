#ifndef _OVERRIDE_STDIO_H_
#define _OVERRIDE_STDIO_H_

#include_next "stdio.h"

#ifdef __BUFSIZ__
# define BUFSIZ __BUFSIZ__
#else
# define BUFSIZ 1024
#endif

#define printf(...) do { \
  unsigned char __printf_buf[BUFSIZ]; \
  sprintf(__printf_buf, __VA_ARGS__); \
  puts(__printf_buf); \
} while(0)

extern void output_redirect(const char *str);
#define puts output_redirect

#endif
