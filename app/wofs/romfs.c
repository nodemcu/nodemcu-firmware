// Filesystem implementation
#include "romfs.h"
#include "c_string.h"
// #include "c_errno.h"
#include "romfiles.h"
#include "c_stdio.h"
// #include "c_stdlib.h"
#include "c_fcntl.h"

#include "platform.h"

#if defined( BUILD_ROMFS ) || defined( BUILD_WOFS )

#define TOTAL_MAX_FDS   8
// DO NOT CHANGE THE ROMFS ALIGNMENT.
// UNLESS YOU _LIKE_ TO WATCH THE WORLD BURN.
#define ROMFS_ALIGN     4

#define fsmin( x , y ) ( ( x ) < ( y ) ? ( x ) : ( y ) )

static FD fd_table[ TOTAL_MAX_FDS ];
static int romfs_num_fd;

#define WOFS_END_MARKER_CHAR  0xFF
#define WOFS_DEL_FIELD_SIZE   ( ROMFS_ALIGN )
#define WOFS_FILE_DELETED     0xAA

// Length of the 'file size' field for both ROMFS/WOFS
#define ROMFS_SIZE_LEN        4

static int romfs_find_empty_fd(void)
{
  int i;
  
  for( i = 0; i < TOTAL_MAX_FDS; i ++ )
    if( fd_table[ i ].baseaddr == 0xFFFFFFFF &&
        fd_table[ i ].offset == 0xFFFFFFFF &&
        fd_table[ i ].size == 0xFFFFFFFF )
      return i;
  return -1;
}

static void romfs_close_fd( int fd )
{
  if(fd<0 || fd>=TOTAL_MAX_FDS) 
    return;
  c_memset( fd_table + fd, 0xFF, sizeof( FD ) );
  fd_table[ fd ].flags = 0;
}

// Helper function: read a byte from the FS
static uint8_t romfsh_read8( uint32_t addr, const FSDATA *pfs )
{
  uint8_t temp;
  if( pfs->flags & ROMFS_FS_FLAG_DIRECT )
    return pfs->pbase[ addr ];
  pfs->readf( &temp, addr, 1, pfs );
  return temp;
}

// Helper function: return 1 if PFS reffers to a WOFS, 0 otherwise
static int romfsh_is_wofs( const FSDATA* pfs )
{
  return ( pfs->flags & ROMFS_FS_FLAG_WO ) != 0;
}

// Find the next file, returning FS_FILE_OK or FS_FILE_NOT_FOUND if there no file left.
static uint8_t romfs_next_file( uint32_t *start, char* fname, size_t len, size_t *act_len, FSDATA *pfs )
{
  uint32_t i, j, n;
  uint32_t fsize;
  int is_deleted;
  
  // Look for the file
  i = *start;
  *act_len = 0;
  if( (i >= INTERNAL_FLASH_SIZE) || (romfsh_read8( i, pfs ) == WOFS_END_MARKER_CHAR ))    // end of file system
  {
    *start = (i >= INTERNAL_FLASH_SIZE)?(INTERNAL_FLASH_SIZE-1):i;
    return FS_FILE_NOT_FOUND;
  }
  // Read file name
  len = len>MAX_FNAME_LENGTH?MAX_FNAME_LENGTH:len;
  for( j = 0; j < len; j ++ )
  {
    fname[ j ] = romfsh_read8( i + j, pfs );
    if( fname[ j ] == 0 )
       break;
  }
  n = j; // save the file name length to n
  // ' i + j' now points at the '0' byte
  j = i + j + 1;
  // Round to a multiple of ROMFS_ALIGN
  j = ( j + ROMFS_ALIGN - 1 ) & ~( ROMFS_ALIGN - 1 );
  // WOFS has an additional WOFS_DEL_FIELD_SIZE bytes before the size as an indication for "file deleted"
  if( romfsh_is_wofs( pfs ) )
  {
    is_deleted = romfsh_read8( j, pfs ) == WOFS_FILE_DELETED;
    j += WOFS_DEL_FIELD_SIZE;
  }
  else
    is_deleted = 0;
  // And read the size
  fsize = romfsh_read8( j, pfs ) + ( romfsh_read8( j + 1, pfs ) << 8 );
  fsize += ( romfsh_read8( j + 2, pfs ) << 16 ) + ( romfsh_read8( j + 3, pfs ) << 24 );
  j += ROMFS_SIZE_LEN;
  if( !is_deleted )
  {
    // Found the valid file
    *act_len = n;
  }
  // Move to next file
  i = j + fsize;
  // On WOFS, all file names must begin at a multiple of ROMFS_ALIGN
  if( romfsh_is_wofs( pfs ) )
    i = ( i + ROMFS_ALIGN - 1 ) & ~( ROMFS_ALIGN - 1 );
  *start = i;   // modify the start address
  return FS_FILE_OK;
}

