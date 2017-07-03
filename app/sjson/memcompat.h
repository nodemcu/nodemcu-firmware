#ifndef __MEMCOMPAT_H__
#define __MEMCOMPAT_H__

#include "c_types.h"
#include "mem.h"

static inline void *malloc(size_t sz) { return os_malloc(sz); }
static inline void free(void *p) { return os_free(p); }
static inline void *calloc(size_t n, size_t sz) { return os_zalloc(n*sz); }

#endif
