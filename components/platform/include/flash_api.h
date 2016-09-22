#ifndef __FLASH_API_H__
#define __FLASH_API_H__
#include "platform.h"
#include "esp_spi_flash.h"

uint32_t flash_safe_get_size_byte(void);
uint16_t flash_safe_get_sec_num(void);

uint32_t flash_rom_get_size_byte(void);
    bool flash_rom_set_size_byte(uint32_t);
uint16_t flash_rom_get_sec_num(void);
 uint8_t flash_rom_get_mode(void);
uint32_t flash_rom_get_speed(void);

#define FLASH_SIZE_1MBYTE   ( 1 * 1024 * 1024)
#define FLASH_SIZE_2MBYTE   ( 2 * 1024 * 1024)
#define FLASH_SIZE_4MBYTE   ( 4 * 1024 * 1024)
#define FLASH_SIZE_8MBYTE   ( 8 * 1024 * 1024)
#define FLASH_SIZE_16MBYTE  (16 * 1024 * 1024)

#define flash_write spi_flash_write
#define flash_erase spi_flash_erase_sector
#define flash_read spi_flash_read

#endif // __FLASH_API_H__
