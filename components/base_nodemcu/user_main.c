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
#include "vfs.h"
#include "sdkconfig.h"
#include "esp_system.h"
#include "flash_api.h"

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

    if (flash_safe_get_size_byte() != flash_rom_get_size_byte()) {
        NODE_ERR("Incorrect flash size reported, adjusting...\n");
        // Fit hardware real flash size.
        flash_rom_set_size_byte(flash_safe_get_size_byte());

        // Reboot to get SDK to use (or write) init data at new location
        system_restart ();

        // Don't post the start_lua task, we're about to reboot...
        return;
    }

#if defined ( CONFIG_BUILD_SPIFFS )
    if (!vfs_mount("/FLASH", 0)) {
        // Failed to mount -- try reformat
	      NODE_ERR("Formatting file system. Please wait...\n");
        if (1 || !vfs_format()) { // FIXME
            NODE_ERR( "*** ERROR ***: unable to format. FS might be compromised.\n" );
            NODE_ERR( "It is advised to re-flash the NodeMCU image.\n" );
        }
        // Note that fs_format leaves the file system mounted
    }
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
      nodemcu_main, "nodemcu", 2560, 0, tskIDLE_PRIORITY +1, NULL);
}
