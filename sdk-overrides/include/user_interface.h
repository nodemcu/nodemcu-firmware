#ifndef SDK_OVERRIDES_INCLUDE_USER_INTERFACE_H_
#define SDK_OVERRIDES_INCLUDE_USER_INTERFACE_H_

#include_next "user_interface.h"

bool wifi_softap_deauth(uint8 mac[6]);
uint8 get_fpm_auto_sleep_flag(void);


#endif /* SDK_OVERRIDES_INCLUDE_USER_INTERFACE_H_ */
