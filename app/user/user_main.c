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
#include "c_stdio.h"
#include "vfs.h"
#include "flash_api.h"
#include "user_interface.h"
#include "user_modules.h"

#include "ets_sys.h"
#include "driver/uart.h"
#include "task/task.h"
#include "mem.h"
#include "espconn.h"
#include "sections.h"

#ifdef LUA_USE_MODULES_RTCTIME
#include "rtc/rtctime.h"
#endif

static task_handle_t input_sig;
static uint8 input_sig_flag = 0;

/* Contents of esp_init_data_default.bin */
extern const uint32_t init_data[];
extern const uint32_t init_data_end[];
__asm__(
  /* Place in .text for same reason as user_start_trampoline */
  ".section \".rodata.dram\"\n"
  ".align 4\n"
  "init_data:\n"
  ".incbin \"" ESP_INIT_DATA_DEFAULT "\"\n"
  "init_data_end:\n"
  ".previous\n"
);

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

  /* Re-implementation of default init data deployment. The SDK does not
   * appear to be laying down its own version of init data anymore, so
   * we have to do it again. To see whether we need to, we read out
   * the flash size and do a test for esp_init_data based on that size.
   * If it's missing, we need to initialize it *right now* before the SDK
   * starts up and gets stuck at "rf_cal[0] !=0x05,is 0xFF".
   * If the size byte is wrong, then we'll end up fixing up the init data
   * again on the next boot, after we've corrected the size byte.
   * Only remaining issue is lack of spare code bytes in iram, so this
   * is deliberately quite terse and not as readable as one might like.
   */
  SPIFlashInfo sfi;

  // enable operations on >4MB flash chip
  extern SpiFlashChip * flashchip;
  uint32 orig_chip_size = flashchip->chip_size;
  flashchip->chip_size = FLASH_SIZE_16MBYTE;

  SPIRead (0, (uint32_t *)(&sfi), sizeof (sfi)); // Cache read not enabled yet, safe to use
  // handle all size entries
  switch (sfi.size) {
  case 0: sfi.size = 1; break; // SIZE_4MBIT
  case 1: sfi.size = 0; break; // SIZE_2MBIT
  case 5: sfi.size = 3; break; // SIZE_16MBIT_8M_8M
  case 6: // fall-through
  case 7: sfi.size = 4; break; // SIZE_32MBIT_8M_8M, SIZE_32MBIT_16M_16M
  case 8: sfi.size = 5; break; // SIZE_64MBIT
  case 9: sfi.size = 6; break; // SIZE_128MBIT
  default: break;
  }
  uint32_t flash_end_addr = (256 * 1024) << sfi.size;
  uint32_t init_data_hdr = 0xffffffff;
  uint32_t init_data_addr = flash_end_addr - 4 * SPI_FLASH_SEC_SIZE;
  SPIRead (init_data_addr, &init_data_hdr, sizeof (init_data_hdr));
  if (init_data_hdr == 0xffffffff)
  {
    SPIEraseSector (init_data_addr);
    SPIWrite (init_data_addr, init_data, 4 * (init_data_end - init_data));
  }

  // revert temporary setting
  flashchip->chip_size = orig_chip_size;

  call_user_start ();
}

// +================== New task interface ==================+
static void start_lua(task_param_t param, uint8 priority) {
  char* lua_argv[] = { (char *)"lua", (char *)"-i", NULL };
  NODE_DBG("Task task_lua started.\n");
  lua_main( 2, lua_argv );
  // Only enable UART interrupts once we've successfully started up,
  // otherwise the task queue might fill up with input events and prevent
  // the start_lua task from being posted.
  ETS_UART_INTR_ENABLE();
}

static void handle_input(task_param_t flag, uint8 priority) {
  (void)priority;
  if (flag & 0x8000) {
    input_sig_flag = flag & 0x4000 ? 1 : 0;
  }
  lua_handle_input (flag & 0x01);
}

bool user_process_input(bool force) {
    return task_post_low(input_sig, force);
}

