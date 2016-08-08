#ifndef _SDK_OVERRIDE_MEM_H_
#define _SDK_OVERRIDE_MEM_H_

void *pvPortMalloc (size_t sz, const char *, unsigned);
void vPortFree (void *p, const char *, unsigned);
void *pvPortZalloc (size_t sz, const char *, unsigned);
void *pvPortRealloc (void *p, size_t n, const char *, unsigned);

#include_next "mem.h"

#endif
