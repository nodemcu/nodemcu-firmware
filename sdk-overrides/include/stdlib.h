#ifndef _SDK_OVERRIDES_STDLIB_H_
#define _SDK_OVERRIDES_STDLIB_H_

extern const char *c_getenv (const char *);
extern double c_strtod(const char *string, char **endPtr);

#include_next <stdlib.h>
#endif
