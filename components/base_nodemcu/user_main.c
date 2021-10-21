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
#include "linput.h"
#include "platform.h"
#include <string.h>
#include <stdlib.h>
#include "sdkconfig.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_spiffs.h"
#include "nvs_flash.h"
#include "flash_api.h"

#include "task/task.h"
#include "sections.h"
#include "nodemcu_esp_event.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#define SIG_LUA 0
#define SIG_UARTINPUT 1

// We don't get argument size data from the esp_event dispatch, so it's
// not possible to copy and forward events from the default event queue
// to one running within our task context. To cope with this, we instead
// have to effectively make a blocking inter-task call, by having our
// default loop handler post a nodemcu task event with a pointer to the
// event data, and then *block* until that task event has been processed.
// This is less elegant than I would like, but trying to run the entire
// LVM in the context of the system default event loop RTOS task is an
// even worse idea, so here we are.
typedef struct {
  esp_event_base_t  event_base;
  int32_t           event_id;
  void             *event_data;
} relayed_event_t;
static task_handle_t     relayed_event_task;
static SemaphoreHandle_t relayed_event_handled;


// This function runs in the context of the system default event loop RTOS task
static void relay_default_loop_events(
  void *arg, esp_event_base_t base, int32_t id, void *data)
{
  (void)arg;
  relayed_event_t event = {
    .event_base = base,
    .event_id = id,
    .event_data = data,
  };
  _Static_assert(sizeof(&event) >= sizeof(task_param_t), "pointer-vs-int");
  // Only block if we actually posted the request, otherwise we'll deadlock!
  if (task_post_medium(relayed_event_task, (intptr_t)&event))
    xSemaphoreTake(relayed_event_handled, portMAX_DELAY);
  else
    printf("ERROR: failed to forward esp event %s/%d", base, id);
}


static void handle_default_loop_event(task_param_t param, task_prio_t prio)
{
  (void)prio;
  const relayed_event_t *event = (const relayed_event_t *)param;

  nodemcu_esp_event_reg_t *evregs = &_esp_event_cb_table_start;
  for (; evregs < &_esp_event_cb_table_end; ++evregs)
  {
    bool event_base_match =
      (evregs->event_base_ptr == NULL) || // ESP_EVENT_ANY_BASE marker
      (*evregs->event_base_ptr == event->event_base);
    bool event_id_match =
      (evregs->event_id == event->event_id) ||
      (evregs->event_id == ESP_EVENT_ANY_ID);

    if (event_base_match && event_id_match)
      evregs->callback(event->event_base, event->event_id, event->event_data);
  }
  xSemaphoreGive(relayed_event_handled);
}


// +================== New task interface ==================+
static void start_lua ()
{
  NODE_DBG("Task task_lua started.\n");
  if (lua_main()) // If it returns true then LFS restart is needed
    lua_main();
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
    const char *label = CONFIG_NODEMCU_DEFAULT_SPIFFS_LABEL;

    esp_vfs_spiffs_conf_t spiffs_cfg = {
      .base_path = "",
      .partition_label = (label && label[0]) ? label : NULL,
      .max_files = CONFIG_NODEMCU_MAX_OPEN_FILES,
      .format_if_mount_failed = true,
    };
    const char *reason = NULL;
    switch(esp_vfs_spiffs_register(&spiffs_cfg))
    {
      case ESP_OK: break;
      case ESP_ERR_NO_MEM:
        reason = "out of memory";
        break;
      case ESP_ERR_INVALID_STATE:
        reason = "already mounted, or encrypted";
        break;
      case ESP_ERR_NOT_FOUND:
        reason = "no SPIFFS partition found";
        break;
      case ESP_FAIL:
        reason = "failed to mount or format partition";
        break;
      default:
        reason = "unknown";
        break;
    }
    if (reason)
      printf("Failed to mount SPIFFS partition: %s\n", reason);
}


void __attribute__((noreturn)) app_main(void)
{
  task_init();

  relayed_event_handled = xSemaphoreCreateBinary();
  relayed_event_task = task_get_id(handle_default_loop_event);

  esp_event_loop_create_default();
  esp_event_handler_register(
    ESP_EVENT_ANY_BASE,
    ESP_EVENT_ANY_ID,
    relay_default_loop_events,
    NULL);

  platform_uart_start(CONFIG_ESP_CONSOLE_UART_NUM);
  setvbuf(stdout, NULL, _IONBF, 0);

  nodemcu_init ();

  nvs_flash_init ();
  esp_netif_init ();

  start_lua ();
  task_pump_messages ();
  __builtin_unreachable ();
}
