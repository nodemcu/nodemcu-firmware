#ifndef __USER_CONFIG_H__
#define __USER_CONFIG_H__

#define AT_CUSTOM_UPGRADE

#ifdef AT_CUSTOM_UPGRADE
    #ifndef AT_UPGRADE_SUPPORT
    #error "upgrade is not supported when eagle.flash.bin+eagle.irom0text.bin!!!"
    #endif
#endif

#endif
