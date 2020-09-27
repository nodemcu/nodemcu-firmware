/*
 * Wrapper around https://github.com/weiss/c99-snprintf
 */

#define HAVE_CONFIG_H 0
#define TEST_SNPRINTF 0
#define HAVE_SNPRINTF 0
#define HAVE_VSNPRINTF 0
#define HAVE_ASPRINTF 0
#define HAVE_VASPRINTF 0

#define HAVE_STDARG_H 1
#define HAVE_STDLIB_H 1

#define HAVE_VA_COPY 0
#define HAVE___VA_COPY 0
#define HAVE_FLOAT_H 1
#define HAVE_INTTYPES_H 1
#define HAVE_LOCALE_H 0
#define HAVE_STDDEF_H 1
#define HAVE_STDINT_H 1
#define HAVE_UNSIGNED_LONG_LONG_INT 1
#define HAVE_UINTMAX_T 1
#define HAVE_LONG_DOUBLE 0
#define HAVE_LONG_LONG_INT 1
#define HAVE_INTMAX_T 1
#define HAVE_UINTPTR_T 1
#define HAVE_PTRDIFF_T 1

#define HAVE_LOCALECONV 0 
#define HAVE_LCONV_DECIMAL_POINT 0
#define HAVE_LCONV_THOUSANDS_SEP 0
#define LDOUBLE_MIN_10_EXP DBL_MIN_10_EXP
#define LDOUBLE_MAX_10_EXP DBL_MAX_10_EXP

#define rpl_snprintf snprintf
#define rpl_vsnprintf vsnprintf
#define rpl_vasprintf vasprintf
#define rpl_asprintf asprintf

#include "c99-snprintf/snprintf.c"

int sprintf(char *s, const char *fmt, ...)
{
  int n;
  va_list arg;
  va_start(arg, fmt);
  n = vsnprintf(s, 1024, fmt, arg);
  va_end(arg);
  return n;
}

int vsprintf (char *d, const char *s, va_list ap)
{
  return vsnprintf(d, 1024, s, ap);
}
    

