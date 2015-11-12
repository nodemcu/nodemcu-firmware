/******************************************************************************
 * Copyright 2013-2014 Espressif Systems (Wuxi)
 *
 * FileName: user_webserver.c
 *
 * Description: The web server mode configration.
 *              Check your hardware connection with the host while use this mode.
 * Modification history:
 *     2014/3/12, v1.0 create this file.
*******************************************************************************/
#include "ets_sys.h"
#include "os_type.h"
#include "osapi.h"
#include "mem.h"
#include "user_interface.h"

#include "user_iot_version.h"
#include "espconn.h"
#include "user_json.h"
#include "user_webserver.h"

#include "upgrade.h"
#if ESP_PLATFORM
#include "user_esp_platform.h"
#endif

#ifdef SERVER_SSL_ENABLE
#include "ssl/cert.h"
#include "ssl/private_key.h"
#endif

#if LIGHT_DEVICE
#include "user_light.h"
#endif

LOCAL struct station_config *sta_conf;
LOCAL struct softap_config *ap_conf;

//LOCAL struct secrty_server_info *sec_server;
//LOCAL struct upgrade_server_info *server;
//struct lewei_login_info *login_info;
LOCAL scaninfo *pscaninfo;
struct bss_info *bss;
struct bss_info *bss_temp;
struct bss_info *bss_head;

extern u16 scannum;

LOCAL uint32 PostCmdNeeRsp = 1;

uint8 upgrade_lock = 0;
LOCAL os_timer_t app_upgrade_10s;
LOCAL os_timer_t upgrade_check_timer;

/******************************************************************************
 * FunctionName : device_get
 * Description  : set up the device information parmer as a JSON format
 * Parameters   : js_ctx -- A pointer to a JSON set up
 * Returns      : result
*******************************************************************************/
LOCAL int ICACHE_FLASH_ATTR
device_get(struct jsontree_context *js_ctx)
{
    const char *path = jsontree_path_name(js_ctx, js_ctx->depth - 1);

    if (os_strncmp(path, "manufacture", 11) == 0) {
        jsontree_write_string(js_ctx, "Espressif Systems");
    } else if (os_strncmp(path, "product", 7) == 0) {
#if SENSOR_DEVICE
#if HUMITURE_SUB_DEVICE
        jsontree_write_string(js_ctx, "Humiture");
#elif FLAMMABLE_GAS_SUB_DEVICE
        jsontree_write_string(js_ctx, "Flammable Gas");
#endif
#endif
#if PLUG_DEVICE
        jsontree_write_string(js_ctx, "Plug");
#endif
#if LIGHT_DEVICE
        jsontree_write_string(js_ctx, "Light");
#endif
    }

    return 0;
}

LOCAL struct jsontree_callback device_callback =
    JSONTREE_CALLBACK(device_get, NULL);
/******************************************************************************
 * FunctionName : userbin_get
 * Description  : get up the user bin paramer as a JSON format
 * Parameters   : js_ctx -- A pointer to a JSON set up
 * Returns      : result
*******************************************************************************/
LOCAL int ICACHE_FLASH_ATTR
userbin_get(struct jsontree_context *js_ctx)
{
    const char *path = jsontree_path_name(js_ctx, js_ctx->depth - 1);
    char string[32];

    if (os_strncmp(path, "status", 8) == 0) {
        os_sprintf(string, "200");
    } else if (os_strncmp(path, "user_bin", 8) == 0) {
    	if (system_upgrade_userbin_check() == 0x00) {
    		 os_sprintf(string, "user1.bin");
    	} else if (system_upgrade_userbin_check() == 0x01) {
    		 os_sprintf(string, "user2.bin");
    	} else{
    		return 0;
    	}
    }

    jsontree_write_string(js_ctx, string);

    return 0;
}

LOCAL struct jsontree_callback userbin_callback =
    JSONTREE_CALLBACK(userbin_get, NULL);

JSONTREE_OBJECT(userbin_tree,
                JSONTREE_PAIR("status", &userbin_callback),
                JSONTREE_PAIR("user_bin", &userbin_callback));
JSONTREE_OBJECT(userinfo_tree,JSONTREE_PAIR("user_info",&userbin_tree));
/******************************************************************************
 * FunctionName : version_get
 * Description  : set up the device version paramer as a JSON format
 * Parameters   : js_ctx -- A pointer to a JSON set up
 * Returns      : result
*******************************************************************************/
LOCAL int ICACHE_FLASH_ATTR
version_get(struct jsontree_context *js_ctx)
{
    const char *path = jsontree_path_name(js_ctx, js_ctx->depth - 1);
    char string[32];

    if (os_strncmp(path, "hardware", 8) == 0) {
#if SENSOR_DEVICE
        os_sprintf(string, "0.3");
#else
        os_sprintf(string, "0.1");
#endif
    } else if (os_strncmp(path, "sdk_version", 11) == 0) {
        os_sprintf(string, "%s", system_get_sdk_version());
    } else if (os_strncmp(path, "iot_version", 11) == 0) {
    	os_sprintf(string,"%s%d.%d.%dt%d(%s)",VERSION_TYPE,IOT_VERSION_MAJOR,\
    	IOT_VERSION_MINOR,IOT_VERSION_REVISION,device_type,UPGRADE_FALG);
    }

    jsontree_write_string(js_ctx, string);

    return 0;
}

LOCAL struct jsontree_callback version_callback =
    JSONTREE_CALLBACK(version_get, NULL);

JSONTREE_OBJECT(device_tree,
                JSONTREE_PAIR("product", &device_callback),
                JSONTREE_PAIR("manufacturer", &device_callback));
JSONTREE_OBJECT(version_tree,
                JSONTREE_PAIR("hardware", &version_callback),
                JSONTREE_PAIR("sdk_version", &version_callback),
                JSONTREE_PAIR("iot_version", &version_callback),
                );
JSONTREE_OBJECT(info_tree,
                JSONTREE_PAIR("Version", &version_tree),
                JSONTREE_PAIR("Device", &device_tree));

JSONTREE_OBJECT(INFOTree,
                JSONTREE_PAIR("info", &info_tree));

LOCAL int ICACHE_FLASH_ATTR
connect_status_get(struct jsontree_context *js_ctx)
{
    const char *path = jsontree_path_name(js_ctx, js_ctx->depth - 1);

    if (os_strncmp(path, "status", 8) == 0) {
        jsontree_write_int(js_ctx, user_esp_platform_get_connect_status());
    }

    return 0;
}

LOCAL struct jsontree_callback connect_status_callback =
    JSONTREE_CALLBACK(connect_status_get, NULL);

JSONTREE_OBJECT(status_sub_tree,
                JSONTREE_PAIR("status", &connect_status_callback));

JSONTREE_OBJECT(connect_status_tree,
                JSONTREE_PAIR("Status", &status_sub_tree));

JSONTREE_OBJECT(con_status_tree,
                JSONTREE_PAIR("info", &connect_status_tree));

#if PLUG_DEVICE
/******************************************************************************
 * FunctionName : status_get
 * Description  : set up the device status as a JSON format
 * Parameters   : js_ctx -- A pointer to a JSON set up
 * Returns      : result
*******************************************************************************/
LOCAL int ICACHE_FLASH_ATTR
status_get(struct jsontree_context *js_ctx)
{
    if (user_plug_get_status() == 1) {
        jsontree_write_int(js_ctx, 1);
    } else {
        jsontree_write_int(js_ctx, 0);
    }

    return 0;
}

/******************************************************************************
 * FunctionName : status_set
 * Description  : parse the device status parmer as a JSON format
 * Parameters   : js_ctx -- A pointer to a JSON set up
 *                parser -- A pointer to a JSON parser state
 * Returns      : result
*******************************************************************************/
LOCAL int ICACHE_FLASH_ATTR
status_set(struct jsontree_context *js_ctx, struct jsonparse_state *parser)
{
    int type;

    while ((type = jsonparse_next(parser)) != 0) {
        if (type == JSON_TYPE_PAIR_NAME) {
            if (jsonparse_strcmp_value(parser, "status") == 0) {
                uint8 status;
                jsonparse_next(parser);
                jsonparse_next(parser);
                status = jsonparse_get_value_as_int(parser);
                user_plug_set_status(status);
            }
        }
    }

    return 0;
}

