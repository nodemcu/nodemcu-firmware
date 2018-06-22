#ifndef __USER_CONFIG_H__
#define __USER_CONFIG_H__

// The firmware supports a range of Flash sizes, though 4 Mbyte seems to be
// the most common currently.  NodeMCU builds include a discovery function
// which is enabled by FLASH_AUTOSIZE, but you can override this by commenting
// this out and enabling the explicitly size, e.g. FLASH_4M.  Valid sizes are
// FLASH_512K, FLASH_1M, FLASH_2M, FLASH_4M, FLASH_8M, FLASH_16M.

#define FLASH_AUTOSIZE
//#define FLASH_4M


// The firmware now selects a baudrate of 115,200 by default, but the driver
// also includes automatic baud rate detection at start-up.  If you want to change
// the default rate then vaild rates are  300, 600, 1200, 2400, 4800, 9600, 19200,
// 31250, 38400, 57600, 74880, 115200, 230400, 256000, 460800 [, 921600, 1843200,
// 368640].  Note that the last 3 rates are not recommended as these might be
// unreliable, but 460800 seems to work well for most USB-serial devices.

#define BIT_RATE_DEFAULT BIT_RATE_115200
//#define BIT_RATE_AUTOBAUD


// Three separate build variants are now supported. The main difference is in the
// processing of numeric data types.  If LUA_NUMBER_INTEGRAL is defined, then
// all numeric calculations are done in integer, with divide being an integer
// operations, and decimal fraction constants are illegal.  Otherwise all
// numeric operations use floating point, though they are exact for integer
// expressions < 2^53.

// The main advantage of INTEGRAL builds is that the basic internal storage unit,
// the TValue, is 8 bytes long.  We have now reduced the size of FP TValues to
// 12 bytes rather than the previous 16 as this gives a material RAM saving with
// no performance loss.  However, you can define LUA_DWORD_ALIGNED_TVALUES and
// this will force 16 byte TValues on FP builds.

//#define LUA_NUMBER_INTEGRAL
//#define LUA_DWORD_ALIGNED_TVALUES


// The Lua Flash Store (LFS) allows you to store Lua code in Flash memory and
// the Lua VMS will execute this code directly from flash without needing any
// RAM overhead.  If you want to enable LFS then set the following define to
// the size of the store that you need.  This can be any multiple of 4kB up to
// a maximum 256Kb.

//#define LUA_FLASH_STORE 0x10000


// By default Lua executes the file init.lua at start up.  The following
// define allows you to replace this with an alternative startup.  Warning:
// you must protect this execution otherwise you will enter a panic loop;
// the simplest way is to wrap the action in a function invoked by a pcall.
// The example provided executes the LFS module "_init" at startup or fails
// through to the interactive prompt.

//#define LUA_INIT_STRING "pcall(function() node.flashindex'_init'() end)"


// NodeMCU supports two file systems: SPIFFS and FATFS, the first is available
// on all ESP8266 modules.  The latter requires extra H/W so is less common.
// If you use SPIFFS then there are a number of options which impact the
// RAM overhead and performance of the file system.

// If you use the spiffsimg tool to create your own FS images on your dev PC
// then we recommend that you fix the location and size of the FS, allowing
// some headroom for rebuilding flash images and LFS.  As an alternative to
// fixing the size of the FS, you can force the SPIFFS file system to end on
// the next 1Mb boundary.  This is useful for certain OTA scenarios.  In
// general, limiting the size of the FS only to what your application needs
// gives the fastest start-up and imaging times.

#define BUILD_SPIFFS
//#define SPIFFS_FIXED_LOCATION        0x100000
//#define SPIFFS_MAX_FILESYSTEM_SIZE    0x20000	
//#define SPIFFS_SIZE_1M_BOUNDARY
#define SPIFFS_CACHE 1          // Enable if you use you SPIFFS in R/W mode
#define SPIFFS_MAX_OPEN_FILES 4 // maximum number of open files for SPIFFS
#define FS_OBJ_NAME_LEN 31      // maximum length of a filename

//#define BUILD_FATFS


// The HTTPS stack requires client SSL to be enabled.  The SSL buffer size is
// used only for espconn-layer secure connections, and is ignored otherwise.
// Some HTTPS  applications require a larger buffer size to work.  See
// https://github.com/nodemcu/nodemcu-firmware/issues/1457 for details.
// The SHA2 and MD2 libraries are also optionally used by the crypto functions.
// The SHA1 and MD5 function are implemented in the ROM BIOS. The MD2 and SHA2
// are by firmware code, and can be enabled if you need this functionality.

//#define CLIENT_SSL_ENABLE
//#define MD2_ENABLE
#define SHA2_ENABLE
#define SSL_BUFFER_SIZE 5120


// GPIO_INTERRUPT_ENABLE needs to be defined if your application uses the
// gpio.trig() or related GPIO interrupt service routine code.  Likewise the
// GPIO interrupt hook is requited for a few modules such as rotary.  If you
// don't require this functionality, then commenting out these options out
// will remove any associated runtime overhead.

#define GPIO_INTERRUPT_ENABLE
#define GPIO_INTERRUPT_HOOK_ENABLE


// If your application uses the light sleep functions and you wish the
// firmware to manage timer rescheduling over sleeps (the CPU clock is
// suspended so timers get out of sync) then enable the following options

//#define ENABLE_TIMER_SUSPEND
//#define PMSLEEP_ENABLE


// The WiFi module optionally offers an enhanced level of WiFi connection
// management, using internal timer callbacks.  Whilst many Lua developers
// prefer to implement equivalent features in Lua, others will prefer the
// Wifi module to do this for them.  Uncomment the following to enable
// this functionality.  See the relevant WiFi module documentation for 
// further details, as the scope of these changes is not obvious.

