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
// #define DEVELOP_VERSION
#define FULL_VERSION_FOR_USER

#define USE_OPTIMIZE_PRINTF

#ifdef DEVELOP_VERSION
#define NODE_DEBUG
#define COAP_DEBUG
#endif	/* DEVELOP_VERSION */

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

// #define BUILD_WOFS		1
#define BUILD_SPIFFS	1

// #define LUA_NUMBER_INTEGRAL

#define LUA_OPTRAM
#ifdef LUA_OPTRAM
#define LUA_OPTIMIZE_MEMORY			2
#else
#define LUA_OPTIMIZE_MEMORY         0
#endif	/* LUA_OPTRAM */

#define READLINE_INTERVAL	80

#ifdef DEVKIT_VERSION_0_9
#define KEY_SHORT_MS	200
#define KEY_LONG_MS		3000
#define KEY_SHORT_COUNT (KEY_SHORT_MS / READLINE_INTERVAL)
#define KEY_LONG_COUNT (KEY_LONG_MS / READLINE_INTERVAL)

#define LED_HIGH_COUNT_DEFAULT 10
#define LED_LOW_COUNT_DEFAULT 0
#endif

#endif	/* __USER_CONFIG_H__ */