LOCAL struct jsontree_callback status_callback =
    JSONTREE_CALLBACK(status_get, status_set);

JSONTREE_OBJECT(status_tree,
                JSONTREE_PAIR("status", &status_callback));
JSONTREE_OBJECT(response_tree,
                JSONTREE_PAIR("Response", &status_tree));
JSONTREE_OBJECT(StatusTree,
                JSONTREE_PAIR("switch", &response_tree));
#endif

#if LIGHT_DEVICE
LOCAL int ICACHE_FLASH_ATTR
light_status_get(struct jsontree_context *js_ctx)
{
    const char *path = jsontree_path_name(js_ctx, js_ctx->depth - 1);

    if (os_strncmp(path, "red", 3) == 0) {
        jsontree_write_int(js_ctx, user_light_get_duty(LIGHT_RED));
    } else if (os_strncmp(path, "green", 5) == 0) {
        jsontree_write_int(js_ctx, user_light_get_duty(LIGHT_GREEN));
    } else if (os_strncmp(path, "blue", 4) == 0) {
        jsontree_write_int(js_ctx, user_light_get_duty(LIGHT_BLUE));
    } else if (os_strncmp(path, "wwhite", 6) == 0) {
        if(PWM_CHANNEL>LIGHT_WARM_WHITE){
            jsontree_write_int(js_ctx, user_light_get_duty(LIGHT_WARM_WHITE));
        }else{
            jsontree_write_int(js_ctx, 0);
        }
    } else if (os_strncmp(path, "cwhite", 6) == 0) {
        if(PWM_CHANNEL>LIGHT_COLD_WHITE){
            jsontree_write_int(js_ctx, user_light_get_duty(LIGHT_COLD_WHITE));
        }else{
            jsontree_write_int(js_ctx, 0);
        }
    } else if (os_strncmp(path, "period", 6) == 0) {
        jsontree_write_int(js_ctx, user_light_get_period());
    }

    return 0;
}

LOCAL int ICACHE_FLASH_ATTR
light_status_set(struct jsontree_context *js_ctx, struct jsonparse_state *parser)
{
    int type;
    static uint32 r,g,b,cw,ww,period;
    period = 1000;
    cw=0;
    ww=0;
    extern uint8 light_sleep_flg;
    
    while ((type = jsonparse_next(parser)) != 0) {
        if (type == JSON_TYPE_PAIR_NAME) {
            if (jsonparse_strcmp_value(parser, "red") == 0) {
                uint32 status;
                jsonparse_next(parser);
                jsonparse_next(parser);
                status = jsonparse_get_value_as_int(parser);
                r=status;
                os_printf("R: %d \n",status);
                //user_light_set_duty(status, LIGHT_RED);
                //light_set_aim_r( r);
            } else if (jsonparse_strcmp_value(parser, "green") == 0) {
                uint32 status;
                jsonparse_next(parser);
                jsonparse_next(parser);
                status = jsonparse_get_value_as_int(parser);
                g=status;
                os_printf("G: %d \n",status);
                //user_light_set_duty(status, LIGHT_GREEN);
                //light_set_aim_g( g);
            } else if (jsonparse_strcmp_value(parser, "blue") == 0) {
                uint32 status;
                jsonparse_next(parser);
                jsonparse_next(parser);
                status = jsonparse_get_value_as_int(parser);
                b=status;
                os_printf("B: %d \n",status);
                //user_light_set_duty(status, LIGHT_BLUE);
                //set_aim_b( b);
            } else if (jsonparse_strcmp_value(parser, "cwhite") == 0) {
                uint32 status;
                jsonparse_next(parser);
                jsonparse_next(parser);
                status = jsonparse_get_value_as_int(parser);
                cw=status;
                os_printf("CW: %d \n",status);
                //user_light_set_duty(status, LIGHT_BLUE);
                //set_aim_b( b);
            } else if (jsonparse_strcmp_value(parser, "wwhite") == 0) {
                uint32 status;
                jsonparse_next(parser);
                jsonparse_next(parser);
                status = jsonparse_get_value_as_int(parser);
                ww=status;
                os_printf("WW: %d \n",status);
                //user_light_set_duty(status, LIGHT_BLUE);
                //set_aim_b( b);
            } else if (jsonparse_strcmp_value(parser, "period") == 0) {
                uint32 status;
                jsonparse_next(parser);
                jsonparse_next(parser);
                status = jsonparse_get_value_as_int(parser);
                os_printf("PERIOD: %d \n",status);
                period=status;
                //user_light_set_period(status);
            }else if (jsonparse_strcmp_value(parser, "response") == 0) {
                uint32 status;
                jsonparse_next(parser);
                jsonparse_next(parser);
                status = jsonparse_get_value_as_int(parser);
                os_printf("rspneed: %d \n",status);
                PostCmdNeeRsp = status;
                
            }
        }
    }

    if((r|g|b|ww|cw) == 0){
        if(light_sleep_flg==0){

        }
        
    }else{
        if(light_sleep_flg==1){
            os_printf("modem sleep en\r\n");
            wifi_set_sleep_type(MODEM_SLEEP_T);
            light_sleep_flg =0;
        }
    }
    light_set_aim(r,g,b,cw,ww,period);
    return 0;
}

LOCAL struct jsontree_callback light_callback =
    JSONTREE_CALLBACK(light_status_get, light_status_set);

JSONTREE_OBJECT(rgb_tree,
                JSONTREE_PAIR("red", &light_callback),
                JSONTREE_PAIR("green", &light_callback),
                JSONTREE_PAIR("blue", &light_callback),
                JSONTREE_PAIR("cwhite", &light_callback),
                JSONTREE_PAIR("wwhite", &light_callback),
                );
JSONTREE_OBJECT(sta_tree,
                JSONTREE_PAIR("period", &light_callback),
                JSONTREE_PAIR("rgb", &rgb_tree));
JSONTREE_OBJECT(PwmTree,
                JSONTREE_PAIR("light", &sta_tree));
#endif

/******************************************************************************
 * FunctionName : wifi_station_get
 * Description  : set up the station paramer as a JSON format
 * Parameters   : js_ctx -- A pointer to a JSON set up
 * Returns      : result
*******************************************************************************/
LOCAL int ICACHE_FLASH_ATTR
wifi_station_get(struct jsontree_context *js_ctx)
{
    const char *path = jsontree_path_name(js_ctx, js_ctx->depth - 1);
    struct ip_info ipconfig;
    uint8 buf[20];
    os_bzero(buf, sizeof(buf));
    wifi_station_get_config(sta_conf);
    wifi_get_ip_info(STATION_IF, &ipconfig);

    if (os_strncmp(path, "ssid", 4) == 0) {
        jsontree_write_string(js_ctx, sta_conf->ssid);
    } else if (os_strncmp(path, "password", 8) == 0) {
        jsontree_write_string(js_ctx, sta_conf->password);
    } else if (os_strncmp(path, "ip", 2) == 0) {
        os_sprintf(buf, IPSTR, IP2STR(&ipconfig.ip));
        jsontree_write_string(js_ctx, buf);
    } else if (os_strncmp(path, "mask", 4) == 0) {
        os_sprintf(buf, IPSTR, IP2STR(&ipconfig.netmask));
        jsontree_write_string(js_ctx, buf);
    } else if (os_strncmp(path, "gw", 2) == 0) {
        os_sprintf(buf, IPSTR, IP2STR(&ipconfig.gw));
        jsontree_write_string(js_ctx, buf);
    }

    return 0;
}

