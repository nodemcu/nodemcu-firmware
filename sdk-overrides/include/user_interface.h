#ifndef SDK_OVERRIDES_INCLUDE_USER_INTERFACE_H_
#define SDK_OVERRIDES_INCLUDE_USER_INTERFACE_H_

#include_next "user_interface.h"

bool wifi_softap_deauth(uint8 mac[6]);
uint8 get_fpm_auto_sleep_flag(void);

enum ext_flash_size_map {
    FLASH_SIZE_32M_MAP_2048_2048 = 7,
    FLASH_SIZE_64M_MAP = 8,
    FLASH_SIZE_128M_MAP = 9
};


#endif /* SDK_OVERRIDES_INCLUDE_USER_INTERFACE_H_ */
