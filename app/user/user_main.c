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
#include <stdio.h>
#include "vfs.h"
#include "flash_api.h"
#include "user_interface.h"
#include "user_modules.h"

#include "ets_sys.h"
#include "driver/uart.h"
#include "driver/input.h"
#include "task/task.h"
#include "mem.h"
#include "espconn.h"
#include "sections.h"

#ifdef LUA_USE_MODULES_RTCTIME
#include "rtc/rtctime.h"
#endif
extern int lua_main (void);

/* Contents of esp_init_data_default.bin */
extern const uint32_t init_data[], init_data_end[];
#define INIT_DATA_SIZE ((init_data_end - init_data)*sizeof(uint32_t))
__asm__(
  ".align 4\n"
  "init_data: .incbin \"" ESP_INIT_DATA_DEFAULT "\"\n"
  "init_data_end:\n"
);
extern const char _irom0_text_start[], _irom0_text_end[], _flash_used_end[];
#define IROM0_SIZE (_irom0_text_end - _irom0_text_start)


#define PRE_INIT_TEXT_ATTR        __attribute__((section(".p3.pre_init")))
#define IROM_PTABLE_ATTR          __attribute__((section(".irom0.ptable")))
#define USED_ATTR                 __attribute__((used))

#define PARTITION(n)  (SYSTEM_PARTITION_CUSTOMER_BEGIN + n)

#define SIZE_256K       0x00040000
#define SIZE_1024K      0x00100000
#define PT_CHUNK        0x00002000
#define PT_ALIGN(n) ((n + (PT_CHUNK-1)) & (~((PT_CHUNK-1))))
#define FLASH_BASE_ADDR ((char *) 0x40200000)

#define NODEMCU_PARTITION_EAGLEROM  PLATFORM_PARTITION(NODEMCU_EAGLEROM_PARTITION)
#define NODEMCU_PARTITION_IROM0TEXT PLATFORM_PARTITION(NODEMCU_IROM0TEXT_PARTITION)
#define NODEMCU_PARTITION_LFS       PLATFORM_PARTITION(NODEMCU_LFS0_PARTITION)
#define NODEMCU_PARTITION_SPIFFS    PLATFORM_PARTITION(NODEMCU_SPIFFS0_PARTITION)

#define MAX_PARTITIONS 20
#define WORDSIZE       sizeof(uint32_t)
#define PTABLE_SIZE    7   /** THIS MUST BE MATCHED TO NO OF PT ENTRIES BELOW **/
struct defaultpt {
  platform_rcr_t hdr;
  partition_item_t pt[PTABLE_SIZE+1]; // the +! is for the endmarker
  };
#define PT_LEN (NUM_PARTITIONS*sizeof(partition_item_t))
/*
 * See app/platform/platform.h for how the platform reboot config records are used
 * and these records are allocated.  The first record is a default partition table
 * and this is statically declared in compilation below.
 */
static const struct defaultpt rompt IROM_PTABLE_ATTR USED_ATTR  = {
  .hdr = {.len = sizeof(struct defaultpt)/WORDSIZE - 1,
          .id  = PLATFORM_RCR_PT},
  .pt  = {
    { NODEMCU_PARTITION_EAGLEROM,         0x00000,     0x0B000},
    { SYSTEM_PARTITION_RF_CAL,            0x0B000,      0x1000},
    { SYSTEM_PARTITION_PHY_DATA,          0x0C000,      0x1000},
    { SYSTEM_PARTITION_SYSTEM_PARAMETER,  0x0D000,      0x3000},
    { NODEMCU_PARTITION_IROM0TEXT,        0x10000,      0x0000},
    { NODEMCU_PARTITION_LFS,              0x0,          LUA_FLASH_STORE},
    { NODEMCU_PARTITION_SPIFFS,           0x0,          SPIFFS_MAX_FILESYSTEM_SIZE},
    {0,(uint32_t) &_irom0_text_end,0}
  }
};
//TODO: map the TLS server and client certs into NODEMCU_TLSCERT_PARTITION

static uint32_t first_time_setup(partition_item_t *pt, uint32_t n, uint32_t flash_size);
static void phy_data_setup (partition_item_t *pt, uint32_t n);
extern void _ResetHandler(void);

