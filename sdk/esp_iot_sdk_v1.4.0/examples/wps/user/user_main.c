/******************************************************************************
 * Copyright 2013-2014 Espressif Systems (Wuxi)
 *
 * FileName: user_main.c
 *
 * Description: entry file of user application
 *
 * Modification history:
 *     2015/7/3, v1.0 create this file.
*******************************************************************************/

#include "osapi.h"
#include "user_interface.h"

#include "driver/key.h"

#define WPS_KEY_NUM        1

#define WPS_KEY_IO_MUX     PERIPHS_IO_MUX_MTCK_U
#define WPS_KEY_IO_NUM     13
#define WPS_KEY_IO_FUNC    FUNC_GPIO13

LOCAL struct keys_param keys;
LOCAL struct single_key_param *single_key;

LOCAL void ICACHE_FLASH_ATTR
user_wps_status_cb(int status)
{
	switch (status) {
		case WPS_CB_ST_SUCCESS:
			wifi_wps_disable();
			wifi_station_connect();
			break;
		case WPS_CB_ST_FAILED:
		case WPS_CB_ST_TIMEOUT:
			wifi_wps_start();
			break;
	}
}

LOCAL void ICACHE_FLASH_ATTR
user_wps_key_short_press(void)
{
	wifi_wps_disable();
	wifi_wps_enable(WPS_TYPE_PBC);
	wifi_set_wps_cb(user_wps_status_cb);
	wifi_wps_start();
}

void ICACHE_FLASH_ATTR
user_rf_pre_init(void)
{
}

void ICACHE_FLASH_ATTR
user_init(void)
{
	single_key = key_init_single(WPS_KEY_IO_NUM, WPS_KEY_IO_MUX, WPS_KEY_IO_FUNC,
		                                    NULL, user_wps_key_short_press);

	keys.key_num = WPS_KEY_NUM;
	keys.single_key = &single_key;

	key_init(&keys);

	wifi_set_opmode(STATION_MODE);
}