/******************************************************************************
 * FunctionName : wifi_station_set
 * Description  : parse the station parmer as a JSON format
 * Parameters   : js_ctx -- A pointer to a JSON set up
 *                parser -- A pointer to a JSON parser state
 * Returns      : result
*******************************************************************************/
LOCAL int ICACHE_FLASH_ATTR
wifi_station_set(struct jsontree_context *js_ctx, struct jsonparse_state *parser)
{
    int type;
    uint8 station_tree;

    while ((type = jsonparse_next(parser)) != 0) {
        if (type == JSON_TYPE_PAIR_NAME) {
            char buffer[64];
            os_bzero(buffer, 64);

            if (jsonparse_strcmp_value(parser, "Station") == 0) {
                station_tree = 1;
            } else if (jsonparse_strcmp_value(parser, "Softap") == 0) {
                station_tree = 0;
            }

            if (station_tree) {
                if (jsonparse_strcmp_value(parser, "ssid") == 0) {
                    jsonparse_next(parser);
                    jsonparse_next(parser);
                    jsonparse_copy_value(parser, buffer, sizeof(buffer));
                    os_memcpy(sta_conf->ssid, buffer, os_strlen(buffer));
                } else if (jsonparse_strcmp_value(parser, "password") == 0) {
                    jsonparse_next(parser);
                    jsonparse_next(parser);
                    jsonparse_copy_value(parser, buffer, sizeof(buffer));
                    os_memcpy(sta_conf->password, buffer, os_strlen(buffer));
                }

#if ESP_PLATFORM

                else if (jsonparse_strcmp_value(parser, "token") == 0) {
                    jsonparse_next(parser);
                    jsonparse_next(parser);
                    jsonparse_copy_value(parser, buffer, sizeof(buffer));
                    user_esp_platform_set_token(buffer);
                }

#endif
            }
        }
    }

    return 0;
}

LOCAL struct jsontree_callback wifi_station_callback =
    JSONTREE_CALLBACK(wifi_station_get, wifi_station_set);

JSONTREE_OBJECT(get_station_config_tree,
                JSONTREE_PAIR("ssid", &wifi_station_callback),
                JSONTREE_PAIR("password", &wifi_station_callback));
JSONTREE_OBJECT(set_station_config_tree,
                JSONTREE_PAIR("ssid", &wifi_station_callback),
                JSONTREE_PAIR("password", &wifi_station_callback),
                JSONTREE_PAIR("token", &wifi_station_callback));

JSONTREE_OBJECT(ip_tree,
                JSONTREE_PAIR("ip", &wifi_station_callback),
                JSONTREE_PAIR("mask", &wifi_station_callback),
                JSONTREE_PAIR("gw", &wifi_station_callback));
JSONTREE_OBJECT(get_station_tree,
                JSONTREE_PAIR("Connect_Station", &get_station_config_tree),
                JSONTREE_PAIR("Ipinfo_Station", &ip_tree));
JSONTREE_OBJECT(set_station_tree,
                JSONTREE_PAIR("Connect_Station", &set_station_config_tree));

//JSONTREE_OBJECT(get_wifi_station_info_tree,
//                JSONTREE_PAIR("Station", &get_station_tree));
//JSONTREE_OBJECT(set_wifi_station_info_tree,
//                JSONTREE_PAIR("station", &set_station_tree));

/******************************************************************************
 * FunctionName : wifi_softap_get
 * Description  : set up the softap paramer as a JSON format
 * Parameters   : js_ctx -- A pointer to a JSON set up
 * Returns      : result
*******************************************************************************/
LOCAL int ICACHE_FLASH_ATTR
wifi_softap_get(struct jsontree_context *js_ctx)
{
    const char *path = jsontree_path_name(js_ctx, js_ctx->depth - 1);
    struct ip_info ipconfig;
    uint8 buf[20];
    os_bzero(buf, sizeof(buf));
    wifi_softap_get_config(ap_conf);
    wifi_get_ip_info(SOFTAP_IF, &ipconfig);

    if (os_strncmp(path, "ssid", 4) == 0) {
        jsontree_write_string(js_ctx, ap_conf->ssid);
    } else if (os_strncmp(path, "password", 8) == 0) {
        jsontree_write_string(js_ctx, ap_conf->password);
    } else if (os_strncmp(path, "channel", 7) == 0) {
        jsontree_write_int(js_ctx, ap_conf->channel);
    } else if (os_strncmp(path, "authmode", 8) == 0) {
        switch (ap_conf->authmode) {
            case AUTH_OPEN:
                jsontree_write_string(js_ctx, "OPEN");
                break;

            case AUTH_WEP:
                jsontree_write_string(js_ctx, "WEP");
                break;

            case AUTH_WPA_PSK:
                jsontree_write_string(js_ctx, "WPAPSK");
                break;

            case AUTH_WPA2_PSK:
                jsontree_write_string(js_ctx, "WPA2PSK");
                break;

            case AUTH_WPA_WPA2_PSK:
                jsontree_write_string(js_ctx, "WPAPSK/WPA2PSK");
                break;

            default :
                jsontree_write_int(js_ctx, ap_conf->authmode);
                break;
        }
    } else if (os_strncmp(path, "ip", 2) == 0) {
        os_sprintf(buf, IPSTR, IP2STR(&ipconfig.ip));
        jsontree_write_string(js_ctx, buf);
    } else if (os_strncmp(path, "mask", 4) == 0) {
        os_sprintf(buf, IPSTR, IP2STR(&ipconfig.netmask));
        jsontree_write_string(js_ctx, buf);
    } else if (os_strncmp(path, "gw", 2) == 0) {
        os_sprintf(buf, IPSTR, IP2STR(&ipconfig.gw));
        jsontree_write_string(js_ctx, buf);
    }

    return 0;
}

/******************************************************************************
 * FunctionName : wifi_softap_set
 * Description  : parse the softap parmer as a JSON format
 * Parameters   : js_ctx -- A pointer to a JSON set up
 *                parser -- A pointer to a JSON parser state
 * Returns      : result
*******************************************************************************/
LOCAL int ICACHE_FLASH_ATTR
wifi_softap_set(struct jsontree_context *js_ctx, struct jsonparse_state *parser)
{
    int type;
    uint8 softap_tree;

    while ((type = jsonparse_next(parser)) != 0) {
        if (type == JSON_TYPE_PAIR_NAME) {
            char buffer[64];
            os_bzero(buffer, 64);

            if (jsonparse_strcmp_value(parser, "Station") == 0) {
                softap_tree = 0;
            } else if (jsonparse_strcmp_value(parser, "Softap") == 0) {
                softap_tree = 1;
            }

            if (softap_tree) {
                if (jsonparse_strcmp_value(parser, "authmode") == 0) {
                    jsonparse_next(parser);
                    jsonparse_next(parser);
                    jsonparse_copy_value(parser, buffer, sizeof(buffer));

                    // other mode will be supported later...
                    if (os_strcmp(buffer, "OPEN") == 0) {
                        ap_conf->authmode = AUTH_OPEN;
                    } else if (os_strcmp(buffer, "WPAPSK") == 0) {
                        ap_conf->authmode = AUTH_WPA_PSK;
                        os_printf("%d %s\n", ap_conf->authmode, buffer);
                    } else if (os_strcmp(buffer, "WPA2PSK") == 0) {
                        ap_conf->authmode = AUTH_WPA2_PSK;
                    } else if (os_strcmp(buffer, "WPAPSK/WPA2PSK") == 0) {
                        ap_conf->authmode = AUTH_WPA_WPA2_PSK;
                    } else {
                        ap_conf->authmode = AUTH_OPEN;
                        return 0;
                    }
                }

                if (jsonparse_strcmp_value(parser, "channel") == 0) {
                    jsonparse_next(parser);
                    jsonparse_next(parser);
                    ap_conf->channel = jsonparse_get_value_as_int(parser);
                } else if (jsonparse_strcmp_value(parser, "ssid") == 0) {
                    jsonparse_next(parser);
                    jsonparse_next(parser);
                    jsonparse_copy_value(parser, buffer, sizeof(buffer));
                    os_memcpy(ap_conf->ssid, buffer, os_strlen(buffer));
                } else if (jsonparse_strcmp_value(parser, "password") == 0) {
                    jsonparse_next(parser);
                    jsonparse_next(parser);
                    jsonparse_copy_value(parser, buffer, sizeof(buffer));
                    os_memcpy(ap_conf->password, buffer, os_strlen(buffer));
                }
            }
        }
    }

    return 0;
}

LOCAL struct jsontree_callback wifi_softap_callback =
    JSONTREE_CALLBACK(wifi_softap_get, wifi_softap_set);

