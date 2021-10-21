#include "platform.h"
#include "flash_api.h"
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

uint32_t platform_flash_get_num_sectors(void)
{
  return flash_safe_get_sec_num ();
}

uint32_t platform_flash_write( const void *from, uint32_t toaddr, uint32_t size )
{
#ifndef INTERNAL_FLASH_WRITE_UNIT_SIZE
  return platform_s_flash_write( from, toaddr, size );
#else // #ifindef INTERNAL_FLASH_WRITE_UNIT_SIZE
  uint32_t temp, rest, ssize = size;
  unsigned i;
  char tmpdata[ INTERNAL_FLASH_WRITE_UNIT_SIZE ];
  const uint8_t *pfrom = ( const uint8_t* )from;
  const uint32_t blksize = INTERNAL_FLASH_WRITE_UNIT_SIZE;
  const uint32_t blkmask = INTERNAL_FLASH_WRITE_UNIT_SIZE - 1;

  // Align the start
  if( toaddr & blkmask )
  {
    rest = toaddr & blkmask;
    temp = toaddr & ~blkmask; // this is the actual aligned address
    // memcpy( tmpdata, ( const void* )temp, blksize );
    platform_s_flash_read( tmpdata, temp, blksize );
    for( i = rest; size && ( i < blksize ); i ++, size --, pfrom ++ )
      tmpdata[ i ] = *pfrom;
    platform_s_flash_write( tmpdata, temp, blksize );
    if( size == 0 )
      return ssize;
    toaddr = temp + blksize;
  }
  // The start address is now a multiple of blksize
  // Compute how many bytes we can write as multiples of blksize
  rest = size & blkmask;
  temp = size & ~blkmask;
  // Program the blocks now
  if( temp )
  {
    platform_s_flash_write( pfrom, toaddr, temp );
    toaddr += temp;
    pfrom += temp;
  }
  // And the final part of a block if needed
  if( rest )
  {
    // memcpy( tmpdata, ( const void* )toaddr, blksize );
    platform_s_flash_read( tmpdata, toaddr, blksize );
    for( i = 0; size && ( i < rest ); i ++, size --, pfrom ++ )
      tmpdata[ i ] = *pfrom;
    platform_s_flash_write( tmpdata, toaddr, blksize );
  }
  return ssize;
#endif // #ifndef INTERNAL_FLASH_WRITE_UNIT_SIZE
}

uint32_t platform_flash_read( void *to, uint32_t fromaddr, uint32_t size )
{
#ifndef INTERNAL_FLASH_READ_UNIT_SIZE
  return platform_s_flash_read( to, fromaddr, size );
#else // #ifindef INTERNAL_FLASH_READ_UNIT_SIZE
  uint32_t temp, rest, ssize = size;
  unsigned i;
  char tmpdata[ INTERNAL_FLASH_READ_UNIT_SIZE ] __attribute__ ((aligned(INTERNAL_FLASH_READ_UNIT_SIZE)));
  uint8_t *pto = ( uint8_t* )to;
  const uint32_t blksize = INTERNAL_FLASH_READ_UNIT_SIZE;
  const uint32_t blkmask = INTERNAL_FLASH_READ_UNIT_SIZE - 1;

  // Align the start
  if( fromaddr & blkmask )
  {
    rest = fromaddr & blkmask;
    temp = fromaddr & ~blkmask; // this is the actual aligned address
    platform_s_flash_read( tmpdata, temp, blksize );
    for( i = rest; size && ( i < blksize ); i ++, size --, pto ++ )
      *pto = tmpdata[ i ];

    if( size == 0 )
      return ssize;
    fromaddr = temp + blksize;
  }
  // The start address is now a multiple of blksize
  // Compute how many bytes we can read as multiples of blksize
  rest = size & blkmask;
  temp = size & ~blkmask;
  // Program the blocks now
  if( temp )
  {
    platform_s_flash_read( pto, fromaddr, temp );
    fromaddr += temp;
    pto += temp;
  }
  // And the final part of a block if needed
  if( rest )
  {
    platform_s_flash_read( tmpdata, fromaddr, blksize );
    for( i = 0; size && ( i < rest ); i ++, size --, pto ++ )
      *pto = tmpdata[ i ];
  }
  return ssize;
#endif // #ifndef INTERNAL_FLASH_READ_UNIT_SIZE
}


/*
 * Assumptions:
 * > toaddr is INTERNAL_FLASH_WRITE_UNIT_SIZE aligned
 * > size is a multiple of INTERNAL_FLASH_WRITE_UNIT_SIZE
 */
uint32_t platform_s_flash_write( const void *from, uint32_t toaddr, uint32_t size )
{
  esp_err_t r;
  const uint32_t blkmask = INTERNAL_FLASH_WRITE_UNIT_SIZE - 1;
  uint32_t *apbuf = NULL;
  uint32_t fromaddr = (uint32_t)from;
  if( (fromaddr & blkmask ) || is_cache_flash_addr(fromaddr)) {
    apbuf = (uint32_t *)malloc(size);
    if(!apbuf)
      return 0;
    memcpy(apbuf, from, size);
  }
  r = flash_write(toaddr, apbuf?(uint32_t *)apbuf:(uint32_t *)from, size);
  if(apbuf)
    free(apbuf);
  if(ESP_OK == r)
    return size;
  else{
    NODE_ERR( "ERROR in flash_write: r=%d at %08X\n", ( int )r, ( unsigned )toaddr);
    return 0;
  }
}


/*
 * Assumptions:
 * > fromaddr is INTERNAL_FLASH_READ_UNIT_SIZE aligned
 * > size is a multiple of INTERNAL_FLASH_READ_UNIT_SIZE
 */
uint32_t platform_s_flash_read( void *to, uint32_t fromaddr, uint32_t size )
{
  if (size==0)
    return 0;

  esp_err_t r;

  const uint32_t blkmask = (INTERNAL_FLASH_READ_UNIT_SIZE - 1);
  if( ((uint32_t)to) & blkmask )
  {
    uint32_t size2=size-INTERNAL_FLASH_READ_UNIT_SIZE;
    uint32_t* to2=(uint32_t*)((((uint32_t)to)&(~blkmask))+INTERNAL_FLASH_READ_UNIT_SIZE);
    r = flash_read(fromaddr, to2, size2);
    if(ESP_OK == r)
    {
      memmove(to,to2,size2);
      char back[ INTERNAL_FLASH_READ_UNIT_SIZE ] __attribute__ ((aligned(INTERNAL_FLASH_READ_UNIT_SIZE)));
      r=flash_read(fromaddr+size2,(uint32_t*)back,INTERNAL_FLASH_READ_UNIT_SIZE);
      memcpy((uint8_t*)to+size2,back,INTERNAL_FLASH_READ_UNIT_SIZE);
    }
  }
  else
    r = flash_read(fromaddr, (uint32_t *)to, size);

  if(ESP_OK == r)
    return size;
  else{
    NODE_ERR( "ERROR in flash_read: r=%d at %08X sz %x\n", ( int )r, ( unsigned )fromaddr, size);
    return 0;
  }
}


int platform_flash_erase_sector( uint32_t sector_id )
{
  return flash_erase( sector_id ) == ESP_OK ? PLATFORM_OK : PLATFORM_ERR;
}