//  Enable the wifi.startsmart() and wifi.stopsmart()  
//#define WIFI_SMART_ENABLE

//  Enable wifi.sta.config() event callbacks
#define WIFI_SDK_EVENT_MONITOR_ENABLE

//  Enable creation on the wifi.eventmon.reason table
#define WIFI_EVENT_MONITOR_DISCONNECT_REASON_LIST_ENABLE

//  Enable use of the WiFi.monitor sub-module
//#define LUA_USE_MODULES_WIFI_MONITOR


// Whilst the DNS client details can be configured through the WiFi API,
// the defaults can be exposed temporarily during start-up.  The following
// WIFI_STA options allow you to configure this in the firmware.  If the
// WIFI_STA_HOSTNAME is not defined then the hostname will default to
// to the last 3 octets (6 hexadecimal digits) of MAC address with the
// prefix "NODE-".  If it is defined then the hostname must only contain
// alphanumeric characters. If you are imaging multiple modules with this
// firmware then you must also define WIFI_STA_HOSTNAME_APPEND_MAC to
// append the last 3 octets of the MAC address.  Note that the total
// Hostname MUST be 32 chars or less.

//#define WIFI_STA_HOSTNAME "NodeMCU"
//#define WIFI_STA_HOSTNAME_APPEND_MAC


// If you use the enduser_setup module, then you can also set the default
// SSID when this module is running in AP mode.

#define ENDUSER_SETUP_AP_SSID "SetupGadget"


// The following sections are only relevent for those developers who are
// developing modules or core Lua changes and configure how extra diagnostics
// are enabled in the firmware. These should only be configured if you are
// building your own custom firmware and have full access to the firmware
// source code.

// Enabling DEVELOPMENT_TOOLS adds the asserts in LUA and also some useful
// extras to the node module. These are silent in normal operation and so can
// be enabled without any harm (except for the code size increase and slight
// slowdown). If you want to use the remote GDB to handle breaks and failed
// assertions then enable the DEVELOPMENT_USE GDB option.  A supplimentary
// define DEVELOPMENT_BREAK_ON_STARTUP_PIN allows you to define a GPIO pin,
// which if pulled low at start-up will immediately initiate a GDB session.

// The DEVELOP_VERSION option enables lots of debug output, and is normally
// only used by hardcore developers.

// These options can be enabled globally here or you can alternatively use
// the DEFINES variable in the relevant Makefile to set these on a per
// directory basis. If you do this then you can also set the corresponding
// compile options (-O0 -ggdb) on a per directory as well.

//#define DEVELOPMENT_TOOLS
//#define DEVELOPMENT_USE_GDB
//#define DEVELOPMENT_BREAK_ON_STARTUP_PIN 1
//#define DEVELOP_VERSION


// *** Heareafter, there be demons ***

// The remaining options are advanced configuration options and you should only
// change this if you have tracked the implications through the Firmware sources
// and understand the these.

#define LUA_TASK_PRIO             USER_TASK_PRIO_0
#define LUA_PROCESS_LINE_SIG      2
#define LUA_OPTIMIZE_DEBUG        2
#define READLINE_INTERVAL        80
#define STRBUF_DEFAULT_INCREMENT  3
#define LUA_USE_BUILTIN_DEBUG_MINIMAL // for debug.getregistry() and debug.traceback()

#ifdef DEVELOPMENT_TOOLS
#if defined(LUA_CROSS_COMPILER) || !defined(DEVELOPMENT_USE_GDB)
extern void luaL_assertfail(const char *file, int line, const char *message);
#define lua_assert(x)    ((x) ? (void) 0 : luaL_assertfail(__FILE__, __LINE__, #x))
#else
extern void luaL_dbgbreak(void);
#define lua_assert(x)    ((x) ? (void) 0 : luaL_dbgbreak())
#endif
#endif

#if !defined(LUA_NUMBER_INTEGRAL) && defined (LUA_DWORD_ALIGNED_TVALUES)
  #define LUA_PACK_TVALUES
#else
  #undef LUA_PACK_TVALUES
#endif

#ifdef DEVELOP_VERSION
#define NODE_DEBUG
#define COAP_DEBUG
#endif /* DEVELOP_VERSION */

#ifdef NODE_DEBUG
#define NODE_DBG dbg_printf
#else
#define NODE_DBG
#endif	/* NODE_DEBUG */

#define NODE_ERROR
#ifdef NODE_ERROR
#define NODE_ERR dbg_printf
#else
#define NODE_ERR
#endif	/* NODE_ERROR */

// #define GPIO_SAFE_NO_INTR_ENABLE
#define ICACHE_STORE_TYPEDEF_ATTR __attribute__((aligned(4),packed))
#define ICACHE_STORE_ATTR __attribute__((aligned(4)))
#define ICACHE_STRING(x) ICACHE_STRING2(x)
#define ICACHE_STRING2(x) #x
#define ICACHE_RAM_ATTR \
  __attribute__((section(".iram0.text." __FILE__ "." ICACHE_STRING(__LINE__))))
#define ICACHE_FLASH_RESERVED_ATTR \
  __attribute__((section(".irom.reserved." __FILE__ "." ICACHE_STRING(__LINE__)),\
                 used,unused,aligned(INTERNAL_FLASH_SECTOR_SIZE)))

#ifdef  GPIO_SAFE_NO_INTR_ENABLE
#define NO_INTR_CODE ICACHE_RAM_ATTR __attribute__ ((noinline))
#else
#define NO_INTR_CODE inline
#endif

#endif	/* __USER_CONFIG_H__ */