JSONTREE_OBJECT(softap_config_tree,
                JSONTREE_PAIR("authmode", &wifi_softap_callback),
                JSONTREE_PAIR("channel", &wifi_softap_callback),
                JSONTREE_PAIR("ssid", &wifi_softap_callback),
                JSONTREE_PAIR("password", &wifi_softap_callback));
JSONTREE_OBJECT(softap_ip_tree,
                JSONTREE_PAIR("ip", &wifi_softap_callback),
                JSONTREE_PAIR("mask", &wifi_softap_callback),
                JSONTREE_PAIR("gw", &wifi_softap_callback));
JSONTREE_OBJECT(get_softap_tree,
                JSONTREE_PAIR("Connect_Softap", &softap_config_tree),
                JSONTREE_PAIR("Ipinfo_Softap", &softap_ip_tree));
JSONTREE_OBJECT(set_softap_tree,
                JSONTREE_PAIR("Ipinfo_Softap", &softap_config_tree));

JSONTREE_OBJECT(get_wifi_tree,
                JSONTREE_PAIR("Station", &get_station_tree),
                JSONTREE_PAIR("Softap", &get_softap_tree));
JSONTREE_OBJECT(set_wifi_tree,
                JSONTREE_PAIR("Station", &set_station_tree),
                JSONTREE_PAIR("Softap", &set_softap_tree));

JSONTREE_OBJECT(wifi_response_tree,
                JSONTREE_PAIR("Response", &get_wifi_tree));
JSONTREE_OBJECT(wifi_request_tree,
                JSONTREE_PAIR("Request", &set_wifi_tree));

JSONTREE_OBJECT(wifi_info_tree,
                JSONTREE_PAIR("wifi", &wifi_response_tree));
JSONTREE_OBJECT(wifi_req_tree,
                JSONTREE_PAIR("wifi", &wifi_request_tree));


/******************************************************************************
 * FunctionName : scan_get
 * Description  : set up the scan data as a JSON format
 * Parameters   : js_ctx -- A pointer to a JSON set up
 * Returns      : result
*******************************************************************************/
LOCAL int ICACHE_FLASH_ATTR
scan_get(struct jsontree_context *js_ctx)
{
    const char *path = jsontree_path_name(js_ctx, js_ctx->depth - 1);
    //    STAILQ_HEAD(, bss_info) *pbss = scanarg;
//    LOCAL struct bss_info *bss;

    if (os_strncmp(path, "TotalPage", 9) == 0) {
        jsontree_write_int(js_ctx, pscaninfo->totalpage);
    } else if (os_strncmp(path, "PageNum", 7) == 0) {
        jsontree_write_int(js_ctx, pscaninfo->pagenum);
    } else if (os_strncmp(path, "bssid", 5) == 0) {
    	if( bss == NULL )
    		bss = bss_head;
        u8 buffer[32];
        //if (bss != NULL){
        os_memset(buffer, 0, sizeof(buffer));
        os_sprintf(buffer, MACSTR, MAC2STR(bss->bssid));
        jsontree_write_string(js_ctx, buffer);
        //}
    } else if (os_strncmp(path, "ssid", 4) == 0) {
        //if (bss != NULL)
        jsontree_write_string(js_ctx, bss->ssid);
    } else if (os_strncmp(path, "rssi", 4) == 0) {
        //if (bss != NULL)
        jsontree_write_int(js_ctx, -(bss->rssi));
    } else if (os_strncmp(path, "channel", 7) == 0) {
        //if (bss != NULL)
        jsontree_write_int(js_ctx, bss->channel);
    } else if (os_strncmp(path, "authmode", 8) == 0) {
        //if (bss != NULL){
        switch (bss->authmode) {
            case AUTH_OPEN:
                jsontree_write_string(js_ctx, "OPEN");
                break;

            case AUTH_WEP:
                jsontree_write_string(js_ctx, "WEP");
                break;

            case AUTH_WPA_PSK:
                jsontree_write_string(js_ctx, "WPAPSK");
                break;

            case AUTH_WPA2_PSK:
                jsontree_write_string(js_ctx, "WPA2PSK");
                break;

            case AUTH_WPA_WPA2_PSK:
                jsontree_write_string(js_ctx, "WPAPSK/WPA2PSK");
                break;

            default :
                jsontree_write_int(js_ctx, bss->authmode);
                break;
        }

        bss = STAILQ_NEXT(bss, next);
//        os_free(bss);
        //}
    }

    return 0;
}

LOCAL struct jsontree_callback scan_callback =
    JSONTREE_CALLBACK(scan_get, NULL);

JSONTREE_OBJECT(scaninfo_tree,
                JSONTREE_PAIR("bssid", &scan_callback),
                JSONTREE_PAIR("ssid", &scan_callback),
                JSONTREE_PAIR("rssi", &scan_callback),
                JSONTREE_PAIR("channel", &scan_callback),
                JSONTREE_PAIR("authmode", &scan_callback));
JSONTREE_ARRAY(scanrslt_tree,
               JSONTREE_PAIR_ARRAY(&scaninfo_tree),
               JSONTREE_PAIR_ARRAY(&scaninfo_tree),
               JSONTREE_PAIR_ARRAY(&scaninfo_tree),
               JSONTREE_PAIR_ARRAY(&scaninfo_tree),
               JSONTREE_PAIR_ARRAY(&scaninfo_tree),
               JSONTREE_PAIR_ARRAY(&scaninfo_tree),
               JSONTREE_PAIR_ARRAY(&scaninfo_tree),
               JSONTREE_PAIR_ARRAY(&scaninfo_tree));

JSONTREE_OBJECT(scantree,
                JSONTREE_PAIR("TotalPage", &scan_callback),
                JSONTREE_PAIR("PageNum", &scan_callback),
                JSONTREE_PAIR("ScanResult", &scanrslt_tree));
JSONTREE_OBJECT(scanres_tree,
                JSONTREE_PAIR("Response", &scantree));
JSONTREE_OBJECT(scan_tree,
                JSONTREE_PAIR("scan", &scanres_tree));

