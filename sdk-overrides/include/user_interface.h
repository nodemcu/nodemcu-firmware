#ifndef SDK_OVERRIDES_INCLUDE_USER_INTERFACE_H_
#define SDK_OVERRIDES_INCLUDE_USER_INTERFACE_H_

#include <espressif/esp_system.h>

/* The SDKs claim ets_delay_us only takes a uint16_t, not a uint32_t. Until
 * we have cleaned up our act and stopped using excessive hard-delays, we
 * need our own prototype for it.
 *
 * Also, our ets_printf() prototype is better.
 */
#define ets_delay_us ets_delay_us_sdk
#define ets_printf ets_print_sdk
#include <espressif/esp_misc.h>
#undef ets_printf
#undef ets_delay_us
#include "rom.h"

#include <espressif/esp_wifi.h>
#include <espressif/esp_sta.h>
#include <espressif/esp_softap.h>
#include <espressif/esp_timer.h>

static inline void system_set_os_print(uint8_t onoff) {
  extern uint8_t os_printf_enabled;
  os_printf_enabled = onoff;
}

bool wifi_softap_deauth(uint8 mac[6]);

#endif /* SDK_OVERRIDES_INCLUDE_USER_INTERFACE_H_ */