// Open the given file, returning one of FS_FILE_NOT_FOUND, FS_FILE_ALREADY_OPENED
// or FS_FILE_OK
static uint8_t romfs_open_file( const char* fname, FD* pfd, FSDATA *pfs, uint32_t *plast, uint32_t *pnameaddr )
{
  uint32_t i, j, n;
  char fsname[ MAX_FNAME_LENGTH + 1 ];
  uint32_t fsize;
  int is_deleted;
  
  // Look for the file
  i = 0;
  while( 1 )
  {
    if( i >= INTERNAL_FLASH_SIZE ){
      *plast = INTERNAL_FLASH_SIZE - 1;   // point to last one
      return FS_FILE_NOT_FOUND;
    }
    if( romfsh_read8( i, pfs ) == WOFS_END_MARKER_CHAR )
    {
      *plast = i;
      return FS_FILE_NOT_FOUND;
    }
    // Read file name
    n = i;
    for( j = 0; j < MAX_FNAME_LENGTH; j ++ )
    {
      fsname[ j ] = romfsh_read8( i + j, pfs );
      if( fsname[ j ] == 0 )
         break;
    }
    // ' i + j' now points at the '0' byte
    j = i + j + 1;
    // Round to a multiple of ROMFS_ALIGN
    j = ( j + ROMFS_ALIGN - 1 ) & ~( ROMFS_ALIGN - 1 );
    // WOFS has an additional WOFS_DEL_FIELD_SIZE bytes before the size as an indication for "file deleted"
    if( romfsh_is_wofs( pfs ) )
    {
      is_deleted = romfsh_read8( j, pfs ) == WOFS_FILE_DELETED;
      j += WOFS_DEL_FIELD_SIZE;
    }
    else
      is_deleted = 0;
    // And read the size
    fsize = romfsh_read8( j, pfs ) + ( romfsh_read8( j + 1, pfs ) << 8 );
    fsize += ( romfsh_read8( j + 2, pfs ) << 16 ) + ( romfsh_read8( j + 3, pfs ) << 24 );
    j += ROMFS_SIZE_LEN;
    if( !c_strncasecmp( fname, fsname, MAX_FNAME_LENGTH ) && !is_deleted )
    {
      // Found the file
      pfd->baseaddr = j;
      pfd->offset = 0;
      pfd->size = fsize;
      if( pnameaddr )
        *pnameaddr = n;
      return FS_FILE_OK;
    }
    // Move to next file
    i = j + fsize;
    // On WOFS, all file names must begin at a multiple of ROMFS_ALIGN
    if( romfsh_is_wofs( pfs ) )
      i = ( i + ROMFS_ALIGN - 1 ) & ~( ROMFS_ALIGN - 1 );
  }
  *plast = 0;
  return FS_FILE_NOT_FOUND;
}

