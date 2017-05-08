#ifndef SDK_OVERRIDES_INCLUDE_USER_INTERFACE_H_
#define SDK_OVERRIDES_INCLUDE_USER_INTERFACE_H_

#include_next "user_interface.h"

bool wifi_softap_deauth(uint8 mac[6]);
uint8 get_fpm_auto_sleep_flag(void);

//force sleep API
#define FPM_SLEEP_MAX_TIME 268435455 //0xFFFFFFF
void wifi_fpm_set_wakeup_cb(void (*fpm_wakeup_cb_func)(void));
bool fpm_is_open(void);
bool fpm_rf_is_closed(void);
uint8 get_fpm_auto_sleep_flag(void);



#endif /* SDK_OVERRIDES_INCLUDE_USER_INTERFACE_H_ */
