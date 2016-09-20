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
#include "sdkconfig.h"

#include "driver/console.h"
#include "task/task.h"
#include "sections.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define SIG_LUA 0
#define SIG_UARTINPUT 1


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

static task_handle_t input_task;

task_handle_t user_get_input_sig(void) {
  return input_task;
}

bool user_process_input(bool force) {
    return task_post_low(input_task, force);
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

// FIXME: this breaks horribly on the ESP32 at the moment; possibly all flash writes?
#if defined(FLASH_SAFE_API) && defined(__ESP8266__)
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


void app_main (void)
{
    input_task = task_get_id (handle_input);

    ConsoleSetup_t cfg;
    cfg.bit_rate  = CONFIG_CONSOLE_BIT_RATE;
    cfg.data_bits = CONSOLE_NUM_BITS_8;
    cfg.parity    = CONSOLE_PARITY_NONE;
    cfg.stop_bits = CONSOLE_STOP_BITS_1;
    cfg.auto_baud = CONFIG_CONSOLE_BIT_RATE_AUTO;

    console_init (&cfg, input_task);

    xTaskCreate (
      nodemcu_main, "nodemcu", 2048, 0, configTIMER_TASK_PRIORITY +1, NULL);
}