/******************************************************************************
 * FunctionName : parse_url
 * Description  : parse the received data from the server
 * Parameters   : precv -- the received data
 *                purl_frame -- the result of parsing the url
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
parse_url(char *precv, URL_Frame *purl_frame)
{
    char *str = NULL;
    uint8 length = 0;
    char *pbuffer = NULL;
    char *pbufer = NULL;

    if (purl_frame == NULL || precv == NULL) {
        return;
    }

    pbuffer = (char *)os_strstr(precv, "Host:");

    if (pbuffer != NULL) {
        length = pbuffer - precv;
        pbufer = (char *)os_zalloc(length + 1);
        pbuffer = pbufer;
        os_memcpy(pbuffer, precv, length);
        os_memset(purl_frame->pSelect, 0, URLSize);
        os_memset(purl_frame->pCommand, 0, URLSize);
        os_memset(purl_frame->pFilename, 0, URLSize);

        if (os_strncmp(pbuffer, "GET ", 4) == 0) {
            purl_frame->Type = GET;
            pbuffer += 4;
        } else if (os_strncmp(pbuffer, "POST ", 5) == 0) {
            purl_frame->Type = POST;
            pbuffer += 5;
        }

        pbuffer ++;
        str = (char *)os_strstr(pbuffer, "?");

        if (str != NULL) {
            length = str - pbuffer;
            os_memcpy(purl_frame->pSelect, pbuffer, length);
            str ++;
            pbuffer = (char *)os_strstr(str, "=");

            if (pbuffer != NULL) {
                length = pbuffer - str;
                os_memcpy(purl_frame->pCommand, str, length);
                pbuffer ++;
                str = (char *)os_strstr(pbuffer, "&");

                if (str != NULL) {
                    length = str - pbuffer;
                    os_memcpy(purl_frame->pFilename, pbuffer, length);
                } else {
                    str = (char *)os_strstr(pbuffer, " HTTP");

                    if (str != NULL) {
                        length = str - pbuffer;
                        os_memcpy(purl_frame->pFilename, pbuffer, length);
                    }
                }
            }
        }

        os_free(pbufer);
    } else {
        return;
    }
}

LOCAL char *precvbuffer;
static uint32 dat_sumlength = 0;
LOCAL bool ICACHE_FLASH_ATTR
save_data(char *precv, uint16 length)
{
    bool flag = false;
    char length_buf[10] = {0};
    char *ptemp = NULL;
    char *pdata = NULL;
    uint16 headlength = 0;
    static uint32 totallength = 0;

    ptemp = (char *)os_strstr(precv, "\r\n\r\n");

    if (ptemp != NULL) {
        length -= ptemp - precv;
        length -= 4;
        totallength += length;
        headlength = ptemp - precv + 4;
        pdata = (char *)os_strstr(precv, "Content-Length: ");

        if (pdata != NULL) {
            pdata += 16;
            precvbuffer = (char *)os_strstr(pdata, "\r\n");

            if (precvbuffer != NULL) {
                os_memcpy(length_buf, pdata, precvbuffer - pdata);
                dat_sumlength = atoi(length_buf);
            }
        } else {
        	if (totallength != 0x00){
        		totallength = 0;
        		dat_sumlength = 0;
        		return false;
        	}
        }
        if ((dat_sumlength + headlength) >= 1024) {
        	precvbuffer = (char *)os_zalloc(headlength + 1);
            os_memcpy(precvbuffer, precv, headlength + 1);
        } else {
        	precvbuffer = (char *)os_zalloc(dat_sumlength + headlength + 1);
        	os_memcpy(precvbuffer, precv, os_strlen(precv));
        }
    } else {
        if (precvbuffer != NULL) {
            totallength += length;
            os_memcpy(precvbuffer + os_strlen(precvbuffer), precv, length);
        } else {
            totallength = 0;
            dat_sumlength = 0;
            return false;
        }
    }

    if (totallength == dat_sumlength) {
        totallength = 0;
        dat_sumlength = 0;
        return true;
    } else {
        return false;
    }
}

LOCAL bool ICACHE_FLASH_ATTR
check_data(char *precv, uint16 length)
{
        //bool flag = true;
    char length_buf[10] = {0};
    char *ptemp = NULL;
    char *pdata = NULL;
    char *tmp_precvbuffer;
    uint16 tmp_length = length;
    uint32 tmp_totallength = 0;
    
    ptemp = (char *)os_strstr(precv, "\r\n\r\n");
    
    if (ptemp != NULL) {
        tmp_length -= ptemp - precv;
        tmp_length -= 4;
        tmp_totallength += tmp_length;
        
        pdata = (char *)os_strstr(precv, "Content-Length: ");
        
        if (pdata != NULL){
            pdata += 16;
            tmp_precvbuffer = (char *)os_strstr(pdata, "\r\n");
            
            if (tmp_precvbuffer != NULL){
                os_memcpy(length_buf, pdata, tmp_precvbuffer - pdata);
                dat_sumlength = atoi(length_buf);
                os_printf("A_dat:%u,tot:%u,lenght:%u\n",dat_sumlength,tmp_totallength,tmp_length);
                if(dat_sumlength != tmp_totallength){
                    return false;
                }
            }
        }
    }
    return true;
}

LOCAL os_timer_t *restart_10ms;
LOCAL rst_parm *rstparm;

/******************************************************************************
 * FunctionName : restart_10ms_cb
 * Description  : system restart or wifi reconnected after a certain time.
 * Parameters   : arg -- Additional argument to pass to the function
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
restart_10ms_cb(void *arg)
{
    if (rstparm != NULL && rstparm->pespconn != NULL) {
        switch (rstparm->parmtype) {
            case WIFI:
                //if (rstparm->pespconn->state == ESPCONN_CLOSE) {
                    if (sta_conf->ssid[0] != 0x00) {
                        wifi_station_set_config(sta_conf);
                        wifi_station_disconnect();
                        wifi_station_connect();
                        user_esp_platform_check_ip(1);
                    }

                    if (ap_conf->ssid[0] != 0x00) {
                        wifi_softap_set_config(ap_conf);
                        system_restart();
                    }

                    os_free(ap_conf);
                    ap_conf = NULL;
                    os_free(sta_conf);
                    sta_conf = NULL;
                    os_free(rstparm);
                    rstparm = NULL;
                    os_free(restart_10ms);
                    restart_10ms = NULL;
                //} else {
                //   os_timer_arm(restart_10ms, 10, 0);
                //}

                break;

            case DEEP_SLEEP:
            case REBOOT:
                if (rstparm->pespconn->state == ESPCONN_CLOSE) {
                    wifi_set_opmode(STATION_MODE);

                    if (rstparm->parmtype == DEEP_SLEEP) {
#if SENSOR_DEVICE
                        system_deep_sleep(SENSOR_DEEP_SLEEP_TIME);
#endif
                    }
                } else {
                    os_timer_arm(restart_10ms, 10, 0);
                }

                break;

            default:
                break;
        }
    }
}

/******************************************************************************
 * FunctionName : data_send
 * Description  : processing the data as http format and send to the client or server
 * Parameters   : arg -- argument to set for client or server
 *                responseOK -- true or false
 *                psend -- The send data
 * Returns      :
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
data_send(void *arg, bool responseOK, char *psend)
{
    uint16 length = 0;
    char *pbuf = NULL;
    char httphead[256];
    struct espconn *ptrespconn = arg;
    os_memset(httphead, 0, 256);

    if (responseOK) {
        os_sprintf(httphead,
                   "HTTP/1.0 200 OK\r\nContent-Length: %d\r\nServer: lwIP/1.4.0\r\n",
                   psend ? os_strlen(psend) : 0);

        if (psend) {
            os_sprintf(httphead + os_strlen(httphead),
                       "Content-type: application/json\r\nExpires: Fri, 10 Apr 2008 14:00:00 GMT\r\nPragma: no-cache\r\n\r\n");
            length = os_strlen(httphead) + os_strlen(psend);
            pbuf = (char *)os_zalloc(length + 1);
            os_memcpy(pbuf, httphead, os_strlen(httphead));
            os_memcpy(pbuf + os_strlen(httphead), psend, os_strlen(psend));
        } else {
            os_sprintf(httphead + os_strlen(httphead), "\n");
            length = os_strlen(httphead);
        }
    } else {
        os_sprintf(httphead, "HTTP/1.0 400 BadRequest\r\n\
Content-Length: 0\r\nServer: lwIP/1.4.0\r\n\n");
        length = os_strlen(httphead);
    }

    if (psend) {
#ifdef SERVER_SSL_ENABLE
        espconn_secure_sent(ptrespconn, pbuf, length);
#else
        espconn_sent(ptrespconn, pbuf, length);
#endif
    } else {
#ifdef SERVER_SSL_ENABLE
        espconn_secure_sent(ptrespconn, httphead, length);
#else
        espconn_sent(ptrespconn, httphead, length);
#endif
    }

    if (pbuf) {
        os_free(pbuf);
        pbuf = NULL;
    }
}

/******************************************************************************
 * FunctionName : json_send
 * Description  : processing the data as json format and send to the client or server
 * Parameters   : arg -- argument to set for client or server
 *                ParmType -- json format type
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
json_send(void *arg, ParmType ParmType)
{
    char *pbuf = NULL;
    pbuf = (char *)os_zalloc(jsonSize);
    struct espconn *ptrespconn = arg;

    switch (ParmType) {
#if LIGHT_DEVICE

        case LIGHT_STATUS:
            json_ws_send((struct jsontree_value *)&PwmTree, "light", pbuf);
            break;
#endif

#if PLUG_DEVICE

        case SWITCH_STATUS:
            json_ws_send((struct jsontree_value *)&StatusTree, "switch", pbuf);
            break;
#endif

        case INFOMATION:
            json_ws_send((struct jsontree_value *)&INFOTree, "info", pbuf);
            break;

        case WIFI:
            json_ws_send((struct jsontree_value *)&wifi_info_tree, "wifi", pbuf);
            break;

        case CONNECT_STATUS:
            json_ws_send((struct jsontree_value *)&con_status_tree, "info", pbuf);
            break;

        case USER_BIN:
        	json_ws_send((struct jsontree_value *)&userinfo_tree, "user_info", pbuf);
        	break;
        case SCAN: {
            u8 i = 0;
            u8 scancount = 0;
            struct bss_info *bss = NULL;
//            bss = STAILQ_FIRST(pscaninfo->pbss);
            bss = bss_head;
            if (bss == NULL) {
                os_free(pscaninfo);
                pscaninfo = NULL;
                os_sprintf(pbuf, "{\n\"successful\": false,\n\"data\": null\n}");
            } else {
                do {
                    if (pscaninfo->page_sn == pscaninfo->pagenum) {
                        pscaninfo->page_sn = 0;
                        os_sprintf(pbuf, "{\n\"successful\": false,\n\"meessage\": \"repeated page\"\n}");
                        break;
                    }

                    scancount = scannum - (pscaninfo->pagenum - 1) * 8;

                    if (scancount >= 8) {
                        pscaninfo->data_cnt += 8;
                        pscaninfo->page_sn = pscaninfo->pagenum;

                        if (pscaninfo->data_cnt > scannum) {
                            pscaninfo->data_cnt -= 8;
                            os_sprintf(pbuf, "{\n\"successful\": false,\n\"meessage\": \"error page\"\n}");
                            break;
                        }

                        json_ws_send((struct jsontree_value *)&scan_tree, "scan", pbuf);
                    } else {
                        pscaninfo->data_cnt += scancount;
                        pscaninfo->page_sn = pscaninfo->pagenum;

                        if (pscaninfo->data_cnt > scannum) {
                            pscaninfo->data_cnt -= scancount;
                            os_sprintf(pbuf, "{\n\"successful\": false,\n\"meessage\": \"error page\"\n}");
                            break;
                        }

                        char *ptrscanbuf = (char *)os_zalloc(jsonSize);
                        char *pscanbuf = ptrscanbuf;
                        os_sprintf(pscanbuf, ",\n\"ScanResult\": [\n");
                        pscanbuf += os_strlen(pscanbuf);

                        for (i = 0; i < scancount; i ++) {
                            JSONTREE_OBJECT(page_tree,
                                            JSONTREE_PAIR("page", &scaninfo_tree));
                            json_ws_send((struct jsontree_value *)&page_tree, "page", pscanbuf);
                            os_sprintf(pscanbuf + os_strlen(pscanbuf), ",\n");
                            pscanbuf += os_strlen(pscanbuf);
                        }

                        os_sprintf(pscanbuf - 2, "]\n");
                        JSONTREE_OBJECT(scantree,
                                        JSONTREE_PAIR("TotalPage", &scan_callback),
                                        JSONTREE_PAIR("PageNum", &scan_callback));
                        JSONTREE_OBJECT(scanres_tree,
                                        JSONTREE_PAIR("Response", &scantree));
                        JSONTREE_OBJECT(scan_tree,
                                        JSONTREE_PAIR("scan", &scanres_tree));
                        json_ws_send((struct jsontree_value *)&scan_tree, "scan", pbuf);
                        os_memcpy(pbuf + os_strlen(pbuf) - 4, ptrscanbuf, os_strlen(ptrscanbuf));
                        os_sprintf(pbuf + os_strlen(pbuf), "}\n}");
                        os_free(ptrscanbuf);
                    }
                } while (0);
            }

            break;
        }

        default :
            break;
    }

    data_send(ptrespconn, true, pbuf);
    os_free(pbuf);
    pbuf = NULL;
}

/******************************************************************************
 * FunctionName : response_send
 * Description  : processing the send result
 * Parameters   : arg -- argument to set for client or server
 *                responseOK --  true or false
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
response_send(void *arg, bool responseOK)
{
    struct espconn *ptrespconn = arg;

    data_send(ptrespconn, responseOK, NULL);
}

/******************************************************************************
 * FunctionName : json_scan_cb
 * Description  : processing the scan result
 * Parameters   : arg -- Additional argument to pass to the callback function
 *                status -- scan status
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR json_scan_cb(void *arg, STATUS status)
{
    pscaninfo->pbss = arg;

    if (scannum % 8 == 0) {
        pscaninfo->totalpage = scannum / 8;
    } else {
        pscaninfo->totalpage = scannum / 8 + 1;
    }

    JSONTREE_OBJECT(totaltree,
                    JSONTREE_PAIR("TotalPage", &scan_callback));
    JSONTREE_OBJECT(totalres_tree,
                    JSONTREE_PAIR("Response", &totaltree));
    JSONTREE_OBJECT(total_tree,
                    JSONTREE_PAIR("total", &totalres_tree));

    bss_temp = bss_head;
    while(bss_temp !=NULL) {
    	bss_head = bss_temp->next.stqe_next;
    	os_free(bss_temp);
    	bss_temp = bss_head;
    }
    bss_head = NULL;
    bss_temp = NULL;
    bss = STAILQ_FIRST(pscaninfo->pbss);
    while(bss != NULL) {
    	if(bss_temp == NULL){
    		bss_temp = (struct bss_info *)os_zalloc(sizeof(struct bss_info));
    		bss_head = bss_temp;
    	} else {
    		bss_temp->next.stqe_next = (struct bss_info *)os_zalloc(sizeof(struct bss_info));
    		bss_temp = bss_temp->next.stqe_next;
    	}
    	if(bss_temp == NULL) {
    		os_printf("malloc scan info failed\n");
    		break;
    	} else{
    		os_memcpy(bss_temp->bssid,bss->bssid,sizeof(bss->bssid));
    		os_memcpy(bss_temp->ssid,bss->ssid,sizeof(bss->ssid));
    		bss_temp->authmode = bss->authmode;
    		bss_temp->rssi = bss->rssi;
    		bss_temp->channel = bss->channel;
    	}
    	bss = STAILQ_NEXT(bss,next);
    }
    char *pbuf = NULL;
    pbuf = (char *)os_zalloc(jsonSize);
    json_ws_send((struct jsontree_value *)&total_tree, "total", pbuf);
    data_send(pscaninfo->pespconn, true, pbuf);
    os_free(pbuf);
}

void ICACHE_FLASH_ATTR
upgrade_check_func(void *arg)
{
	struct espconn *ptrespconn = arg;
	os_timer_disarm(&upgrade_check_timer);
	if(system_upgrade_flag_check() == UPGRADE_FLAG_START) {
		response_send(ptrespconn, false);
        system_upgrade_deinit();
        system_upgrade_flag_set(UPGRADE_FLAG_IDLE);
        upgrade_lock = 0;
		os_printf("local upgrade failed\n");
	} else if( system_upgrade_flag_check() == UPGRADE_FLAG_FINISH ) {
		os_printf("local upgrade success\n");
		response_send(ptrespconn, true);
		upgrade_lock = 0;
	} else {

	}


}
/******************************************************************************
 * FunctionName : upgrade_deinit
 * Description  : disconnect the connection with the host
 * Parameters   : bin -- server number
 * Returns      : none
*******************************************************************************/
void ICACHE_FLASH_ATTR
LOCAL local_upgrade_deinit(void)
{
    if (system_upgrade_flag_check() != UPGRADE_FLAG_START) {
    	os_printf("system upgrade deinit\n");
        system_upgrade_deinit();
    }
}


