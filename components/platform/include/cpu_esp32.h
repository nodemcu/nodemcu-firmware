#ifndef _CPU_ESP32_H_
#define _CPU_ESP32_H_

#include "sdkconfig.h"
#include "esp_spi_flash.h"

#define NUM_UART SOC_UART_NUM

#define INTERNAL_FLASH_SECTOR_SIZE      SPI_FLASH_SEC_SIZE
#define INTERNAL_FLASH_WRITE_UNIT_SIZE  4
#define INTERNAL_FLASH_READ_UNIT_SIZE	  4

#define FLASH_SEC_NUM   (flash_safe_get_sec_num())

// Determine whether an address is in the flash-cache range
static inline bool is_cache_flash_addr (uint32_t addr)
{
  return addr >= 0x3F400000 && addr < 0x3FC00000;
}

#endif
