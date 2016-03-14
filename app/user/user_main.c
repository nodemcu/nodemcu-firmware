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
#include "c_string.h"
#include "c_stdlib.h"
#include "flash_fs.h"
#include "user_interface.h"
#include "user_exceptions.h"
#include "user_modules.h"

#include "ets_sys.h"
#include "driver/uart.h"
#include "task/task.h"
#include "mem.h"

#ifdef LUA_USE_MODULES_RTCTIME
#include "rtc/rtctime.h"
#endif

#define SIG_LUA 0
#define SIG_UARTINPUT 1
#define TASK_QUEUE_LEN 4

static os_event_t *taskQueue;

/* Important: no_init_data CAN NOT be left as zero initialised, as that
 * initialisation will happen after user_start_trampoline, but before
 * the user_init, thus clobbering our state!
 */
static uint8_t no_init_data = 0xff;


/* Note: the trampoline *must* be explicitly put into the .text segment, since
 * by the time it is invoked the irom has not yet been mapped. This naturally
 * also goes for anything the trampoline itself calls.
 */
void TEXT_SECTION_ATTR user_start_trampoline (void)
{
   __real__xtos_set_exception_handler (
     EXCCAUSE_LOAD_STORE_ERROR, load_non_32_wide_handler);

#ifdef LUA_USE_MODULES_RTCTIME
  // Note: Keep this as close to call_user_start() as possible, since it
  // is where the cpu clock actually gets bumped to 80MHz.
  rtctime_early_startup ();
#endif


  /* Minimal early detection of missing esp_init_data.
   * If it is missing, the SDK will write its own and thus we'd end up
   * using that unless the flash size field is incorrect. This then leads
   * to different esp_init_data being used depending on whether the user
   * flashed with the right flash size or not (and the better option would
   * be to flash with an *incorrect* flash size, counter-intuitively).
   * To avoid that mess, we read out the flash size and do a test for
   * esp_init_data based on that size. If it's missing, flag for later.
   * If the flash size was incorrect, we'll end up fixing it all up
   * anyway, so this ends up solving the conundrum. Only remaining issue
   * is lack of spare code bytes in iram, so this is deliberately quite
   * terse and not as readable as one might like.
   */
  SPIFlashInfo sfi;
  SPIRead (0, &sfi, sizeof (sfi)); // Cache read not enabled yet, safe to use
  if (sfi.size < 2) // Compensate for out-of-order 4mbit vs 2mbit values
    sfi.size ^= 1;
  uint32_t flash_end_addr = (256 * 1024) << sfi.size;
  uint32_t init_data_hdr = 0xffffffff;
  SPIRead (flash_end_addr - 4 * SPI_FLASH_SEC_SIZE, &init_data_hdr, sizeof (init_data_hdr));
  no_init_data = (init_data_hdr == 0xffffffff);

  call_user_start ();
}

// +================== New task interface ==================+
static void start_lua(task_param_t param, uint8 priority) {
  char* lua_argv[] = { (char *)"lua", (char *)"-i", NULL };
  NODE_DBG("Task task_lua started.\n");
  lua_main( 2, lua_argv );
}

static void handle_input(task_param_t flag, uint8 priority) {
//  c_printf("HANDLE_INPUT: %u %u\n", flag, priority);          REMOVE
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
        // Write out init data at real location.
        no_init_data = true;
    }
#endif // defined(FLASH_SAFE_API)

    if (no_init_data)
    {
        NODE_ERR("Restore init data.\n");
        // Flash init data at FLASHSIZE - 0x04000 Byte.
        flash_init_data_default();
        // Flash blank data at FLASHSIZE - 0x02000 Byte.
        flash_init_data_blank();
        // Reboot to make the new data come into effect
        system_restart ();
    }

#if defined ( BUILD_SPIFFS )
    if (!fs_mount()) {
        // Failed to mount -- try reformat
	NODE_ERR("Formatting file system.\n");
        if (!fs_format()) {
            NODE_ERR( "\n*** ERROR ***: unable to format. FS might be compromised.\n" );
            NODE_ERR( "It is advised to re-flash the NodeMCU image.\n" );
        }
        // Note that fs_format leaves the file system mounted
    }
    // test_spiffs();
#endif
    // endpoint_setup();

    task_post_low(task_get_id(start_lua),'s');
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

    system_init_done_cb(nodemcu_init);
}
