/******************************************************************************
 * Flash api for NodeMCU
 * NodeMCU Team
 * 2014-12-31
*******************************************************************************/
#include "flash_api.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "../../bootloader/src/main/bootloader_config.h"

#define FLASH_HDR_ADDR 0x1000

static inline struct flash_hdr flash_load_rom_header (void)
{
  struct flash_hdr hdr;
  if (ESP_OK !=
      spi_flash_read (FLASH_HDR_ADDR, (uint32_t *)&hdr, sizeof (hdr)))
  {
    NODE_ERR("Failed to load flash header block!\n");
    abort();
  }
  return hdr;
}

static uint32_t flash_detect_size_byte(void)
{
#define DETECT_SZ 32
  uint32_t detected_size = FLASH_SIZE_1MBYTE;
  uint8_t data_orig[DETECT_SZ] PLATFORM_ALIGNMENT = {0};
  uint8_t data_new[DETECT_SZ] PLATFORM_ALIGNMENT = {0};
  // Detect read failure or wrap-around on flash read to find end of flash
  if (ESP_OK == spi_flash_read (0, (uint32_t *)data_orig, DETECT_SZ))
  {
    while ((detected_size < FLASH_SIZE_16MBYTE) &&
           (ESP_OK == spi_flash_read (
              detected_size, (uint32_t *)data_new, DETECT_SZ)) &&
           (0 != memcmp(data_orig, data_new, DETECT_SZ)))
    {
        detected_size *= 2;
    }
  };
  return detected_size;
#undef FLASH_BUFFER_SIZE_DETECT
}

uint32_t flash_safe_get_size_byte(void)
{
  static uint32_t flash_size = 0;
  if (flash_size == 0)
  {
    flash_size = flash_detect_size_byte();
  }
  return flash_size;
}


uint16_t flash_safe_get_sec_num(void)
{
  return (flash_safe_get_size_byte() / (SPI_FLASH_SEC_SIZE));
}

uint32_t flash_rom_get_size_byte(void)
{
  static uint32_t flash_size = 0;
  if (flash_size == 0)
  {
    switch (flash_load_rom_header ().spi_size)
    {
      default: // Unknown flash size, fall back mode.
      case SPI_SIZE_1MB:  flash_size = FLASH_SIZE_1MBYTE; break;
      case SPI_SIZE_2MB:  flash_size = FLASH_SIZE_2MBYTE; break;
      case SPI_SIZE_4MB:  flash_size = FLASH_SIZE_4MBYTE; break;
      case SPI_SIZE_8MB:  flash_size = FLASH_SIZE_8MBYTE; break;
      case SPI_SIZE_16MB: flash_size = FLASH_SIZE_16MBYTE; break;
    }
  }
  return flash_size;
}

static bool flash_rom_set_size_type(uint8_t size_code)
{
    // Dangerous, here are dinosaur infested!!!!!
    // Reboot required!!!
    // If you don't know what you're doing, your nodemcu may turn into stone ...
    NODE_DBG("\nBEGIN SET FLASH HEADER\n");
    struct flash_hdr *hdr = (struct flash_hdr *)malloc (SPI_FLASH_SEC_SIZE);
    if (!hdr)
      return false;

    if (ESP_OK == spi_flash_read (FLASH_HDR_ADDR, (uint32_t *)hdr, SPI_FLASH_SEC_SIZE))
    {
      hdr->spi_size = size_code;
      if (ESP_OK == spi_flash_erase_sector (FLASH_HDR_ADDR / SPI_FLASH_SEC_SIZE))
      {
        NODE_DBG("\nERASE SUCCESS\n");
      }
      if (ESP_OK == spi_flash_write(FLASH_HDR_ADDR, (uint32_t *)hdr, SPI_FLASH_SEC_SIZE))
      {
        NODE_DBG("\nWRITE SUCCESS, %u\n", size_code);
      }
    }
    free (hdr);
    NODE_DBG("\nEND SET FLASH HEADER\n");
    return true;
}


bool flash_rom_set_size_byte(uint32_t size)
{
    // Dangerous, here are dinosaur infested!!!!!
    // Reboot required!!!
    bool ok = true;
    uint8_t size_code = 0;
    switch (size)
    {
      case FLASH_SIZE_1MBYTE:  size_code = SPI_SIZE_1MB; break;
      case FLASH_SIZE_2MBYTE:  size_code = SPI_SIZE_2MB; break;
      case FLASH_SIZE_4MBYTE:  size_code = SPI_SIZE_4MB; break;
      case FLASH_SIZE_8MBYTE:  size_code = SPI_SIZE_8MB; break;
      case FLASH_SIZE_16MBYTE: size_code = SPI_SIZE_16MB; break;
      default: ok = false; break;
    }
    if (ok)
      ok = flash_rom_set_size_type (size_code);
    return ok;
}


uint16_t flash_rom_get_sec_num(void)
{
  return ( flash_rom_get_size_byte() / (SPI_FLASH_SEC_SIZE) );
}


uint8_t flash_rom_get_mode(void)
{
  return flash_load_rom_header ().spi_mode;
}


uint32_t flash_rom_get_speed(void)
{
  switch (flash_load_rom_header ().spi_speed)
  {
    case SPI_SPEED_40M: return 40000000;
    case SPI_SPEED_26M: return 26700000; // TODO: verify 26.7MHz
    case SPI_SPEED_20M: return 20000000;
    case SPI_SPEED_80M: return 80000000;
    default: break;
  }
  return 0;
}
