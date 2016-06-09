/******************************************************************************
 * Copyright 2013-2014 Espressif Systems (Wuxi)
 *
 * FileName: user_main.c
 *
 * Description: entry file of user application
 *
 * Modification history:
 *     2014/1/1, v1.0 create this file.
*******************************************************************************/
#include "lua.h"
#include "platform.h"
#include <string.h>
#include <stdlib.h>
#include "flash_fs.h"
#include "flash_api.h"
#include "user_interface.h"
#include "user_modules.h"
#include "rom.h"

#include "ets_sys.h"
#include "driver/uart.h"
#include "task/task.h"
#include "sections.h"
#include "mem.h"
#include "freertos/task.h"

#ifdef LUA_USE_MODULES_RTCTIME
#include "rtc/rtctime.h"
#endif

#define SIG_LUA 0
#define SIG_UARTINPUT 1


#ifdef __ESP32__
void system_soft_wdt_feed (void)
{
  // FIXME this shouldn't go here, and it needs to tickle hardware watchdog
}
#endif

extern void call_user_start (void);


/* Note: the trampoline *must* be explicitly put into the .text segment, since
 * by the time it is invoked the irom has not yet been mapped. This naturally
 * also goes for anything the trampoline itself calls.
 */
void TEXT_SECTION_ATTR user_start_trampoline (void)
{
#ifdef LUA_USE_MODULES_RTCTIME
  // Note: Keep this as close to call_user_start() as possible, since it
  // is where the cpu clock actually gets bumped to 80MHz.
  rtctime_early_startup ();
#endif

  /* The first thing call_user_start() does is switch the interrupt vector
   * base, which enables our 8/16bit load handler. */
  call_user_start ();
}

// +================== New task interface ==================+
static void start_lua(task_param_t param, task_prio_t prio) {
  (void)param;
  (void)prio;
  char* lua_argv[] = { (char *)"lua", (char *)"-i", NULL };
  NODE_DBG("Task task_lua started.\n");
  lua_main( 2, lua_argv );
}

static void handle_input(task_param_t flag, task_prio_t priority) {
  (void)priority;
  lua_handle_input (flag);
}

static task_handle_t input_sig;

task_handle_t user_get_input_sig(void) {
  return input_sig;
}

bool user_process_input(bool force) {
    return task_post_low(input_sig, force);
}

void nodemcu_init(void)
{
    NODE_ERR("\n");
    // Initialize platform first for lua modules.
    if( platform_init() != PLATFORM_OK )
    {
        // This should never happen
        NODE_DBG("Can not init platform for modules.\n");
        return;
    }

#if defined(FLASH_SAFE_API)
    if( flash_safe_get_size_byte() != flash_rom_get_size_byte()) {
        NODE_ERR("Self adjust flash size.\n");
        // Fit hardware real flash size.
        flash_rom_set_size_byte(flash_safe_get_size_byte());

        if( !fs_format() )
        {
            NODE_ERR( "\ni*** ERROR ***: unable to format. FS might be compromised.\n" );
            NODE_ERR( "It is advised to re-flash the NodeMCU image.\n" );
        }
        else{
            NODE_ERR( "format done.\n" );
        }
        fs_unmount();   // mounted by format.

        // Reboot to get SDK to use (or write) init data at new location
        system_restart ();

        // Don't post the start_lua task, we're about to reboot...
        return;
    }
#endif // defined(FLASH_SAFE_API)

#if defined ( BUILD_SPIFFS )
    fs_mount();
    // test_spiffs();
#endif

    task_post_low(task_get_id(start_lua), 0);
}


static void nodemcu_main (void *param)
{
  (void)param;

  nodemcu_init ();

  task_pump_messages ();
  __builtin_unreachable ();
}


/******************************************************************************
 * FunctionName : user_init
 * Description  : entry of user application, init user function here
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
void user_init(void)
{
#ifdef LUA_USE_MODULES_RTCTIME
    rtctime_late_startup ();
#endif

    UartBautRate br = BIT_RATE_DEFAULT;

    input_sig = task_get_id(handle_input);
    uart_init (br, br, input_sig);

#ifndef NODE_DEBUG
    system_set_os_print(0);
#endif

    // FIXME! Max supported RTOS stack size is 512 words (2k bytes), but
    // NodeMCU currently uses more than that. The game is on to find these
    // culprits, but this gcc doesn't do the -fstack-usage option :(
    xTaskCreate (
      nodemcu_main, "nodemcu", 1536, 0, configTIMER_TASK_PRIORITY +1, NULL);
}