void nodemcu_init(void) {
    NODE_ERR("\n");
    // Initialize platform first for lua modules.
    if( platform_init() != PLATFORM_OK )
    {
        // This should never happen
        NODE_DBG("Can not init platform for modules.\n");
        return;
    }
    uint32_t size_detected = flash_detect_size_byte();
    uint32_t size_from_rom = flash_rom_get_size_byte();
    if( size_detected != size_from_rom ) {
        NODE_ERR("Self adjust flash size. 0x%x (ROM) -> 0x%x (Detected)\n", 
                 size_from_rom, size_detected);
        // Fit hardware real flash size.
        flash_rom_set_size_byte(size_detected);

        system_restart ();
        // Don't post the start_lua task, we're about to reboot...
        return;
    }

#if 0
// espconn_secure_set_size() is not effective
// see comments for MBEDTLS_SSL_MAX_CONTENT_LEN in user_mbedtls.h
#if defined ( CLIENT_SSL_ENABLE ) && defined ( SSL_BUFFER_SIZE )
    espconn_secure_set_size(ESPCONN_CLIENT, SSL_BUFFER_SIZE);
#endif
#endif

#ifdef BUILD_SPIFFS
    if (!vfs_mount("/FLASH", 0)) {
        // Failed to mount -- try reformat
        dbg_printf("Formatting file system. Please wait...\n");
        if (!vfs_format()) {
            NODE_ERR( "\n*** ERROR ***: unable to format. FS might be compromised.\n" );
            NODE_ERR( "It is advised to re-flash the NodeMCU image.\n" );
        }
        // Note that fs_format leaves the file system mounted
    }
    // test_spiffs();
#endif
    // endpoint_setup();

    if (!task_post_low(task_get_id(start_lua),'s'))
      NODE_ERR("Failed to post the start_lua task!\n");
}

#ifdef LUA_USE_MODULES_WIFI
#include "../modules/wifi_common.h"

void user_rf_pre_init(void)
{
//set WiFi hostname before RF initialization (adds ~479 us to boot time)
  wifi_change_default_host_name();
}
#endif

#define EAGLE_FLASH_BIN_ADDR      (SYSTEM_PARTITION_CUSTOMER_BEGIN + 1)
#define EAGLE_IROM0TEXT_BIN_ADDR  (SYSTEM_PARTITION_CUSTOMER_BEGIN + 2)
static const partition_item_t pt_template[] ICACHE_RODATA_ATTR STORE_ATTR = {
    { EAGLE_FLASH_BIN_ADDR,     0x00000, 0x10000},
    { EAGLE_IROM0TEXT_BIN_ADDR, 0x10000, 0},
    { SYSTEM_PARTITION_RF_CAL, 0, 0x1000},
    { SYSTEM_PARTITION_PHY_DATA, 0, 0x1000},
    { SYSTEM_PARTITION_SYSTEM_PARAMETER, 0, 0x3000},
};

void ICACHE_FLASH_ATTR user_pre_init(void)
{
    partition_item_t pt[sizeof(pt_template)/sizeof(pt_template[0])];
    memcpy(pt, pt_template, sizeof(pt_template));

    pt[1].size = platform_flash_get_first_free_block_address(NULL)
               - pt[0].size;

    enum flash_size_map size_map = system_get_flash_size_map();
    switch (size_map) {
        case FLASH_SIZE_8M_MAP_512_512: // 2
            pt[2].addr = 0xfb000;
            pt[3].addr = 0xfc000;
            pt[4].addr = 0xfd000;
            break;
        case FLASH_SIZE_16M_MAP_512_512: // 3
        case FLASH_SIZE_16M_MAP_1024_1024: // 5
            pt[2].addr = 0x1fb000;
            pt[3].addr = 0x1fc000;
            pt[4].addr = 0x1fd000;
            break;
        case FLASH_SIZE_32M_MAP_512_512: // 4
        case FLASH_SIZE_32M_MAP_1024_1024: // 6
        case FLASH_SIZE_32M_MAP_2048_2048: // 7
            pt[2].addr = 0x3fb000;
            pt[3].addr = 0x3fc000;
            pt[4].addr = 0x3fd000;
            break;
        case FLASH_SIZE_64M_MAP_1024_1024: // 8
            // XXX No examples given in docs; best guess
            pt[2].addr = 0x7fb000;
            pt[3].addr = 0x7fc000;
            pt[4].addr = 0x7fd000;
            break;
        case FLASH_SIZE_128M_MAP_1024_1024:  // 9
            // XXX No examples given in docs; best guess
            pt[2].addr = 0xffb000;
            pt[3].addr = 0xffc000;
            pt[4].addr = 0xffd000;
            break;

        case FLASH_SIZE_4M_MAP_256_256: /* 0: Too small */
        case FLASH_SIZE_2M:             /* 1: Way too small */
        default:
            os_printf("Cannot construct partition table!");
            while(1);
    }

    if(!system_partition_table_regist(pt, sizeof(pt)/sizeof(pt[0]),size_map)) {
        os_printf("system_partition_table_regist fail\r\n");
        while(1);
    }
}

uint32 ICACHE_RAM_ATTR user_iram_memory_is_enabled(void)
{
    return 0;
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
    uart_init (br, br, input_sig, &input_sig_flag);

#ifndef NODE_DEBUG
    system_set_os_print(0);
#endif
    system_init_done_cb(nodemcu_init);
}