static int romfs_open( const char *path, int flags, int mode, void *pdata )
{
  FD tempfs;
  int i;
  FSDATA *pfsdata = ( FSDATA* )pdata;
  int must_create = 0;
  int exists;
  uint8_t lflags = ROMFS_FILE_FLAG_READ;
  uint32_t firstfree, nameaddr;

  if( romfs_num_fd == TOTAL_MAX_FDS )
  {
    return -1;
  }
  // Does the file exist?
  exists = romfs_open_file( path, &tempfs, pfsdata, &firstfree, &nameaddr ) == FS_FILE_OK;
  // Now interpret "flags" to set file flags and to check if we should create the file
  if( flags & O_CREAT )
  {
    // If O_CREAT is specified with O_EXCL and the file already exists, return with error
    if( ( flags & O_EXCL ) && exists )
    {
      return -1;
    }
    // Otherwise create the file if it does not exist
    must_create = !exists;
  }
  if( ( flags & O_TRUNC ) && ( flags & ( O_WRONLY | O_RDWR ) ) && exists )
  {
    // The file exists, but it must be truncated
    // In the case of WOFS, this effectively means "create a new file"
    must_create = 1;
  }
  // ROMFS can't create files
  if( must_create && ( ( pfsdata->flags & ROMFS_FS_FLAG_WO ) == 0 ) )
  {
    return -1;
  }
  // Decode access mode
  if( flags & O_WRONLY )
    lflags = ROMFS_FILE_FLAG_WRITE;
  else if( flags & O_RDWR )
    lflags = ROMFS_FILE_FLAG_READ | ROMFS_FILE_FLAG_WRITE;
  if( flags & O_APPEND )
    lflags |= ROMFS_FILE_FLAG_APPEND;
  // If a write access is requested when the file must NOT be created, this
  // is an error
  if( ( lflags & ( ROMFS_FILE_FLAG_WRITE | ROMFS_FILE_FLAG_APPEND ) ) && !must_create )
  {
    return -1;
  }
  if( ( lflags & ( ROMFS_FILE_FLAG_WRITE | ROMFS_FILE_FLAG_APPEND ) ) && romfs_fs_is_flag_set( pfsdata, ROMFS_FS_FLAG_WRITING ) )
  {
    // At most one file can be opened in write mode at any given time on WOFS
    return -1;
  }
  // Do we need to create the file ?
  if( must_create )
  {
    if( exists )
    {
      // Invalidate the file first by changing WOFS_DEL_FIELD_SIZE bytes before
      // the file length to WOFS_FILE_DELETED
      uint8_t tempb[] = { WOFS_FILE_DELETED, 0xFF, 0xFF, 0xFF };
      pfsdata->writef( tempb, tempfs.baseaddr - ROMFS_SIZE_LEN - WOFS_DEL_FIELD_SIZE, WOFS_DEL_FIELD_SIZE, pfsdata );
    }
    // Find the last available position by asking romfs_open_file to look for a file
    // with an invalid name
    romfs_open_file( "\1", &tempfs, pfsdata, &firstfree, NULL );
    // Is there enough space on the FS for another file?
    if( pfsdata->max_size - firstfree + 1 < c_strlen( path ) + 1 + WOFS_MIN_NEEDED_SIZE + WOFS_DEL_FIELD_SIZE )
    {
      return -1;
    }

    // Make sure we can get a file descriptor before writing
    if( ( i = romfs_find_empty_fd() ) < 0 )
    {
      return -1;
    }

    // Write the name of the file
    pfsdata->writef( path, firstfree, c_strlen( path ) + 1, pfsdata );
    firstfree += c_strlen( path ) + 1; // skip over the name
    // Align to a multiple of ROMFS_ALIGN
    firstfree = ( firstfree + ROMFS_ALIGN - 1 ) & ~( ROMFS_ALIGN - 1 );
    firstfree += ROMFS_SIZE_LEN + WOFS_DEL_FIELD_SIZE; // skip over the size and the deleted flags area
    tempfs.baseaddr = firstfree;
    tempfs.offset = tempfs.size = 0;
    // Set the "writing" flag on the FS to indicate that there is a file opened in write mode
    romfs_fs_set_flag( pfsdata, ROMFS_FS_FLAG_WRITING );
  }
  else // File must exist (and was found in the previous 'romfs_open_file' call)
  {
    if( !exists )
    {
      return -1;
    }

    if( ( i = romfs_find_empty_fd() ) < 0 )
    {
      return -1;
    }
  }
  // Copy the descriptor information
  tempfs.flags = lflags;
  c_memcpy( fd_table + i, &tempfs, sizeof( FD ) );
  romfs_num_fd ++;
  return i;
}

static int romfs_close( int fd, void *pdata )
{
  if(fd<0 || fd>=TOTAL_MAX_FDS) 
    return 0;
  FD* pfd = fd_table + fd;
  FSDATA *pfsdata = ( FSDATA* )pdata;
  uint8_t temp[ ROMFS_SIZE_LEN ];

  if( pfd->flags & ( ROMFS_FILE_FLAG_WRITE | ROMFS_FILE_FLAG_APPEND ) )
  {
    // Write back the size
    temp[ 0 ] = pfd->size & 0xFF;
    temp[ 1 ] = ( pfd->size >> 8 ) & 0xFF;
    temp[ 2 ] = ( pfd->size >> 16 ) & 0xFF;
    temp[ 3 ] = ( pfd->size >> 24 ) & 0xFF;
    pfsdata->writef( temp, pfd->baseaddr - ROMFS_SIZE_LEN, ROMFS_SIZE_LEN, pfsdata );
    // Clear the "writing" flag on the FS instance to allow other files to be opened
    // in write mode
    romfs_fs_clear_flag( pfsdata, ROMFS_FS_FLAG_WRITING );
  }
  romfs_close_fd( fd );
  romfs_num_fd --;
  return 0;
}

