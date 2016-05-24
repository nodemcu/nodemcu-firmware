#ifndef _SDK_OVERRIDE_MEM_H_
#define _SDK_OVERRIDE_MEM_H_

#include "freertos/FreeRTOS.h"
void *pvPortZalloc (size_t sz);
void *pvPortRealloc (void *p, size_t sz);

#define os_zalloc pvPortZalloc
#define os_free   vPortFree
#define os_malloc pvPortMalloc

#include_next "lwip/mem.h"

#endif
