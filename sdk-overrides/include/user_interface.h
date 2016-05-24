#ifndef SDK_OVERRIDES_INCLUDE_USER_INTERFACE_H_
#define SDK_OVERRIDES_INCLUDE_USER_INTERFACE_H_

#define ets_timer_arm_new(tmr, ms, rpt, isms) os_timer_arm(tmr,ms,rpt)

//#include_next "user_interface.h"
#include "espressif/esp_system.h"
#include "espressif/esp_misc.h"
#include "espressif/esp_wifi.h"
#include "espressif/esp_sta.h"
#include "espressif/esp_softap.h"
#include "espressif/esp_timer.h"


// FIXME
static inline void system_set_os_print(uint8 onoff) { (void)onoff; }
static inline void system_init_done_cb(void *cb) { (void)cb; }

bool wifi_softap_deauth(uint8 mac[6]);

#endif /* SDK_OVERRIDES_INCLUDE_USER_INTERFACE_H_ */