static _ssize_t romfs_write( int fd, const void* ptr, size_t len, void *pdata )
{
  if(fd<0 || fd>=TOTAL_MAX_FDS) 
    return -1;
  if(len == 0)
    return 0;
  FD* pfd = fd_table + fd;
  FSDATA *pfsdata = ( FSDATA* )pdata;

  if( ( pfd->flags & ( ROMFS_FILE_FLAG_WRITE | ROMFS_FILE_FLAG_APPEND ) ) == 0 )
  {
    return -1;
  }
  // Append mode: set the file pointer to the end
  if( pfd->flags & ROMFS_FILE_FLAG_APPEND )
    pfd->offset = pfd->size;
  // Only write at the end of the file!
  if( pfd->offset != pfd->size )
    return 0;
  // Check if we have enough space left on the device. Always keep 1 byte for the final 0xFF
  // and ROMFS_ALIGN - 1 bytes for aligning the contents of the file data in the worst case
  // scenario (so ROMFS_ALIGN bytes in total)
  if( pfd->baseaddr + pfd->size + len > pfsdata->max_size - ROMFS_ALIGN )
    len = pfsdata->max_size - ( pfd->baseaddr + pfd->size ) - ROMFS_ALIGN;
  pfsdata->writef( ptr, pfd->offset + pfd->baseaddr, len, pfsdata );
  pfd->offset += len;
  pfd->size += len;
  return len;
}

static _ssize_t romfs_read( int fd, void* ptr, size_t len, void *pdata )
{
  if(fd<0 || fd>=TOTAL_MAX_FDS) 
    return -1;
  if(len == 0)
    return 0;

  FD* pfd = fd_table + fd;
  long actlen = fsmin( len, pfd->size - pfd->offset );
  FSDATA *pfsdata = ( FSDATA* )pdata;

  if( ( pfd->flags & ROMFS_FILE_FLAG_READ ) == 0 )
  {
    return -1;
  }
  if( pfsdata->flags & ROMFS_FS_FLAG_DIRECT )
    c_memcpy( ptr, pfsdata->pbase + pfd->offset + pfd->baseaddr, actlen );
  else
    actlen = pfsdata->readf( ptr, pfd->offset + pfd->baseaddr, actlen, pfsdata );
  pfd->offset += actlen;
  return actlen;
}

// lseek
static int romfs_lseek( int fd, int off, int whence, void *pdata )
{
  if(fd<0 || fd>=TOTAL_MAX_FDS) 
    return -1;

  FD* pfd = fd_table + fd;
  uint32_t newpos = 0;
  
  switch( whence )
  {
    case SEEK_SET:
      newpos = off;
      break;
      
    case SEEK_CUR:
      newpos = pfd->offset + off;
      break;
      
    case SEEK_END:
      newpos = pfd->size + off;
      break;
      
    default:
      return -1;
  }    
  if( newpos > pfd->size )
    return -1;
  pfd->offset = newpos;
  return newpos;
}

// ****************************************************************************
// WOFS functions and instance descriptor for real hardware

#if defined( BUILD_WOFS )
static uint32_t sim_wofs_write( const void *from, uint32_t toaddr, uint32_t size, const void *pdata )
{
  const FSDATA *pfsdata = ( const FSDATA* )pdata;
  if(toaddr>=INTERNAL_FLASH_SIZE)
  {
    NODE_ERR("ERROR in flash op: wrong addr.\n");
    return 0;
  }
  toaddr += ( uint32_t )pfsdata->pbase;
  return platform_flash_write( from, toaddr, size );
}

static uint32_t sim_wofs_read( void *to, uint32_t fromaddr, uint32_t size, const void *pdata )
{
  const FSDATA *pfsdata = ( const FSDATA* )pdata;
  if(fromaddr>=INTERNAL_FLASH_SIZE)
  {
    NODE_ERR("ERROR in flash op: wrong addr.\n");
    return 0;
  }
  fromaddr += ( uint32_t )pfsdata->pbase;
  return platform_flash_read( to, fromaddr, size );
}

// This must NOT be a const!
static FSDATA wofs_fsdata =
{
  NULL,
  ROMFS_FS_FLAG_WO,
  sim_wofs_read,
  sim_wofs_write,
  0
};

