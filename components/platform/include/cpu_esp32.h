#ifndef _CPU_ESP32_H_
#define _CPU_ESP32_H_

#include "sdkconfig.h"
#include "esp_spi_flash.h"

#define NUM_UART 2

#if defined(CONFIG_FLASH_SIZE_512K)
# define FLASH_SEC_NUM  0x80  // 4MByte: 0x400, 2MByte: 0x200, 1MByte: 0x100, 512KByte: 0x80
#elif defined(CONFIG_FLASH_SIZE_1M)
# define FLASH_SEC_NUM  0x100
#elif defined(CONFIG_FLASH_SIZE_2M)
# define FLASH_SEC_NUM  0x200
#elif defined(CONFIG_FLASH_SIZE_4M)
# define FLASH_SEC_NUM  0x400
#elif defined(CONFIG_FLASH_SIZE_8M)
# define FLASH_SEC_NUM  0x800
#elif defined(CONFIG_FLASH_SIZE_16M)
# define FLASH_SEC_NUM  0x1000
#elif defined(CONFIG_FLASH_SIZE_AUTO)
# if defined(FLASH_SAFE_API)
#  define FLASH_SEC_NUM   (flash_safe_get_sec_num())
# else
#  define FLASH_SEC_NUM   (flash_rom_get_sec_num())
# endif // defined(FLASH_SAFE_API)
#else
# define FLASH_SEC_NUM  0x80
#endif


#define SYS_PARAM_SEC_NUM 4
#define SYS_PARAM_SEC_START (FLASH_SEC_NUM - SYS_PARAM_SEC_NUM)


#define INTERNAL_FLASH_SECTOR_SIZE      SPI_FLASH_SEC_SIZE
#define INTERNAL_FLASH_WRITE_UNIT_SIZE  4
#define INTERNAL_FLASH_READ_UNIT_SIZE	  4

#define INTERNAL_FLASH_SIZE             ( (SYS_PARAM_SEC_START) * INTERNAL_FLASH_SECTOR_SIZE )

#define IROM0_START_MAPPED_ADDR 0x3F400000
// TODO: tie this in with the partition table!
#define IROM0_START_FLASH_ADDR     0x40000
// TODO: might need to revamp all this once cache windows fully understood
#define INTERNAL_FLASH_MAPPED_ADDRESS   IROM0_START_MAPPED_ADDR


#if defined(FLASH_SAFE_API)
#define flash_write flash_safe_write
#define flash_erase flash_safe_erase_sector
#define flash_read flash_safe_read
#else
#define flash_write spi_flash_write
#define flash_erase spi_flash_erase_sector
#define flash_read spi_flash_read
#endif // defined(FLASH_SAFE_API)

#endif
