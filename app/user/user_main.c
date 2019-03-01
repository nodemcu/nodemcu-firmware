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
  ".align 4\n"
  "init_data: .incbin \"" ESP_INIT_DATA_DEFAULT "\"\n"
  "init_data_end:\n"
);
extern const char _irom0_text_start[], _irom0_text_end[],_flash_used_end[];
#define IROM0_SIZE (_irom0_text_end - _irom0_text_start)

#define INIT_DATA_SIZE (init_data_end - init_data)

#define PRE_INIT_TEXT_ATTR        __attribute__((section(".p3.pre_init")))
#define IROM_PTABLE_ATTR          __attribute__((section(".irom0.ptable")))

#define PARTITION(n)  (SYSTEM_PARTITION_CUSTOMER_BEGIN + n)

#define SIZE_256K       0x00040000
#define SIZE_1024K      0x00100000
#define FLASH_BASE_ADDR ((char *) 0x40200000)

//TODO: map the TLS server and client certs into NODEMCU_TLSCERT_PARTITION
const partition_item_t partition_init_table[] IROM_PTABLE_ATTR = {
    { PARTITION(NODEMCU_EAGLEROM_PARTITION),     0x00000,     0x0B000},
    { SYSTEM_PARTITION_RF_CAL,                   0x0B000,      0x1000},
    { SYSTEM_PARTITION_PHY_DATA,                 0x0C000,      0x1000},
    { SYSTEM_PARTITION_SYSTEM_PARAMETER,         0x0D000,      0x3000},
    { PARTITION(NODEMCU_IROM0TEXT_PARTITION),    0x10000,      0x0000},
    { PARTITION(NODEMCU_LFS0_PARTITION),             0x0, LUA_FLASH_STORE},
    { PARTITION(NODEMCU_SPIFFS0_PARTITION), SPIFFS_FIXED_LOCATION, SPIFFS_MAX_FILESYSTEM_SIZE},
    {0,(uint32_t) &_irom0_text_end,0}
};
// The following enum must maintain the partition table order
enum partition {iram0=0, rf_call, phy_data, sys_parm, irom0, lfs, spiffs};
#define PTABLE_SIZE ((sizeof(partition_init_table)/sizeof(partition_item_t))-1)

/*
 * The non-OS SDK prolog has been fundamentally revised in V3.  See SDK EN document
 * Partition Table.md for further discussion. This version of user_main.c is a
 * complete rework aligned to V3, with the redundant pre-V3 features removed.
 *
 * SDK V3 significantly reduced the RAM footprint required by the SDK and introduces
 * the use of a partition table (PT) to control flash allocation. The NodeMCU uses
 * this PT for overall allocation of its flash resources.  A constant copy PT is
 * maintained at the start of IROM0 (flash offset 0x10000) to facilitate it
 * modification either in the firmware binary or in the flash itself. This is Flash
 * PT used during startup to create the live PT in RAM that is used by the SDK.
 *
 * Note that user_pre_init() runs with Icache enabled -- that is the IROM0 partition
 * is already mapped the address space at 0x40210000 and so that most SDK services
 * are available, such as system_get_flash_size_map() which returns the valid flash
 * size (including the 8Mb and 16Mb variants).
 */
