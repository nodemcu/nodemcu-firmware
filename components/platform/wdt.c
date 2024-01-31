/******************************************************************************
 * WDT api for NodeMCU
 * NodeMCU Team
 * 2018-04-04
*******************************************************************************/

#include "platform.h"
#include "esp_task_wdt.h"

int platform_wdt_feed( void )
{
#ifdef CONFIG_TASK_WDT
  esp_task_wdt_config_t cfg = {
    .timeout_ms = CONFIG_TASK_WDT_TIMEOUT_S * 1000,
    .idle_core_mask = (uint32_t)-1,
    .trigger_panic =
#ifdef CONFIG_TASK_WDT_PANIC
      true,
#else
      false,
#endif
  };
  return esp_task_wdt_init(&cfg) == ESP_OK ? PLATFORM_OK : PLATFORM_ERR;
#else
  return PLATFORM_OK;
#endif
}