/******************************************************************************
 * FunctionName : upgrade_download
 * Description  : Processing the upgrade data from the host
 * Parameters   : bin -- server number
 *                pusrdata -- The upgrade data (or NULL when the connection has been closed!)
 *                length -- The length of upgrade data
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
local_upgrade_download(void * arg,char *pusrdata, unsigned short length)
{
    char *ptr = NULL;
    char *ptmp2 = NULL;
    char lengthbuffer[32];
    static uint32 totallength = 0;
    static uint32 sumlength = 0;
    static uint32 erase_length = 0;
    char A_buf[2] = {0xE9 ,0x03}; char	B_buf[2] = {0xEA,0x04};
    struct espconn *pespconn = arg;
    if (totallength == 0 && (ptr = (char *)os_strstr(pusrdata, "\r\n\r\n")) != NULL &&
            (ptr = (char *)os_strstr(pusrdata, "Content-Length")) != NULL) {
    	ptr = (char *)os_strstr(pusrdata, "Content-Length: ");
		if (ptr != NULL) {
			ptr += 16;
			ptmp2 = (char *)os_strstr(ptr, "\r\n");

			if (ptmp2 != NULL) {
				os_memset(lengthbuffer, 0, sizeof(lengthbuffer));
				os_memcpy(lengthbuffer, ptr, ptmp2 - ptr);
				sumlength = atoi(lengthbuffer);
				if (sumlength == 0) {
					os_timer_disarm(&upgrade_check_timer);
					os_timer_setfn(&upgrade_check_timer, (os_timer_func_t *)upgrade_check_func, pespconn);
					os_timer_arm(&upgrade_check_timer, 10, 0);
					return;
				}
			} else {
				os_printf("sumlength failed\n");
			}
		} else {
			os_printf("Content-Length: failed\n");
		}
		if (sumlength != 0) {
			if (sumlength >= LIMIT_ERASE_SIZE){
				system_upgrade_erase_flash(0xFFFF);
				erase_length = sumlength - LIMIT_ERASE_SIZE;
			} else {
			system_upgrade_erase_flash(sumlength);
				erase_length = 0;
			}
		}
        ptr = (char *)os_strstr(pusrdata, "\r\n\r\n");
        length -= ptr - pusrdata;
        length -= 4;
        totallength += length;
        os_printf("upgrade file download start.\n");
        system_upgrade(ptr + 4, length);

    } else {
        totallength += length;
        if (erase_length >= LIMIT_ERASE_SIZE){
			system_upgrade_erase_flash(0xFFFF);
			erase_length -= LIMIT_ERASE_SIZE;
		} else {
			system_upgrade_erase_flash(erase_length);
			erase_length = 0;
		}
        system_upgrade(pusrdata, length);
    }

    if (totallength == sumlength) {
        os_printf("upgrade file download finished.\n");
        system_upgrade_flag_set(UPGRADE_FLAG_FINISH);
        totallength = 0;
        sumlength = 0;
        upgrade_check_func(pespconn);
        os_timer_disarm(&app_upgrade_10s);
        os_timer_setfn(&app_upgrade_10s, (os_timer_func_t *)local_upgrade_deinit, NULL);
        os_timer_arm(&app_upgrade_10s, 10, 0);
    }
}

/******************************************************************************
 * FunctionName : webserver_recv
 * Description  : Processing the received data from the server
 * Parameters   : arg -- Additional argument to pass to the callback function
 *                pusrdata -- The received data (or NULL when the connection has been closed!)
 *                length -- The length of received data
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
webserver_recv(void *arg, char *pusrdata, unsigned short length)
{
    URL_Frame *pURL_Frame = NULL;
    char *pParseBuffer = NULL;
    bool parse_flag = false;
    struct espconn *ptrespconn = arg;

    if(upgrade_lock == 0){

        os_printf("len:%u\n",length);
        if(check_data(pusrdata, length) == false)
        {
            os_printf("goto\n");
             goto _temp_exit;
        }
        
    	 parse_flag = save_data(pusrdata, length);
        if (parse_flag == false) {
        	response_send(ptrespconn, false);
        }

//        os_printf(precvbuffer);
        pURL_Frame = (URL_Frame *)os_zalloc(sizeof(URL_Frame));
        parse_url(precvbuffer, pURL_Frame);

        switch (pURL_Frame->Type) {
            case GET:
                os_printf("We have a GET request.\n");

                if (os_strcmp(pURL_Frame->pSelect, "client") == 0 &&
                        os_strcmp(pURL_Frame->pCommand, "command") == 0) {
                    if (os_strcmp(pURL_Frame->pFilename, "info") == 0) {
                        json_send(ptrespconn, INFOMATION);
                    }

                    if (os_strcmp(pURL_Frame->pFilename, "status") == 0) {
                        json_send(ptrespconn, CONNECT_STATUS);
                    } else if (os_strcmp(pURL_Frame->pFilename, "scan") == 0) {
                        char *strstr = NULL;
                        strstr = (char *)os_strstr(pusrdata, "&");

                        if (strstr == NULL) {
                            if (pscaninfo == NULL) {
                                pscaninfo = (scaninfo *)os_zalloc(sizeof(scaninfo));
                            }

                            pscaninfo->pespconn = ptrespconn;
                            pscaninfo->pagenum = 0;
                            pscaninfo->page_sn = 0;
                            pscaninfo->data_cnt = 0;
                            wifi_station_scan(NULL, json_scan_cb);
                        } else {
                            strstr ++;

                            if (os_strncmp(strstr, "page", 4) == 0) {
                                if (pscaninfo != NULL) {
                                    pscaninfo->pagenum = *(strstr + 5);
                                    pscaninfo->pagenum -= 0x30;

                                    if (pscaninfo->pagenum > pscaninfo->totalpage || pscaninfo->pagenum == 0) {
                                        response_send(ptrespconn, false);
                                    } else {
                                        json_send(ptrespconn, SCAN);
                                    }
                                } else {
                                    response_send(ptrespconn, false);
                                }
                            } else if(os_strncmp(strstr, "finish", 6) == 0){
                            	bss_temp = bss_head;
                            	while(bss_temp != NULL) {
                            		bss_head = bss_temp->next.stqe_next;
                            		os_free(bss_temp);
                            		bss_temp = bss_head;
                            	}
                            	bss_head = NULL;
                            	bss_temp = NULL;
                            	response_send(ptrespconn, true);
                            } else {
                                response_send(ptrespconn, false);
                            }
                        }
                    } else {
                        response_send(ptrespconn, false);
                    }
                } else if (os_strcmp(pURL_Frame->pSelect, "config") == 0 &&
                           os_strcmp(pURL_Frame->pCommand, "command") == 0) {
                    if (os_strcmp(pURL_Frame->pFilename, "wifi") == 0) {
                        ap_conf = (struct softap_config *)os_zalloc(sizeof(struct softap_config));
                        sta_conf = (struct station_config *)os_zalloc(sizeof(struct station_config));
                        json_send(ptrespconn, WIFI);
                        os_free(sta_conf);
                        os_free(ap_conf);
                        sta_conf = NULL;
                        ap_conf = NULL;
                    }

#if PLUG_DEVICE
                    else if (os_strcmp(pURL_Frame->pFilename, "switch") == 0) {
                        json_send(ptrespconn, SWITCH_STATUS);
                    }

#endif

#if LIGHT_DEVICE
                    else if (os_strcmp(pURL_Frame->pFilename, "light") == 0) {
                        json_send(ptrespconn, LIGHT_STATUS);
                    }
                    

#endif

                    else if (os_strcmp(pURL_Frame->pFilename, "reboot") == 0) {
                        json_send(ptrespconn, REBOOT);
                    } else {
                        response_send(ptrespconn, false);
                    }
                } else if (os_strcmp(pURL_Frame->pSelect, "upgrade") == 0 &&
    					os_strcmp(pURL_Frame->pCommand, "command") == 0) {
    					if (os_strcmp(pURL_Frame->pFilename, "getuser") == 0) {
    						json_send(ptrespconn , USER_BIN);
    					}
    			} else {
                    response_send(ptrespconn, false);
                }

                break;

            case POST:
                os_printf("We have a POST request.\n");
                pParseBuffer = (char *)os_strstr(precvbuffer, "\r\n\r\n");

                if (pParseBuffer == NULL) {
                    break;
                }

                pParseBuffer += 4;

                if (os_strcmp(pURL_Frame->pSelect, "config") == 0 &&
                        os_strcmp(pURL_Frame->pCommand, "command") == 0) {
#if SENSOR_DEVICE

                    if (os_strcmp(pURL_Frame->pFilename, "sleep") == 0) {
#else

                    if (os_strcmp(pURL_Frame->pFilename, "reboot") == 0) {
#endif

                        if (pParseBuffer != NULL) {
                            if (restart_10ms != NULL) {
                                os_timer_disarm(restart_10ms);
                            }

                            if (rstparm == NULL) {
                                rstparm = (rst_parm *)os_zalloc(sizeof(rst_parm));
                            }

                            rstparm->pespconn = ptrespconn;
#if SENSOR_DEVICE
                            rstparm->parmtype = DEEP_SLEEP;
#else
                            rstparm->parmtype = REBOOT;
#endif

                            if (restart_10ms == NULL) {
                                restart_10ms = (os_timer_t *)os_malloc(sizeof(os_timer_t));
                            }

                            os_timer_setfn(restart_10ms, (os_timer_func_t *)restart_10ms_cb, NULL);
                            os_timer_arm(restart_10ms, 10, 0);  // delay 10ms, then do

                            response_send(ptrespconn, true);
                        } else {
                            response_send(ptrespconn, false);
                        }
                    } else if (os_strcmp(pURL_Frame->pFilename, "wifi") == 0) {
                        if (pParseBuffer != NULL) {
                            struct jsontree_context js;
                            user_esp_platform_set_connect_status(DEVICE_CONNECTING);

                            if (restart_10ms != NULL) {
                                os_timer_disarm(restart_10ms);
                            }

                            if (ap_conf == NULL) {
                                ap_conf = (struct softap_config *)os_zalloc(sizeof(struct softap_config));
                            }

                            if (sta_conf == NULL) {
                                sta_conf = (struct station_config *)os_zalloc(sizeof(struct station_config));
                            }

                            jsontree_setup(&js, (struct jsontree_value *)&wifi_req_tree, json_putchar);
                            json_parse(&js, pParseBuffer);

                            if (rstparm == NULL) {
                                rstparm = (rst_parm *)os_zalloc(sizeof(rst_parm));
                            }

                            rstparm->pespconn = ptrespconn;
                            rstparm->parmtype = WIFI;

                            if (sta_conf->ssid[0] != 0x00 || ap_conf->ssid[0] != 0x00) {
                                ap_conf->ssid_hidden = 0;
                                ap_conf->max_connection = 4;

                                if (restart_10ms == NULL) {
                                    restart_10ms = (os_timer_t *)os_malloc(sizeof(os_timer_t));
                                }

                                os_timer_disarm(restart_10ms);
                                os_timer_setfn(restart_10ms, (os_timer_func_t *)restart_10ms_cb, NULL);
                                os_timer_arm(restart_10ms, 10, 0);  // delay 10ms, then do
                            } else {
                                os_free(ap_conf);
                                os_free(sta_conf);
                                os_free(rstparm);
                                sta_conf = NULL;
                                ap_conf = NULL;
                                rstparm =NULL;
                            }

                            response_send(ptrespconn, true);
                        } else {
                            response_send(ptrespconn, false);
                        }
                    }

#if PLUG_DEVICE
                    else if (os_strcmp(pURL_Frame->pFilename, "switch") == 0) {
                        if (pParseBuffer != NULL) {
                            struct jsontree_context js;
                            jsontree_setup(&js, (struct jsontree_value *)&StatusTree, json_putchar);
                            json_parse(&js, pParseBuffer);
                            response_send(ptrespconn, true);
                        } else {
                            response_send(ptrespconn, false);
                        }
                    }

#endif

#if LIGHT_DEVICE
                    else if (os_strcmp(pURL_Frame->pFilename, "light") == 0) {
                        if (pParseBuffer != NULL) {
                            struct jsontree_context js;

                            jsontree_setup(&js, (struct jsontree_value *)&PwmTree, json_putchar);
                            json_parse(&js, pParseBuffer);

                            os_printf("rsp1:%u\n",PostCmdNeeRsp);
                            if(PostCmdNeeRsp == 0)
                                PostCmdNeeRsp = 1;
                            else
                                response_send(ptrespconn, true);
                        } else {
                            response_send(ptrespconn, false);
                        }
                    }
                    else if (os_strcmp(pURL_Frame->pFilename, "reset") == 0) {
                            response_send(ptrespconn, true);
                            extern  struct esp_platform_saved_param esp_param;
                            esp_param.activeflag = 0;
                            system_param_save_with_protect(ESP_PARAM_START_SEC, &esp_param, sizeof(esp_param));
                            
                            system_restore();
                            system_restart();
                    }

#endif
                    else {
                        response_send(ptrespconn, false);
                    }
                }
				else if(os_strcmp(pURL_Frame->pSelect, "upgrade") == 0 &&
					    os_strcmp(pURL_Frame->pCommand, "command") == 0){
					if (os_strcmp(pURL_Frame->pFilename, "start") == 0){
						response_send(ptrespconn, true);
						os_printf("local upgrade start\n");
						upgrade_lock = 1;
						system_upgrade_init();
						system_upgrade_flag_set(UPGRADE_FLAG_START);
						os_timer_disarm(&upgrade_check_timer);
						os_timer_setfn(&upgrade_check_timer, (os_timer_func_t *)upgrade_check_func, NULL);
						os_timer_arm(&upgrade_check_timer, 120000, 0);
					} else if (os_strcmp(pURL_Frame->pFilename, "reset") == 0) {

						response_send(ptrespconn, true);
						os_printf("local upgrade restart\n");
						system_upgrade_reboot();
					} else {
						response_send(ptrespconn, false);
					}
				}else {
					response_send(ptrespconn, false);
                }
                 break;
        }

        if (precvbuffer != NULL){
        	os_free(precvbuffer);
        	precvbuffer = NULL;
        }
        os_free(pURL_Frame);
        pURL_Frame = NULL;
        _temp_exit:
            ;
    }
    else if(upgrade_lock == 1){
    	local_upgrade_download(ptrespconn,pusrdata, length);
		if (precvbuffer != NULL){
			os_free(precvbuffer);
			precvbuffer = NULL;
		}
		os_free(pURL_Frame);
		pURL_Frame = NULL;
    }
}

/******************************************************************************
 * FunctionName : webserver_recon
 * Description  : the connection has been err, reconnection
 * Parameters   : arg -- Additional argument to pass to the callback function
 * Returns      : none
*******************************************************************************/
LOCAL ICACHE_FLASH_ATTR
void webserver_recon(void *arg, sint8 err)
{
    struct espconn *pesp_conn = arg;

    os_printf("webserver's %d.%d.%d.%d:%d err %d reconnect\n", pesp_conn->proto.tcp->remote_ip[0],
    		pesp_conn->proto.tcp->remote_ip[1],pesp_conn->proto.tcp->remote_ip[2],
    		pesp_conn->proto.tcp->remote_ip[3],pesp_conn->proto.tcp->remote_port, err);
}

