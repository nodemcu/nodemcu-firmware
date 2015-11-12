#ifndef __USER_CONFIG_H__
#define __USER_CONFIG_H__

// #define DEVKIT_VERSION_0_9 1 	// define this only if you use NodeMCU devkit v0.9

// #define FLASH_512K
// #define FLASH_1M
// #define FLASH_2M
// #define FLASH_4M
// #define FLASH_8M
// #define FLASH_16M
#define FLASH_AUTOSIZE
#define FLASH_SAFE_API

// Byte 107 of esp_init_data_default, only one of these 3 can be picked
#define ESP_INIT_DATA_ENABLE_READVDD33
//#define ESP_INIT_DATA_ENABLE_READADC
//#define ESP_INIT_DATA_FIXED_VDD33_VALUE 33

// #define DEVELOP_VERSION
#ifdef DEVELOP_VERSION
#define NODE_DEBUG
#define COAP_DEBUG
#define BIT_RATE_DEFAULT BIT_RATE_74880
#else
#define BIT_RATE_DEFAULT BIT_RATE_9600
#endif /* DEVELOP_VERSION */


#define NODE_ERROR

#ifdef NODE_DEBUG
#define NODE_DBG c_printf
#else
#define NODE_DBG
#endif	/* NODE_DEBUG */

#ifdef NODE_ERROR
#define NODE_ERR c_printf
#else
#define NODE_ERR
#endif	/* NODE_ERROR */

#define ICACHE_STORE_TYPEDEF_ATTR __attribute__((aligned(4),packed))
#define ICACHE_STORE_ATTR __attribute__((aligned(4)))
#define ICACHE_RAM_ATTR __attribute__((section(".iram0.text")))

#define CLIENT_SSL_ENABLE
#define GPIO_INTERRUPT_ENABLE
//#define MD2_ENABLE
#define SHA2_ENABLE

// #define BUILD_WOFS		1
#define BUILD_SPIFFS	1

#define SPIFFS_CACHE 1

// #define LUA_NUMBER_INTEGRAL

#define LUA_OPTRAM
#ifdef LUA_OPTRAM
#define LUA_OPTIMIZE_MEMORY			2
#else
#define LUA_OPTIMIZE_MEMORY         0
#endif	/* LUA_OPTRAM */

#define READLINE_INTERVAL 80
#define LUA_TASK_PRIO USER_TASK_PRIO_0
#define LUA_PROCESS_LINE_SIG 2
#define LUA_OPTIMIZE_DEBUG      2

#ifdef DEVKIT_VERSION_0_9
#define KEYLED_INTERVAL	80

#define KEY_SHORT_MS	200
#define KEY_LONG_MS		3000
#define KEY_SHORT_COUNT (KEY_SHORT_MS / READLINE_INTERVAL)
#define KEY_LONG_COUNT (KEY_LONG_MS / READLINE_INTERVAL)

#define LED_HIGH_COUNT_DEFAULT 10
#define LED_LOW_COUNT_DEFAULT 0
#endif

#define ENDUSER_SETUP_AP_SSID "SetupGadget"

#define STRBUF_DEFAULT_INCREMENT 32

#endif	/* __USER_CONFIG_H__ */
