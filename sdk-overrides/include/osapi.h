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

#ifdef ENABLE_TIMER_SUSPEND
extern void swtmr_register(void* timer_ptr);
#undef os_timer_arm
#define os_timer_arm(timer_ptr, duration, mode) do{swtmr_register(timer_ptr); \
  ets_timer_arm_new(timer_ptr, duration, mode, 1);}while(0);

extern void swtmr_unregister(void* timer_ptr);
#undef os_timer_disarm
#define os_timer_disarm(timer_ptr) do{swtmr_unregister(timer_ptr); ets_timer_disarm(timer_ptr);}while(0);
#endif

#endif
