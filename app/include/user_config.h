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

// This adds the asserts in LUA. It also adds some useful extras to the
// node module. This is all silent in normal operation and so can be enabled
// without any harm (except for the code size increase and slight slowdown)
//#define DEVELOPMENT_TOOLS

#ifdef DEVELOPMENT_TOOLS
extern void luaL_assertfail(const char *file, int line, const char *message);
#define lua_assert(x)    ((x) ? (void) 0 : luaL_assertfail(__FILE__, __LINE__, #x))
#endif

// This enables lots of debug output and changes the serial bit rate. This
// is normally only used by hardcore developers
// #define DEVELOP_VERSION
#ifdef DEVELOP_VERSION
#define NODE_DEBUG
#define COAP_DEBUG
#endif /* DEVELOP_VERSION */

#define BIT_RATE_DEFAULT BIT_RATE_115200

// This enables automatic baud rate detection at startup
#define BIT_RATE_AUTOBAUD

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

#define GPIO_INTERRUPT_ENABLE
#define GPIO_INTERRUPT_HOOK_ENABLE
// #define GPIO_SAFE_NO_INTR_ENABLE

#define ICACHE_STORE_TYPEDEF_ATTR __attribute__((aligned(4),packed))
#define ICACHE_STORE_ATTR __attribute__((aligned(4)))
#define ICACHE_RAM_ATTR __attribute__((section(".iram0.text")))
#ifdef  GPIO_SAFE_NO_INTR_ENABLE
#define NO_INTR_CODE ICACHE_RAM_ATTR __attribute__ ((noinline))
#else
#define NO_INTR_CODE inline
#endif

//#define CLIENT_SSL_ENABLE
//#define MD2_ENABLE
#define SHA2_ENABLE

#define BUILD_SPIFFS	1
#define SPIFFS_CACHE 1

// Uncomment this next line for fastest startup 
// It reduces the format time dramatically
// #define SPIFFS_MAX_FILESYSTEM_SIZE	32768
//
// You can force the spiffs file system to be at a fixed location
// #define SPIFFS_FIXED_LOCATION   	0x100000
//
// You can force the SPIFFS file system to end on the next !M boundary
// (minus the 16k parameter space). THis is useful for certain OTA scenarios
// #define SPIFFS_SIZE_1M_BOUNDARY

// #define LUA_NUMBER_INTEGRAL

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

/*
 * A valid hostname only contains alphanumeric and hyphen(-) characters, with no hyphens at first or last char
 * if WIFI_STA_HOSTNAME not defined: hostname will default to NODE-xxxxxx (xxxxxx being last 3 octets of MAC address)
 * if WIFI_STA_HOSTNAME defined: hostname must only contain alphanumeric characters
 * if WIFI_STA_HOSTNAME_APPEND_MAC not defined: Hostname MUST be 32 chars or less
 * if WIFI_STA_HOSTNAME_APPEND_MAC defined: Hostname MUST be 26 chars or less, since last 3 octets of MAC address will be appended
 * if defined hostname is invalid: hostname will default to NODE-xxxxxx (xxxxxx being last 3 octets of MAC address)
*/
//#define WIFI_STA_HOSTNAME "NodeMCU"
//#define WIFI_STA_HOSTNAME_APPEND_MAC

//#define WIFI_SMART_ENABLE

#define WIFI_STATION_STATUS_MONITOR_ENABLE
#define WIFI_SDK_EVENT_MONITOR_ENABLE
#define WIFI_EVENT_MONITOR_DISCONNECT_REASON_LIST_ENABLE

#define STRBUF_DEFAULT_INCREMENT 32

#endif	/* __USER_CONFIG_H__ */