// WOFS formatting function
// Returns 1 if OK, 0 for error
int wofs_format( void )
{
  uint32_t sect_first, sect_last;
  FD tempfd;
  platform_flash_get_first_free_block_address( &sect_first );
  // Get the first free address in WOFS. We use this address to compute the last block that we need to
  // erase, instead of simply erasing everything from sect_first to the last Flash page. 
  romfs_open_file( "\1", &tempfd, &wofs_fsdata, &sect_last, NULL );
  sect_last = platform_flash_get_sector_of_address( sect_last + ( uint32_t )wofs_fsdata.pbase );
  while( sect_first <= sect_last )
    if( platform_flash_erase_sector( sect_first ++ ) == PLATFORM_ERR )
      return 0;
  return 1;
}

int wofs_open(const char *_name, int flags){
  return romfs_open( _name, flags, 0, &wofs_fsdata );
}

int wofs_close( int fd ){
  return romfs_close( fd, &wofs_fsdata );
}

size_t wofs_write( int fd, const void* ptr, size_t len ){
  return romfs_write( fd, ptr, len, &wofs_fsdata );
}

size_t wofs_read( int fd, void* ptr, size_t len){
  return romfs_read( fd, ptr, len, &wofs_fsdata );
}

int wofs_lseek( int fd, int off, int whence ){
  return romfs_lseek( fd, off, whence, &wofs_fsdata );
}

int wofs_eof( int fd ){
  if(fd<0 || fd>=TOTAL_MAX_FDS) 
    return -1;
  FD* pfd = fd_table + fd;
  // NODE_DBG("off:%d, sz:%d\n",pfd->offset, pfd->size);
  return pfd->offset == pfd->size;
}

int wofs_getc( int fd ){
  char c = EOF;
  if(!wofs_eof(fd)){
    romfs_read( fd, &c, 1, &wofs_fsdata );
  }
  // NODE_DBG("c: %d\n", c);
  return (int)c;
}

int wofs_ungetc( int c, int fd ){
  return romfs_lseek( fd, -1, SEEK_CUR, &wofs_fsdata );
}

// Find the next file, returning FS_FILE_OK or FS_FILE_NOT_FOUND if there no file left.
uint8_t wofs_next( uint32_t *start, char* fname, size_t len, size_t *act_len ){
  return romfs_next_file( start, fname, len, act_len, &wofs_fsdata );
}

#endif // #ifdef BUILD_WOFS

// Initialize both ROMFS and WOFS as needed
int romfs_init( void )
{
  unsigned i;

  for( i = 0; i < TOTAL_MAX_FDS; i ++ )
  {
    c_memset( fd_table + i, 0xFF, sizeof( FD ) );
    fd_table[ i ].flags = 0;
  }
#if defined( BUILD_WOFS ) 
  // Get the start address and size of WOFS and register it
  wofs_fsdata.pbase = ( uint8_t* )platform_flash_get_first_free_block_address( NULL );
  wofs_fsdata.max_size = INTERNAL_FLASH_SIZE - ( ( uint32_t )wofs_fsdata.pbase - INTERNAL_FLASH_START_ADDRESS );
  NODE_DBG("wofs.pbase:%x,max:%x\n",wofs_fsdata.pbase,wofs_fsdata.max_size);
#endif // ifdef BUILD_WOFS
  return 0;
}

#else // #if defined( BUILD_ROMFS ) || defined( BUILD_WOFS )

int romfs_init( void )
{
}

#endif // #if defined( BUILD_ROMFS ) || defined( BUILD_WOFS )

int test_romfs()
{
  int fd;
  int i, size;
  
  fd = wofs_open("init.lua",O_RDONLY);
  NODE_DBG("open file fd:%d\n", fd);
  char r[128];
  NODE_DBG("read from file:\n");
  c_memset(r,0,128);
  size = wofs_read(fd,r,128);
  r[size]=0;
  NODE_DBG(r);
  NODE_DBG("\n");
  wofs_close(fd);

  fd = wofs_open("testm.lua",O_RDONLY);
  NODE_DBG("open file fd:%d\n", fd);
  NODE_DBG("read from file:\n");
  c_memset(r,0,128);
  size = wofs_read(fd,r,128);
  r[size]=0;
  NODE_DBG(r);
  NODE_DBG("\n");
  wofs_close(fd);

  return 0;
}
