/******************************************************************************
 * Flash api for NodeMCU
 * NodeMCU Team
 * 2014-12-31
*******************************************************************************/
#include "user_config.h"
#include "flash_api.h"
#include "spi_flash.h"
#include "c_stdio.h"

static volatile const uint8_t flash_init_data[128] ICACHE_STORE_ATTR ICACHE_RODATA_ATTR =
{
    0x05, 0x00, 0x04, 0x02, 0x05, 0x05, 0x05, 0x02, 0x05, 0x00, 0x04, 0x05, 0x05, 0x04, 0x05, 0x05,
    0x04, 0xFE, 0xFD, 0xFF, 0xF0, 0xF0, 0xF0, 0xE0, 0xE0, 0xE0, 0xE1, 0x0A, 0xFF, 0xFF, 0xF8, 0x00,
    0xF8, 0xF8, 0x52, 0x4E, 0x4A, 0x44, 0x40, 0x38, 0x00, 0x00, 0x01, 0x01, 0x02, 0x03, 0x04, 0x05,
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xE1, 0x0A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x93, 0x43, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

SPIFlashInfo flash_get_info(void)
{
    volatile SPIFlashInfo spi_flash_info ICACHE_STORE_ATTR;
    spi_flash_info = *((SPIFlashInfo *)(FLASH_MAP_START_ADDRESS));
    return spi_flash_info;
}

uint8_t flash_get_size(void)
{
    return flash_get_info().size;
}

uint32_t flash_get_size_byte(void)
{
    uint32_t flash_size = 0;
    switch (flash_get_info().size)
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
    case SIZE_64MBIT:
        // 64Mbit, 8MByte
        flash_size = 8 * 1024 * 1024;
    case SIZE_128MBIT:
        // 128Mbit, 16MByte
        flash_size = 16 * 1024 * 1024;
        break;
    default:
        // Unknown flash size, fall back mode.
        flash_size = 512 * 1024;
        break;
    }
    return flash_size;
}

bool flash_set_size(uint8_t size)
{
    // Dangerous, here are dinosaur infested!!!!!
    // Reboot required!!!
    // If you don't know what you're doing, your nodemcu may turn into stone ...
    uint8_t data[SPI_FLASH_SEC_SIZE] ICACHE_STORE_ATTR;
    spi_flash_read(0, (uint32 *)data, sizeof(data));
    SPIFlashInfo *p_spi_flash_info = (SPIFlashInfo *)(data);
    p_spi_flash_info->size = size;
    spi_flash_erase_sector(0);
    spi_flash_write(0, (uint32 *)data, sizeof(data));
    //p_spi_flash_info = flash_get_info();
    //p_spi_flash_info->size = size;
    return true;
}

bool flash_set_size_byte(uint32_t size)
{
    // Dangerous, here are dinosaur infested!!!!!
    // Reboot required!!!
    // If you don't know what you're doing, your nodemcu may turn into stone ...
    bool result = true;
    uint32_t flash_size = 0;
    switch (size)
    {
    case 256 * 1024:
        // 2Mbit, 256kByte
        flash_size = SIZE_2MBIT;
        flash_set_size(flash_size);
        break;
    case 512 * 1024:
        // 4Mbit, 512kByte
        flash_size = SIZE_4MBIT;
        flash_set_size(flash_size);
        break;
    case 1 * 1024 * 1024:
        // 8Mbit, 1MByte
        flash_size = SIZE_8MBIT;
        flash_set_size(flash_size);
        break;
    case 2 * 1024 * 1024:
        // 16Mbit, 2MByte
        flash_size = SIZE_16MBIT;
        flash_set_size(flash_size);
        break;
    case 4 * 1024 * 1024:
        // 32Mbit, 4MByte
        flash_size = SIZE_32MBIT;
        flash_set_size(flash_size);
        break;
    default:
        // Unknown flash size.
        result = false;
        break;
    }
    return result;
}

uint16_t flash_get_sec_num(void)
{
    return flash_get_size_byte() / SPI_FLASH_SEC_SIZE;
}

uint8_t flash_get_mode(void)
{
    SPIFlashInfo spi_flash_info = flash_get_info();
    switch (spi_flash_info.mode)
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
    return spi_flash_info.mode;
}

uint32_t flash_get_speed(void)
{
    uint32_t speed = 0;
    SPIFlashInfo spi_flash_info = flash_get_info();
    switch (spi_flash_info.speed)
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

bool flash_init_data_written(void)
{
    // FLASH SEC - 4
    uint32_t data[2] ICACHE_STORE_ATTR;
    if (SPI_FLASH_RESULT_OK == spi_flash_read((flash_get_sec_num() - 4) * SPI_FLASH_SEC_SIZE, (uint32 *)data, sizeof(data)))
    {
        if (data[0] == 0xFFFFFFFF && data[1] == 0xFFFFFFFF)
        {
            return false;
        }
    }
    return true;
}

bool flash_init_data_default(void)
{
    // FLASH SEC - 4
    // Dangerous, here are dinosaur infested!!!!!
    // Reboot required!!!
    // It will init system data to default!
    bool result = false;
    if (SPI_FLASH_RESULT_OK == spi_flash_erase_sector((flash_get_sec_num() - 4)))
    {
        if (SPI_FLASH_RESULT_OK == spi_flash_write((flash_get_sec_num() - 4) * SPI_FLASH_SEC_SIZE, (uint32 *)flash_init_data, 128))
        {
            result = true;
        }
    }
    return result;
}

bool flash_init_data_blank(void)
{
    // FLASH SEC - 2
    // Dangerous, here are dinosaur infested!!!!!
    // Reboot required!!!
    // It will init system config to blank!
    bool result = false;
    if ((SPI_FLASH_RESULT_OK == spi_flash_erase_sector((flash_get_sec_num() - 2))) &&
            (SPI_FLASH_RESULT_OK == spi_flash_erase_sector((flash_get_sec_num() - 1))))
    {
        result = true;
    }

    return result ;
}

bool flash_self_destruct(void)
{
    // Dangerous, Erase your flash. Good bye!
    SPIEraseChip();
    return true;
}

uint8_t byte_of_aligned_array(const uint8_t* aligned_array, uint32_t index)
{
    if( (((uint32_t)aligned_array)%4) != 0 ){
        NODE_DBG("aligned_array is not 4-byte aligned.\n");
        return 0;
    }
    uint32_t v = ((uint32_t *)aligned_array)[ index/4 ];
    uint8_t *p = (uint8_t *) (&v);
    return p[ (index%4) ];
}
