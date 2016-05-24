#ifndef _SDK_OVERRIDES_ESP_SYSTEM_H_
#define _SDK_OVERRIDES_ESP_SYSTEM_H_
void system_soft_wdt_feed(void);
#include_next "esp_system.h"
#endif
