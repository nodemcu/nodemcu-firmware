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
#include "esp_event.h"
#include "esp_event_loop.h"
#include "nvs_flash.h"
#include "flash_api.h"

#include "driver/console.h"
#include "task/task.h"
#include "sections.h"
#include "nodemcu_esp_event.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#define SIG_LUA 0
#define SIG_UARTINPUT 1


static task_handle_t esp_event_task;
static QueueHandle_t esp_event_queue;


// The callback happens in the wrong task context, so we bounce the event
// into our own queue so they all get handled in the same context as the
// LVM, making life as easy as possible for us.
esp_err_t bounce_events(void *ignored_ctx, system_event_t *event)
{
  if (!event)
    return ESP_ERR_INVALID_ARG;

  if (!esp_event_task || !esp_event_queue)
    return ESP_ERR_INVALID_STATE; // too early!

  portBASE_TYPE ret = xQueueSendToBack (esp_event_queue, event, 0);
  if (ret != pdPASS)
  {
    NODE_ERR("failed to queue esp event %d", event->event_id);
    return ESP_FAIL;
  }

  // If the task_post() fails, it only means the event gets delayed, hence
  // we claim OK regardless.
  task_post_medium (esp_event_task, 0);
  return ESP_OK;
}


static void handle_esp_event (task_param_t param, task_prio_t prio)
{
  (void)param;
  (void)prio;

  system_event_t evt;
  while (xQueueReceive (esp_event_queue, &evt, 0) == pdPASS)
  {
    nodemcu_esp_event_reg_t *evregs;
    for (evregs = &esp_event_cb_table; evregs->callback; ++evregs)
    {
      if (evregs->event_id == evt.event_id)
        evregs->callback (&evt);
    }
  }
}


// +================== New task interface ==================+
static void start_lua ()
{
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
        esp_restart ();

        // Don't post the start_lua task, we're about to reboot...
        return;
    }

#if defined ( CONFIG_BUILD_SPIFFS )
    // This can take a while, so be nice and provide some feedback while waiting
    printf ("Mounting flash filesystem...\n");
    if (!vfs_mount("/FLASH", 0)) {
        // Failed to mount -- try reformat
	      NODE_ERR("Formatting file system. Please wait...\n");
        if (!vfs_format()) {
            NODE_ERR( "*** ERROR ***: unable to format. FS might be compromised.\n" );
            NODE_ERR( "It is advised to re-flash the NodeMCU image.\n" );
        }
        // Note that fs_format leaves the file system mounted
    }
#endif
}


void app_main (void)
{
  task_init();

  esp_event_queue =
    xQueueCreate (CONFIG_SYSTEM_EVENT_QUEUE_SIZE, sizeof (system_event_t));
  esp_event_task = task_get_id (handle_esp_event);

  input_task = task_get_id (handle_input);

  esp_event_loop_init(bounce_events, NULL);

  ConsoleSetup_t cfg;
  cfg.bit_rate  = CONFIG_CONSOLE_BIT_RATE;
  cfg.data_bits = CONSOLE_NUM_BITS_8;
  cfg.parity    = CONSOLE_PARITY_NONE;
  cfg.stop_bits = CONSOLE_STOP_BITS_1;
  cfg.auto_baud = 
#ifdef CONFIG_CONSOLE_BIT_RATE_AUTO
    true;
#else
    false;
#endif

  console_init (&cfg, input_task);
  setvbuf(stdout, NULL, _IONBF, 0);

  nodemcu_init ();

  nvs_flash_init ();
  tcpip_adapter_init ();

  start_lua ();
  task_pump_messages ();
  __builtin_unreachable ();
}
