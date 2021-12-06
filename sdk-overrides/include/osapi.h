#ifndef _SDK_OVERRIDE_OSAPI_H_
#define _SDK_OVERRIDE_OSAPI_H_

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#define USE_OPTIMIZE_PRINTF

#include_next "osapi.h"

#include "rom.h"

unsigned int uart_baudrate_detect(unsigned int uart_no, unsigned int async);

void NmiTimSetFunc(void (*func)(void));

void call_user_start(void);

#endif