static int setup_partition_table(partition_item_t *pt, uint32_t *n) {

// Flash size lookup is SIZE_256K*2^N where N is as follows (see SDK/user_interface.h)
    static char flash_size_scaler[] =
  /*   0   1   2   3   4   5   6   7   8   9  */
  /*  ½M  ¼M  1M  2M  4M  2M  4M  4M  8M 16M  */
   "\001\000\002\003\004\003\004\004\005\006";
    enum flash_size_map fs_size_code = system_get_flash_size_map();
    uint32_t flash_size = SIZE_256K << flash_size_scaler[fs_size_code];
    uint32_t first_free_flash_addr = partition_init_table[PTABLE_SIZE].addr
                                     - (uint32_t) FLASH_BASE_ADDR;
    int i,j;

    os_memcpy(pt, partition_init_table, PTABLE_SIZE * sizeof(*pt));


    if (flash_size < SIZE_1024K) {
        os_printf("Flash size (%u) too small to support NodeMCU\n", flash_size);
        return -1;
    } else {
        os_printf("system SPI FI size:%u, Flash size: %u\n", fs_size_code, flash_size );
    }

// Calculate the runtime sized partitions
    if (pt[irom0].size == 0) {
        pt[irom0].size    = first_free_flash_addr - pt[irom0].addr;
    }
    if (pt[lfs].addr == 0) {
        pt[lfs].addr      = pt[irom0].addr + pt[irom0].size;
    }
    if (pt[spiffs].addr == 0) {
        pt[spiffs].addr = pt[lfs].addr        + pt[lfs].size;
    }
    if (pt[spiffs].size == 0) {
      pt[sys_parm].size = flash_size          - pt[sys_parm].addr;
    }

//  Check that the phys data partition has been initialised and if not then do this
//  now to prevent the SDK halting on a "rf_cal[0] !=0x05,is 0xFF" error.
    uint32_t init_data_hdr = 0xffffffff, data_addr = pt[phy_data].addr;
    int status = spi_flash_read(data_addr, &init_data_hdr, sizeof (uint32_t));
    if (status == SPI_FLASH_RESULT_OK && *(char *)&init_data_hdr != 0x05) {
        uint32_t idata[INIT_DATA_SIZE];
        os_printf("Writing Init Data to 0x%08x\n",data_addr);
        spi_flash_erase_sector(data_addr/SPI_FLASH_SEC_SIZE);
        os_memcpy(idata, init_data, sizeof(idata));
        spi_flash_write(data_addr, idata, sizeof(idata));
        os_delay_us(1000);
    }

// Check partitions are page aligned and remove and any zero-length partitions.
// This must be done last as this might break the enum partition ordering.
    for (i = 0, j = 0; i < PTABLE_SIZE; i++) {
        const partition_item_t *p = pt + i;
        if ((p->addr| p->size) & (SPI_FLASH_SEC_SIZE-1)) {
            os_printf("Partitions must be flash page aligned\n");
            return -1;
        }
        if (p->size == 0)
          continue;
        if (j < i) {
          pt[j] = *p;
          p = pt + j;
        }
        os_printf("%2u: %08x %08x %08x\n", j, p->type, p->addr, p->size);
        j++;
    }

    *n = j;
    return fs_size_code;
}

void user_pre_init(void) {
#ifdef LUA_USE_MODULES_RTCTIME
  // Note: Keep this as close to call_user_start() as possible, since it
  // is where the cpu clock actually gets bumped to 80MHz.
    rtctime_early_startup ();
#endif
    static partition_item_t pt[PTABLE_SIZE];

    uint32_t pt_size;
    uint32_t fs_size_code = setup_partition_table(pt, &pt_size);
    if( fs_size_code > 0 && system_partition_table_regist(pt, pt_size, fs_size_code)) {
        return;
    }
    os_printf("system_partition_table_regist fail (%u)\n", fs_size_code);
    while(1);

}

uint32 ICACHE_RAM_ATTR user_iram_memory_is_enabled(void) {
    return FALSE;  // NodeMCU runs like a dog if iRAM is enabled
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

#ifdef BUILD_SPIFFS
    if (!vfs_mount("/FLASH", 0)) {
        // Failed to mount -- try reformat
        dbg_printf("Formatting file system. Please wait...\n");
        if (!vfs_format()) {
            NODE_ERR( "\n*** ERROR ***: unable to format. FS might be compromised.\n" );
            NODE_ERR( "It is advised to re-flash the NodeMCU image.\n" );
        }
    }
#endif

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

/*
 * The SDK now establishes exception handlers for EXCCAUSE errors: ILLEGAL,
 * INSTR_ERROR, LOAD_STORE_ERROR, PRIVILEGED, UNALIGNED, LOAD_PROHIBITED,
 * STORE_PROHIBITED.  These handlers are established in SDK/app_main.c.
 * LOAD_STORE_ERROR is handled by SDK/user_exceptions.o:load_non_32_wide_handler()
 * which is a fork of our version. The remaining are handled by a static function
 * at SDK:app+main.c:offset 0x0348.
 *
void __real__xtos_set_exception_handler (uint32_t cause, exception_handler_fn fn);
void __wrap__xtos_set_exception_handler (uint32_t cause, exception_handler_fn fn) {
    os_printf("Exception handler %x  %x\n", cause, fn);
    __real__xtos_set_exception_handler (cause, fn);
}
 */
