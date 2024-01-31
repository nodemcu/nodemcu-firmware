#ifndef _CPU_ESP32_H_
#define _CPU_ESP32_H_

#include "sdkconfig.h"
#include "spi_flash_mmap.h"

#define NUM_UART SOC_UART_NUM

#define INTERNAL_FLASH_SECTOR_SIZE      SPI_FLASH_SEC_SIZE
#define INTERNAL_FLASH_WRITE_UNIT_SIZE  4
#define INTERNAL_FLASH_READ_UNIT_SIZE	  4

#endif
