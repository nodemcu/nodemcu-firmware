#ifndef SDK_OVERRIDES_INCLUDE_ETS_SYS_H_
#define SDK_OVERRIDES_INCLUDE_ETS_SYS_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include_next "ets_sys.h"

#include <stdarg.h>

int ets_sprintf(char *str, const char *format, ...)  __attribute__ ((format (printf, 2, 3)));

int ets_vsprintf (char *d, const char *s, va_list ap);

extern ETSTimer *timer_list;

#endif /* SDK_OVERRIDES_INCLUDE_ETS_SYS_H_ */
