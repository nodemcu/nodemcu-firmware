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
#include <fcntl.h>
#include "sdkconfig.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_spiffs.h"
#include "esp_netif.h"
#include "esp_vfs_dev.h"
#include "esp_vfs_cdcacm.h"
#include "esp_vfs_usb_serial_jtag.h"
#include "driver/usb_serial_jtag.h"
#include "nvs_flash.h"

#include "task/task.h"
#include "sections.h"
#include "nodemcu_esp_event.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#define SIG_LUA 0
#define SIG_UARTINPUT 1

// Line ending config from Kconfig
#if CONFIG_NEWLIB_STDIN_LINE_ENDING_CRLF
# define RX_LINE_ENDINGS_CFG ESP_LINE_ENDINGS_CRLF
#elif CONFIG_NEWLIB_STDIN_LINE_ENDING_CR
# define RX_LINE_ENDINGS_CFG ESP_LINE_ENDINGS_CR
#else
# define RX_LINE_ENDINGS_CFG ESP_LINE_ENDINGS_LF
#endif

#if CONFIG_NEWLIB_STDOUT_LINE_ENDING_CRLF
# define TX_LINE_ENDINGS_CFG ESP_LINE_ENDINGS_CRLF
#elif CONFIG_NEWLIB_STDOUT_LINE_ENDING_CR
# define TX_LINE_ENDINGS_CFG ESP_LINE_ENDINGS_CR
#else
# define TX_LINE_ENDINGS_CFG ESP_LINE_ENDINGS_LF
#endif


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

static task_handle_t lua_feed_task;


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
    printf("ERROR: failed to forward esp event %s/%ld", base, id);
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

static void nodemcu_init(void)
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


static bool have_console_on_data_cb(void)
{
#if CONFIG_ESP_CONSOLE_UART_DEFAULT || CONFIG_ESP_CONSOLE_UART_CUSTOM
  return uart_has_on_data_cb(CONFIG_ESP_CONSOLE_UART_NUM);
#else
  return false;
#endif
}


static void console_nodemcu_task(task_param_t param, task_prio_t prio)
{
  (void)prio;
  char c = (char)param;

  if (run_input)
    feed_lua_input(&c, 1);

#if CONFIG_ESP_CONSOLE_UART_DEFAULT || CONFIG_ESP_CONSOLE_UART_CUSTOM
  if (have_console_on_data_cb())
    uart_feed_data(CONFIG_ESP_CONSOLE_UART_NUM, &c, 1);
#endif

  // The IDF doesn't seem to honor setvbuf(stdout, NULL, _IONBF, 0) :(
  fsync(fileno(stdout));
}


static void console_task(void *)
{
  for (;;)
  {
    /* We can't use a large read buffer here as some console choices
     * (e.g. usb-serial-jtag) don't support read timeouts/partial reads,
     * which breaks the echo support and makes for a bad user experience.
     */
    char c;
    ssize_t n = read(fileno(stdin), &c, 1);
    if (n > 0 && (run_input || have_console_on_data_cb()))
    {
      if (!task_post_block_high(lua_feed_task, (task_param_t)c))
      {
        NODE_ERR("Lost console input data?!\n");
      }
    }
  }
}


static void console_init(void)
{
  fflush(stdout);
  fsync(fileno(stdout));

  /* Disable buffering */
  setvbuf(stdin, NULL, _IONBF, 0);
  setvbuf(stdout, NULL, _IONBF, 0);

  /* Disable non-blocking mode */
  fcntl(fileno(stdin), F_SETFL, 0);
  fcntl(fileno(stdout), F_SETFL, 0);

#if CONFIG_ESP_CONSOLE_UART_DEFAULT || CONFIG_ESP_CONSOLE_UART_CUSTOM
  /* Based on console/advanced example */

  esp_vfs_dev_uart_port_set_rx_line_endings(
    CONFIG_ESP_CONSOLE_UART_NUM, RX_LINE_ENDINGS_CFG);
  esp_vfs_dev_uart_port_set_tx_line_endings(
    CONFIG_ESP_CONSOLE_UART_NUM, TX_LINE_ENDINGS_CFG);

  /* Configure UART. Note that REF_TICK is used so that the baud rate remains
   * correct while APB frequency is changing in light sleep mode.
   */
  const uart_config_t uart_config = {
    .baud_rate = CONFIG_ESP_CONSOLE_UART_BAUDRATE,
    .data_bits = UART_DATA_8_BITS,
    .parity = UART_PARITY_DISABLE,
    .stop_bits = UART_STOP_BITS_1,
#if SOC_UART_SUPPORT_REF_TICK
    .source_clk = UART_SCLK_REF_TICK,
#elif SOC_UART_SUPPORT_XTAL_CLK
    .source_clk = UART_SCLK_XTAL,
#endif
  };
  /* Install UART driver for interrupt-driven reads and writes */
  uart_driver_install(CONFIG_ESP_CONSOLE_UART_NUM, 256, 0, 0, NULL, 0);
  uart_param_config(CONFIG_ESP_CONSOLE_UART_NUM, &uart_config);

  /* Tell VFS to use UART driver */
  esp_vfs_dev_uart_use_driver(CONFIG_ESP_CONSOLE_UART_NUM);

#elif CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG
  /* Based on @pjsg's work */

  esp_vfs_dev_usb_serial_jtag_set_rx_line_endings(RX_LINE_ENDINGS_CFG);
  esp_vfs_dev_usb_serial_jtag_set_tx_line_endings(TX_LINE_ENDINGS_CFG);

  usb_serial_jtag_driver_config_t usb_serial_jtag_config =
    USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
  /* Install USB-SERIAL-JTAG driver for interrupt-driven reads and write */
  usb_serial_jtag_driver_install(&usb_serial_jtag_config);

  esp_vfs_usb_serial_jtag_use_driver();
#elif CONFIG_ESP_CONSOLE_USB_CDC
  /* Based on console/advanced_usb_cdc */

  esp_vfs_dev_cdcacm_set_rx_line_endings(RX_LINE_ENDINGS_CFG);
  esp_vfs_dev_cdcacm_set_tx_line_endings(TX_LINE_ENDINGS_CFG);
#else
# error "Unsupported console type"
#endif

  xTaskCreate(
    console_task, "console", 1024, NULL, ESP_TASK_MAIN_PRIO+1, NULL);
}


void __attribute__((noreturn)) app_main(void)
{
  task_init();

  lua_feed_task = task_get_id(console_nodemcu_task);

  relayed_event_handled = xSemaphoreCreateBinary();
  relayed_event_task = task_get_id(handle_default_loop_event);

  esp_event_loop_create_default();
  esp_event_handler_register(
    ESP_EVENT_ANY_BASE,
    ESP_EVENT_ANY_ID,
    relay_default_loop_events,
    NULL);

  nodemcu_init ();

  nvs_flash_init ();
  esp_netif_init ();

  console_init();

  start_lua ();
  task_pump_messages ();
  __builtin_unreachable ();
}
