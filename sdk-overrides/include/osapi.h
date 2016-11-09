#ifndef _SDK_OVERRIDE_OSAPI_H_
#define _SDK_OVERRIDE_OSAPI_H_

#include "rom.h"
void ets_timer_arm_new (ETSTimer *a, int b, int c, int isMstimer);

int atoi(const char *nptr);
int os_printf(const char *format, ...) __attribute__ ((format (printf, 1, 2)));
int os_printf_plus(const char *format, ...)  __attribute__ ((format (printf, 1, 2)));

unsigned int uart_baudrate_detect(unsigned int uart_no, unsigned int async);

void NmiTimSetFunc(void (*func)(void));

void call_user_start(void);

#include_next "osapi.h"

extern void sw_timer_register(void* timer_ptr);
#undef os_timer_arm
#define os_timer_arm(timer_ptr, duration, mode) do{sw_timer_register(timer_ptr); \
  ets_timer_arm_new(timer_ptr, duration, mode, 1);}while(0);

extern void sw_timer_unregister(void* timer_ptr);
#undef os_timer_disarm
#define os_timer_disarm(timer_ptr) do{sw_timer_unregister(timer_ptr); ets_timer_disarm(timer_ptr);}while(0);


#endif
