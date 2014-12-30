/******************************************************************************
 * Flash api for NodeMCU
*******************************************************************************/
#include "user_config.h"
#include "flash_api.h"
#include "spi_flash.h"

SPIFlashInfo *ICACHE_FLASH_ATTR
flash_get_info()
{
    static SPIFlashInfo spi_flash_info;
    static bool is_spi_flash_info_initialized = false;
    // Make the code more fast
    if (!is_spi_flash_info_initialized)
    {
        SPIRead(0, &spi_flash_info, sizeof(spi_flash_info));
        is_spi_flash_info_initialized = true;
    }
    return &spi_flash_info;
}

uint32_t ICACHE_FLASH_ATTR
flash_get_size_byte()
{
    static uint32_t flash_size = 0;
    // Make the code more fast
    if (flash_size == 0 )
    {
        SPIFlashInfo *p_spi_flash_info = flash_get_info();
        switch (p_spi_flash_info->size)
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
        default:
            // Unknown flash size, fall back mode.
            flash_size = 512 * 1024;
            break;
        }
    }
    return flash_size;
}

uint16_t ICACHE_FLASH_ATTR
flash_get_sec_num()
{
    static uint16_t result = 0;
    // Make the code more fast
    if (result == 0 )
    {
        result = flash_get_size_byte() / SPI_FLASH_SEC_SIZE;
    }
    return result;
}