#ifndef _OVERRIDE_STDLIB_H_
#define _OVERRIDE_STDLIB_H_

#include_next "stdlib.h"
#include <stdbool.h>
#include "mem.h"

#define free          os_free
#define malloc        os_malloc
#define calloc(n,sz)  os_zalloc(n*sz)
#define realloc       os_realloc

#endif
