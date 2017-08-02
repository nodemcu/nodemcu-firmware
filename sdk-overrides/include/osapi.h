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

#ifdef TIMER_SUSPEND_ENABLE
extern void swtmr_cb_register(void* timer_cb_ptr);

#undef os_timer_setfn
#define os_timer_setfn(ptimer, pfunction, parg) \
  do{ \
    ets_timer_setfn(ptimer, pfunction, parg); \
    swtmr_cb_register(pfunction); \
  }while(0);

#endif

#endif
