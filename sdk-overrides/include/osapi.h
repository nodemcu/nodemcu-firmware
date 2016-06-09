#ifndef _SDK_OVERRIDE_OSAPI_H_
#define _SDK_OVERRIDE_OSAPI_H_

#include "user_interface.h"
#include "user_config.h"

void call_user_start(void);

#define os_memcmp   memcmp
#define os_memcpy   memcpy
#define os_memmove  memmove
#define os_memset   memset
#define os_strcat   strcat
#define os_strchr   strchr
#define os_strcmp   strcmp
#define os_strcpy   strcpy
#define os_strlen   strlen
#define os_strncmp  strncmp
#define os_strncpy  strncpy
#define os_strstr   strstr
#define os_sprintf  sprintf
#define os_printf_plus printf

#include <string.h>

#endif
