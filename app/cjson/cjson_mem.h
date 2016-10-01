#ifndef _CJSON_MEM_H_
#define _CJSON_MEM_H_

#include "../lua/lua.h"

void *cjson_mem_malloc (uint32_t sz);
void *cjson_mem_realloc (void *p, uint32_t sz);

#endif