/******************************************************************************
 * FunctionName : webserver_recon
 * Description  : the connection has been err, reconnection
 * Parameters   : arg -- Additional argument to pass to the callback function
 * Returns      : none
*******************************************************************************/
LOCAL ICACHE_FLASH_ATTR
void webserver_discon(void *arg)
{
    struct espconn *pesp_conn = arg;

    os_printf("webserver's %d.%d.%d.%d:%d disconnect\n", pesp_conn->proto.tcp->remote_ip[0],
        		pesp_conn->proto.tcp->remote_ip[1],pesp_conn->proto.tcp->remote_ip[2],
        		pesp_conn->proto.tcp->remote_ip[3],pesp_conn->proto.tcp->remote_port);
}

/******************************************************************************
 * FunctionName : user_accept_listen
 * Description  : server listened a connection successfully
 * Parameters   : arg -- Additional argument to pass to the callback function
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
webserver_listen(void *arg)
{
    struct espconn *pesp_conn = arg;

    espconn_regist_recvcb(pesp_conn, webserver_recv);
    espconn_regist_reconcb(pesp_conn, webserver_recon);
    espconn_regist_disconcb(pesp_conn, webserver_discon);
}

/******************************************************************************
 * FunctionName : user_webserver_init
 * Description  : parameter initialize as a server
 * Parameters   : port -- server port
 * Returns      : none
*******************************************************************************/
void ICACHE_FLASH_ATTR
user_webserver_init(uint32 port)
{
    LOCAL struct espconn esp_conn;
    LOCAL esp_tcp esptcp;

    esp_conn.type = ESPCONN_TCP;
    esp_conn.state = ESPCONN_NONE;
    esp_conn.proto.tcp = &esptcp;
    esp_conn.proto.tcp->local_port = port;
    espconn_regist_connectcb(&esp_conn, webserver_listen);

#ifdef SERVER_SSL_ENABLE
    espconn_secure_set_default_certificate(default_certificate, default_certificate_len);
    espconn_secure_set_default_private_key(default_private_key, default_private_key_len);
    espconn_secure_accept(&esp_conn);
#else
    espconn_accept(&esp_conn);
#endif
}
