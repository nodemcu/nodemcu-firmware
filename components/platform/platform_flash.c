#include "platform.h"
#include "platform_wdt.h"
#include "esp_flash.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

// ****************************************************************************
// Internal flash support functions

// Helper function: find the flash sector in which an address resides
// Return the sector number, as well as the start and end address of the sector
static uint32_t flashh_find_sector( uint32_t address, uint32_t *pstart, uint32_t *pend )
{
  // All the sectors in the flash have the same size, so just align the address
  uint32_t sect_id = address / INTERNAL_FLASH_SECTOR_SIZE;

  if( pstart )
    *pstart = sect_id * INTERNAL_FLASH_SECTOR_SIZE ;
  if( pend )
    *pend = ( sect_id + 1 ) * INTERNAL_FLASH_SECTOR_SIZE - 1;
  return sect_id;
}


uint32_t platform_flash_get_sector_of_address( uint32_t addr )
{
  return flashh_find_sector( addr, NULL, NULL );
}


uint32_t platform_flash_write( const void *from, uint32_t toaddr, uint32_t size )
{
  esp_err_t err = esp_flash_write(NULL, from, toaddr, size);
  if (err != ESP_OK)
    return 0;
  else
    return size;
}


uint32_t platform_flash_read( void *to, uint32_t fromaddr, uint32_t size )
{
  esp_err_t err = esp_flash_write(NULL, to, fromaddr, size);
  if (err != ESP_OK)
    return 0;
  else
    return size;
}


int platform_flash_erase_sector( uint32_t sector_id )
{
  platform_wdt_feed();

  uint32_t addr = sector_id * INTERNAL_FLASH_SECTOR_SIZE;
  esp_err_t err =
    esp_flash_erase_region(NULL, addr, INTERNAL_FLASH_SECTOR_SIZE);
  if (err == ESP_OK)
    return PLATFORM_OK;
  else
    return PLATFORM_ERR;
}

