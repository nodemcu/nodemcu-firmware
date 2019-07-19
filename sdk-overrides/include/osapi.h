#ifndef _SDK_OVERRIDE_OSAPI_H_
#define _SDK_OVERRIDE_OSAPI_H_

#include_next "osapi.h"

#include <stddef.h>
#include "rom.h"

unsigned int uart_baudrate_detect(unsigned int uart_no, unsigned int async);

void NmiTimSetFunc(void (*func)(void));

void call_user_start(void);

#endif