/*
 * The non-OS SDK prolog has been fundamentally revised in V3.  See SDK EN document
 * Partition Table.md for further discussion. This version of user_main.c is a
 * complete rework aligned to V3, with the redundant pre-V3 features removed.
 *
 * SDK V3 significantly reduces the RAM footprint required by the SDK and introduces
 * the use of a partition table (PT) to control flash allocation. The NodeMCU uses
 * this PT for overall allocation of its flash resources. The non_OS SDK calls the
 * user_pre_init() entry to do all of this startup configuration.  Note that this
 * runs with Icache enabled -- that is the IROM0 partition is already mapped to the
 * address space at 0x40210000 and so that most SDK services are available, such
 * as system_get_flash_size_map() which returns the valid flash size (including the
 * 8Mb and 16Mb variants).
 *
 * The first 4K page of IROM0 (flash offset 0x10000) is used to maintain a set of
 * Resource Communication Records (RCR) for inter-boot configuration using a NAND
 * write-once algo (see app/platform/platform.h).  One of the current records is the
 * SDK3.0 PT. This build statically compiles in an initial version at the start of
 * the PT, with a {0, _irom0_text_end,0} marker as the last record and some fields
 * also that need to be recomputed at runtime. This version is either replaced
 * by first boot processing after provisioning, or by a node.setpartitiontable()
 * API call. These replacement PTs are complete and can be passed directly for use
 * by the non-OS SDK.
 *
 * Note that we have released a host PC-base python tool, nodemcu-partition.py, to
 * configure the PT, etc during provisioning.
 */
void user_pre_init(void) {
#ifdef LUA_USE_MODULES_RTCTIME
  // Note: Keep this as close to call_user_start() as possible, since it
  // is where the cpu clock actually gets bumped to 80MHz.
    rtctime_early_startup ();
#endif
    partition_item_t *rcr_pt = NULL, *pt;
    enum flash_size_map fs_size_code = system_get_flash_size_map();
// Flash size lookup is SIZE_256K*2^N where N is as follows (see SDK/user_interface.h)
                                     /*   0   1   2   3   4   5   6   7   8   9  */
                                     /*  ½M  ¼M  1M  2M  4M  2M  4M  4M  8M 16M  */
    static char flash_size_scaler[] = "\001\000\002\003\004\003\004\004\005\006";
    uint32_t flash_size = SIZE_256K << flash_size_scaler[fs_size_code];

    uint32_t i = platform_rcr_read(PLATFORM_RCR_PT, (void **) &rcr_pt);
    uint32_t n = i / sizeof(partition_item_t);

    if (flash_size < SIZE_1024K) {
        os_printf("Flash size (%u) too small to support NodeMCU\n", flash_size);
        return;
    } else {
        os_printf("system SPI FI size:%u, Flash size: %u\n", fs_size_code, flash_size );
    }

    pt = os_malloc_iram(i);  // We will work on and register a copy of the PT in iRAM
    // Return if anything is amiss; The SDK will halt if the PT hasn't been registered
    if ( !rcr_pt || !pt || n * sizeof(partition_item_t) != i) {
        return;
    }
    os_memcpy(pt, rcr_pt, i);

    if (pt[n-1].type == 0) {
        // If the last PT entry is a {0,XX,0} end marker, then we need first time setup
        n = first_time_setup(pt, n-1, flash_size); // return n because setup might shrink the PT
    }

    if (platform_rcr_read(PLATFORM_RCR_PHY_DATA, NULL)!=0) {
        phy_data_setup(pt, n);
    }

    // Now register the partition and return
// for (i=0;i<n;i++) os_printf("P%d: %3d %06x %06x\n", i, pt[i].type, pt[i].addr, pt[i].size);
    if( fs_size_code > 1 && system_partition_table_regist(pt, n, fs_size_code)) {
        return;
    }
    os_printf("Invalid system partition table\n");
    while (1) {};
}

/*
 * If the PLATFORM_RCR_PT record doesn't exist then the PHY_DATA partition might
 * not have been initialised.  This must be set to the proper default init data
 * otherwise the SDK will halt on the "rf_cal[0] !=0x05,is 0xFF" error.
 */
static void phy_data_setup (partition_item_t *pt, uint32_t n) {
    uint8_t  header[sizeof(uint32_t)] = {0};
    int i;

    for (i = 0; i < n; i++) {
        if (pt[i].type == SYSTEM_PARTITION_PHY_DATA) {
            uint32_t addr = pt[i].addr;
            platform_s_flash_read(header, addr, sizeof(header));
            if (header[0] != 0x05) {
                uint32_t sector = pt[i].addr/INTERNAL_FLASH_SECTOR_SIZE;
                if (platform_flash_erase_sector(sector) == PLATFORM_OK) {
                    os_printf("Writing Init Data to 0x%08x\n",addr);
                    platform_s_flash_write(init_data, addr, INIT_DATA_SIZE);
                }
            }
            // flag setup complete so we don't retry this every boot
            platform_rcr_write(PLATFORM_RCR_PHY_DATA, &addr, 0);
            return;
        }
    }
    // If the PHY_DATA doesn't exist or the write fails then the
    // SDK will raise the rf_cal error anyway, so just return.
}

