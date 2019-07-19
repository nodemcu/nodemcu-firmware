/******************************************************************************
 * Flash api for NodeMCU
 * NodeMCU Team
 * 2014-12-31
*******************************************************************************/
#include "user_config.h"
#include "flash_api.h"
#include "spi_flash.h"
#include <stdio.h>
#include <string.h>

uint32_t flash_detect_size_byte(void)
{
    // enable operations on whole physical flash, SDK might have restricted
    // the flash size already
    extern SpiFlashChip * flashchip;
    uint32 orig_chip_size = flashchip->chip_size;
    flashchip->chip_size = FLASH_SIZE_16MBYTE;

#define FLASH_BUFFER_SIZE_DETECT 32
    uint32_t dummy_size = FLASH_SIZE_256KBYTE;
    uint8_t data_orig[FLASH_BUFFER_SIZE_DETECT] ICACHE_STORE_ATTR = {0};
    uint8_t data_new[FLASH_BUFFER_SIZE_DETECT] ICACHE_STORE_ATTR = {0};
    if (SPI_FLASH_RESULT_OK == flash_read(0, (uint32 *)data_orig, FLASH_BUFFER_SIZE_DETECT))
    {
        dummy_size = FLASH_SIZE_256KBYTE;
        while ((dummy_size < FLASH_SIZE_16MBYTE) &&
                (SPI_FLASH_RESULT_OK == flash_read(dummy_size, (uint32 *)data_new, FLASH_BUFFER_SIZE_DETECT)) &&
                (0 != memcmp(data_orig, data_new, FLASH_BUFFER_SIZE_DETECT))
              )
        {
            dummy_size *= 2;
        }
    };

    // revert temporary setting
    flashchip->chip_size = orig_chip_size;

    return dummy_size;
#undef FLASH_BUFFER_SIZE_DETECT
}

static SPIFlashInfo spi_flash_info = {0};

SPIFlashInfo *flash_rom_getinfo(void)
{
    if (spi_flash_info.entry_point == 0) {
        spi_flash_read(0, (uint32 *)(& spi_flash_info), sizeof(spi_flash_info));
    }
    return &spi_flash_info;
}

uint8_t flash_rom_get_size_type(void)
{
    return flash_rom_getinfo()->size;
}

uint32_t flash_rom_get_size_byte(void)
{
    static uint32_t flash_size = 0;
    if (flash_size == 0)
    {
        switch (flash_rom_getinfo()->size)
        {
        case SIZE_2MBIT:
            // 2Mbit, 256kByte
            flash_size = 256 * 1024;
            break;
        case SIZE_4MBIT:
            // 4Mbit, 512kByte
            flash_size = 512 * 1024;
            break;
        case SIZE_8MBIT:
            // 8Mbit, 1MByte
            flash_size = 1 * 1024 * 1024;
            break;
        case SIZE_16MBIT:
            // 16Mbit, 2MByte
            flash_size = 2 * 1024 * 1024;
            break;
        case SIZE_32MBIT:
            // 32Mbit, 4MByte
            flash_size = 4 * 1024 * 1024;
            break;
        case SIZE_16MBIT_8M_8M:
            // 16Mbit, 2MByte
            flash_size = 2 * 1024 * 1024;
            break;
        case SIZE_32MBIT_8M_8M:
            // 32Mbit, 4MByte
            flash_size = 4 * 1024 * 1024;
            break;
        case SIZE_32MBIT_16M_16M:
            // 32Mbit, 4MByte
            flash_size = 4 * 1024 * 1024;
            break;
        case SIZE_64MBIT:
            // 64Mbit, 8MByte
            flash_size = 8 * 1024 * 1024;
            break;
        case SIZE_128MBIT:
            // 128Mbit, 16MByte
            flash_size = 16 * 1024 * 1024;
            break;
        default:
            // Unknown flash size, fall back mode.
            flash_size = 512 * 1024;
            break;
        }
    }
    return flash_size;
}

uint16_t flash_rom_get_sec_num(void)
{
    return ( flash_rom_get_size_byte() / (SPI_FLASH_SEC_SIZE) );
}

uint8_t flash_rom_get_mode(void)
{
    uint8_t mode = flash_rom_getinfo()->mode;
    switch (mode)
    {
    // Reserved for future use
    case MODE_QIO:
        break;
    case MODE_QOUT:
        break;
    case MODE_DIO:
        break;
    case MODE_DOUT:
        break;
    }
    return mode;
}

uint32_t flash_rom_get_speed(void)
{
    uint32_t speed = 0;
    uint8_t spi_speed = flash_rom_getinfo()->speed;
    switch (spi_speed)
    {
    case SPEED_40MHZ:
        // 40MHz
        speed = 40000000;
        break;
    case SPEED_26MHZ:
        //26.7MHz
        speed = 26700000;
        break;
    case SPEED_20MHZ:
        // 20MHz
        speed = 20000000;
        break;
    case SPEED_80MHZ:
        //80MHz
        speed = 80000000;
        break;
    }
    return speed;
}

uint8_t byte_of_aligned_array(const uint8_t *aligned_array, uint32_t index)
{
    if ( (((uint32_t)aligned_array) % 4) != 0 )
    {
        NODE_DBG("aligned_array is not 4-byte aligned.\n");
        return 0;
    }
    volatile uint32_t v = ((uint32_t *)aligned_array)[ index / 4 ];
    uint8_t *p = (uint8_t *) (&v);
    return p[ (index % 4) ];
}

uint16_t word_of_aligned_array(const uint16_t *aligned_array, uint32_t index)
{
    if ( (((uint32_t)aligned_array) % 4) != 0 )
    {
        NODE_DBG("aligned_array is not 4-byte aligned.\n");
        return 0;
    }
    volatile uint32_t v = ((uint32_t *)aligned_array)[ index / 2 ];
    uint16_t *p = (uint16_t *) (&v);
    return (index % 2 == 0) ? p[ 0 ] : p[ 1 ];
}

