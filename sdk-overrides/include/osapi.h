#ifndef _SDK_OVERRIDE_OSAPI_H_
#define _SDK_OVERRIDE_OSAPI_H_

#include "rom.h"

int atoi(const char *nptr);
int os_printf(const char *format, ...) __attribute__ ((format (printf, 1, 2)));
int os_printf_plus(const char *format, ...)  __attribute__ ((format (printf, 1, 2)));

unsigned int uart_baudrate_detect(unsigned int uart_no, unsigned int async);

void NmiTimSetFunc(void (*func)(void));

void call_user_start(void);

#include_next "osapi.h"

#endif