/*
 * First time setup does the one-off PT calculations and checks.  If these are OK,
 * then writes back a new RCR for the updated PT and triggers a reboot. It returns
 * on failure.
 */
static uint32_t first_time_setup(partition_item_t *pt, uint32_t n, uint32_t flash_size) {
    int i, j, last = 0, newn = n;
    /*
    * Scan down the PT adjusting and 0 entries to sensible defaults.  Also delete any
    * zero-sized partitions (as the SDK barfs on these).
    */
    for (i = 0, j = 0; i < n; i ++) {
        partition_item_t *p = pt + i;
        switch (p->type) {

          case NODEMCU_PARTITION_IROM0TEXT:
            // If the IROM0 partition size is 0 then compute from the IROM0_SIZE. Note
            // that the size in the end-marker is used by the nodemcu-partition.py
            // script and not here.
            if (p->size == 0) {
                p->size = PT_ALIGN(IROM0_SIZE);
            }
            break;

          case NODEMCU_PARTITION_LFS:
            // Properly align the LFS partition size and make it consecutive to
            // the previous partition.
            p->size = PT_ALIGN(p->size);
            if (p->addr == 0)
                p->addr = last;
            break;

          case NODEMCU_PARTITION_SPIFFS:
            if (p->size == ~0x0 && p->addr == 0) {
                // This allocate all the remaining flash to SPIFFS
                p->addr = last;
                p->size = flash_size - last;
            } else if (p->size == ~0x0) {
                p->size = flash_size - p->addr;
           }  else if (p->addr == 0) {
                // if the is addr not specified then start SPIFFS at 1Mb
                // boundary if the size will fit otherwise make it consecutive
                // to the previous partition.
                p->addr = (p->size <= flash_size - 0x100000) ? 0x100000 : last;
            }
        }

        if (p->size == 0) {
            // Delete 0-sized partitions as the SDK barfs on these
            newn--;
        } else {
            // Do consistency tests on the partition
            if (p->addr & (INTERNAL_FLASH_SECTOR_SIZE - 1) ||
                p->size & (INTERNAL_FLASH_SECTOR_SIZE - 1) ||
                p->addr < last ||
                p->addr + p->size > flash_size) {
                os_printf("Partition %u invalid alignment\n", i);
                while(1) {/*system_soft_wdt_feed ();*/}
            }
            if (j < i)   // shift the partition down if we have any deleted slots
                pt[j] = *p;
//os_printf("Partition %d: %04x %06x %06x\n", j, p->type, p->addr, p->size);
            j++;
            last = p->addr + p->size;
        }
    }

    platform_rcr_write(PLATFORM_RCR_PT, pt, newn*sizeof(partition_item_t));
    ets_delay_us(5000);
    _ResetHandler(); // Trigger reset; the new PT will be loaded on reboot
}

uint32 ICACHE_RAM_ATTR user_iram_memory_is_enabled(void) {
    return FALSE;  // NodeMCU runs like a dog if iRAM is enabled
}

void nodemcu_init(void) {
   NODE_DBG("Task task_lua starting.\n");
   // Call the Lua bootstrap startup directly.  This uses the task interface
   // internally to carry out the main lua libraries initialisation.
   if(lua_main())
     lua_main();  // If it returns true then LFS restart is needed
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
void user_init(void) {
#ifdef LUA_USE_MODULES_RTCTIME
    rtctime_late_startup ();
#endif
    if( platform_init() != PLATFORM_OK ) {
        // This should never happen
        NODE_DBG("Can not init platform for modules.\n");
        return;
    }
    UartBautRate br = BIT_RATE_DEFAULT;
    uart_init (br, br);
#ifndef NODE_DEBUG
    system_set_os_print(0);
#endif
    system_init_done_cb(nodemcu_init);
}
#if 0
/*
 * The SDK now establishes exception handlers for EXCCAUSE errors: ILLEGAL,
 * INSTR_ERROR, LOAD_STORE_ERROR, PRIVILEGED, UNALIGNED, LOAD_PROHIBITED,
 * STORE_PROHIBITED.  These handlers are established in SDK/app_main.c.
 * LOAD_STORE_ERROR is handled by SDK/user_exceptions.o:load_non_32_wide_handler()
 * which is a fork of our version. The remaining are handled by a static function
 * at SDK:app+main.c:offset 0x0348.  This wrappoer is only needed for debugging.
 */
void __real__xtos_set_exception_handler (uint32_t cause, exception_handler_fn fn);
void __wrap__xtos_set_exception_handler (uint32_t cause, exception_handler_fn fn) {
    os_printf("Exception handler %x  %x\n", cause, fn);
    __real__xtos_set_exception_handler (cause, fn);
}
#endif
