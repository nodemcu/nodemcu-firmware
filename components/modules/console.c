#include "module.h"
#include "platform.h"
#include "lauxlib.h"
#include "linput.h"
#include "serial_common.h"
#include "task/task.h"

#include "esp_vfs_cdcacm.h"
#include "driver/uart_vfs.h"
#include "driver/usb_serial_jtag.h"
#include "driver/usb_serial_jtag_vfs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>

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

typedef enum { NONINTERACTIVE, INTERACTIVE } console_mode_t;

static serial_input_cfg_t *cb_cfg;
static task_handle_t feed_lua_task;


// --- Console input task related -----------------------------------

static void console_feed_lua(task_param_t param, task_prio_t prio)
{
  (void)prio;
  char c = (char)param;

  if (run_input)
    feed_lua_input(&c, 1);

  if (serial_input_has_data_cb(cb_cfg))
    serial_input_feed_data(cb_cfg, &c, 1);

  // The IDF doesn't seem to honor setvbuf(stdout, NULL, _IONBF, 0) :(
  fflush(stdout);
  fsync(fileno(stdout));
}


static void console_task(void *)
{
  for (;;)
  {
    // TODO: Support linenoise editing here as an option?
    // The run_input switch would need to also control whether we do
    // linenoise or raw byte input, to allow for binary xfers.
    // But, we would have a big race condition here as the execution
    // of the last line happens after we've already started reading the
    // next one. We'd have to use a newer version of linenoise than what
    // the IDF has, so we get the async interface. Plus switch everything to
    // using select() before picking which input method we're using.
    // For the race condition, would it be sufficient to wait for the next
    // next prompt display to be reasonably certain it's switched?
    // But even the prompt handling would be problematic with linenoise as
    // that's fixed on linenoiseEditStart(). To solve that we'd need to
    // be running the console within the LVM task, synchronously, but we
    // can't do that because we need the LVM accessible to handle events.
    // These are incompatible design constraints, sigh.

    /* We can't use a large read buffer here as some console choices
     * (e.g. usb-serial-jtag) don't support read timeouts/partial reads,
     * which breaks the echo support and makes for a bad user experience.
     */
    char c;
    ssize_t n = read(fileno(stdin), &c, 1);
    if (n > 0 && (run_input || serial_input_has_data_cb(cb_cfg)))
    {
      if (!task_post_block_high(feed_lua_task, (task_param_t)c))
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

  uart_vfs_dev_port_set_rx_line_endings(
    CONFIG_ESP_CONSOLE_UART_NUM, RX_LINE_ENDINGS_CFG);
  uart_vfs_dev_port_set_tx_line_endings(
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
  uart_vfs_dev_use_driver(CONFIG_ESP_CONSOLE_UART_NUM);

#elif CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG
  /* Based on @pjsg's work */

  usb_serial_jtag_vfs_set_rx_line_endings(RX_LINE_ENDINGS_CFG);
  usb_serial_jtag_vfs_set_tx_line_endings(TX_LINE_ENDINGS_CFG);

  usb_serial_jtag_driver_config_t usb_serial_jtag_config =
    USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
  /* Install USB-SERIAL-JTAG driver for interrupt-driven reads and write */
  usb_serial_jtag_driver_install(&usb_serial_jtag_config);

  usb_serial_jtag_vfs_use_driver();
#elif CONFIG_ESP_CONSOLE_USB_CDC
  /* Based on console/advanced_usb_cdc */

  esp_vfs_dev_cdcacm_set_rx_line_endings(RX_LINE_ENDINGS_CFG);
  esp_vfs_dev_cdcacm_set_tx_line_endings(TX_LINE_ENDINGS_CFG);
#else
# error "Unsupported console type"
#endif

  xTaskCreate(
    console_task, "console", configMINIMAL_STACK_SIZE,
    NULL, ESP_TASK_MAIN_PRIO+1, NULL);
}


// --- Lua interface related ----------------------------------------

static int retrying_write(const char *buf, size_t len)
{
  size_t written = 0;
  while (written < len)
  {
    // At least the USB-Serial-JTAG appears to silently drop characters
    // sometimes when writing more than 255 bytes, so we break such strings
    // up into multiple calls as a workaround.
    const size_t MAX_LEN = 255;
    size_t left = len - written;
    size_t to_write = left > MAX_LEN ? MAX_LEN : left;
    size_t n = fwrite(buf + written, 1, to_write, stdout);
    // Additionally, we have to explicitly flush after each chunk we've written.
    fflush(stdout);
    fsync(fileno(stdout));

    if (n > 0)
      written += n;
    else if (ferror(stdout))
      break;
    else
      vTaskDelay(1);
  }
  return written;
}


// Lua: console.on("method", [number/char], function)
static int console_on(lua_State *L)
{
  return serial_input_register(L, cb_cfg);
}


// Lua: console.mode(onoff)
static int console_mode(lua_State *L)
{
  switch (luaL_checkint(L, 1))
  {
    case NONINTERACTIVE: run_input = false; break;
    case INTERACTIVE: run_input = true; break;
    default: luaL_error(L, "invalid mode");
  }
  return 0;
}


// Lua: console.write(str_or_num [, str_or_num2 ... ])
static int console_write(lua_State *L)
{
  int total = lua_gettop(L);
  for (int s = 1; s <= total; ++s)
  {
    if (lua_type(L, s) == LUA_TSTRING)
    {
      size_t len = 0;
      const char *buf = lua_tolstring(L, s, &len);
      retrying_write(buf, len);
    }
    else if (lua_isnumber(L, s))
    {
      int n = lua_tointeger(L, s);
      if (n < 0 || n > 255)
        return luaL_error(L, "invalid number");
      char ch = n;
      retrying_write(&ch, 1);
    }
  }
  return 0;
}


// Module function map
LROT_BEGIN(console, NULL, 0)
  LROT_FUNCENTRY( mode,               console_mode      )
  LROT_FUNCENTRY( on,                 console_on        )
  LROT_FUNCENTRY( write,              console_write     )
  LROT_NUMENTRY( INTERACTIVE,         INTERACTIVE       )
  LROT_NUMENTRY( NONINTERACTIVE,      NONINTERACTIVE    )
LROT_END(console, NULL, 0)


int luaopen_console( lua_State *L ) {
  cb_cfg = serial_input_new();
  if (!cb_cfg)
    return luaL_error(L, "out of mem");

  feed_lua_task = task_get_id(console_feed_lua);

  console_init();

  return 0;
}

NODEMCU_MODULE(CONSOLE, "console", console, luaopen_console);
