// Common code for all backends

#include "platform.h"
#include "common.h"
#include "c_string.h"
#include "c_stdio.h"

void cmn_platform_init(void)
{

}

// ****************************************************************************
// Internal flash support functions

// This symbol must be exported by the linker command file and must reflect the
// TOTAL size of flash used by the eLua image (not only the code and constants,
// but also .data and whatever else ends up in the eLua image). FS will start
// at the next usable (aligned to a flash sector boundary) address after
// flash_used_size.

// extern char flash_used_size[];
extern char _flash_used_end[];

// Helper function: find the flash sector in which an address resides
// Return the sector number, as well as the start and end address of the sector
static uint32_t flash_find_sector( uint32_t address, uint32_t *pstart, uint32_t *pend )
{
#ifdef INTERNAL_FLASH_SECTOR_SIZE
  // All the sectors in the flash have the same size, so just align the address
  uint32_t sect_id = address / INTERNAL_FLASH_SECTOR_SIZE;

  if( pstart )
    *pstart = sect_id * INTERNAL_FLASH_SECTOR_SIZE ;
  if( pend )
    *pend = ( sect_id + 1 ) * INTERNAL_FLASH_SECTOR_SIZE - 1;
  return sect_id;
#else // #ifdef INTERNAL_FLASH_SECTOR_SIZE
  // The flash has blocks of different size
  // Their size is decribed in the INTERNAL_FLASH_SECTOR_ARRAY macro
  const uint32_t flash_sect_size[] = INTERNAL_FLASH_SECTOR_ARRAY;
  uint32_t total = 0, i = 0;

  while( ( total <= address ) && ( i < sizeof( flash_sect_size ) / sizeof( uint32_t ) ) )
    total += flash_sect_size[ i ++ ];
  if( pstart )
    *pstart = ( total - flash_sect_size[ i - 1 ] );
  if( pend )
    *pend = total - 1;
  return i - 1;
#endif // #ifdef INTERNAL_FLASH_SECTOR_SIZE
}

uint32_t platform_flash_get_sector_of_address( uint32_t addr )
{
  return flash_find_sector( addr, NULL, NULL );
}

uint32_t platform_flash_get_num_sectors(void)
{
#ifdef INTERNAL_FLASH_SECTOR_SIZE
  return INTERNAL_FLASH_SIZE / INTERNAL_FLASH_SECTOR_SIZE;
#else // #ifdef INTERNAL_FLASH_SECTOR_SIZE
  const uint32_t flash_sect_size[] = INTERNAL_FLASH_SECTOR_ARRAY;

  return sizeof( flash_sect_size ) / sizeof( uint32_t );
#endif // #ifdef INTERNAL_FLASH_SECTOR_SIZE
}

uint32_t platform_flash_get_first_free_block_address( uint32_t *psect )
{
  // Round the total used flash size to the closest flash block address
  uint32_t start, end, sect;
  NODE_DBG("_flash_used_end:%08x\n", (uint32_t)_flash_used_end);
  if(_flash_used_end>0){ // find the used sector
    sect = flash_find_sector( platform_flash_mapped2phys ( (uint32_t)_flash_used_end - 1), NULL, &end );
    if( psect )
      *psect = sect + 1;
    return end + 1;
  } else {
    sect = flash_find_sector( 0, &start, NULL ); // find the first free sector
    if( psect )
      *psect = sect;
    return start;
  }
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
    // c_memcpy( tmpdata, ( const void* )temp, blksize );
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
    // c_memcpy( tmpdata, ( const void* )toaddr, blksize );
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
