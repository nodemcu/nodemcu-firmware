/******************************************************************************
 * WDT api for NodeMCU
 * NodeMCU Team
 * 2018-04-04
*******************************************************************************/

#include "platform.h"
#include "esp_task_wdt.h"

#ifdef CONFIG_TASK_WDT
static uint32_t task_wdt_timeout = CONFIG_TASK_WDT_TIMEOUT_S;

static bool task_wdt_panic =
#ifdef CONFIG_TASK_WDT_PANIC
  true;
#else
false;
#endif

#endif


int platform_wdt_feed( void )
{
#ifdef CONFIG_TASK_WDT
  return esp_task_wdt_init(task_wdt_timeout, task_wdt_panic) == ESP_OK ? PLATFORM_OK : PLATFORM_ERR;
#else
  return PLATFORM_OK;
#endif
}
