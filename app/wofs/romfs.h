// Read-only ROM filesystem

#ifndef __ROMFS_H__
#define __ROMFS_H__

#include "c_types.h"
#include "c_fcntl.h"

/*******************************************************************************
The Read-Only "filesystem" resides in a contiguous zone of memory, with the
following structure (repeated for each file):

Filename: ASCIIZ, max length is DM_MAX_FNAME_LENGTH, first byte is 0xFF if last file
File size: (4 bytes), aligned to ROMFS_ALIGN bytes 
File data: (file size bytes)

The WOFS (Write Once File System) uses much of the ROMFS functions, thuss it is
also implemented in romfs.c. It resides in a contiguous zone of memory, with a
structure that is quite similar with ROMFS' structure (repeated for each file):

Filename: ASCIIZ, max length is DM_MAX_FNAME_LENGTH, first byte is 0xFF if last file.
          WOFS filenames always begin at an address which is a multiple of ROMFS_ALIGN.
File deleted flag: (WOFS_DEL_FIELD_SIZE bytes), aligned to ROMFS_ALIGN bytes
File size: (4 bytes), aligned to ROMFS_ALIGN bytes
File data: (file size bytes)

*******************************************************************************/

// GLOBAL maximum file length (on ALL supported filesystem)
#define MAX_FNAME_LENGTH   30

enum
{
  FS_FILE_NOT_FOUND,
  FS_FILE_OK
};
 
// ROMFS/WOFS functions
typedef uint32_t ( *p_fs_read )( void *to, uint32_t fromaddr, uint32_t size, const void *pdata );
typedef uint32_t ( *p_fs_write )( const void *from, uint32_t toaddr, uint32_t size, const void *pdata );

// File flags
#define ROMFS_FILE_FLAG_READ      0x01
#define ROMFS_FILE_FLAG_WRITE     0x02
#define ROMFS_FILE_FLAG_APPEND    0x04

// A small "FILE" structure
typedef struct 
{
  uint32_t baseaddr;
  uint32_t offset;
  uint32_t size;
  uint8_t flags;
} FD;

// WOFS constants
// The miminum size we need in order to create another file
// This size will be added to the size of the filename when creating a new file
// to ensure that there's enough space left on the device
// This comes from the size of the file length field (4) + the maximum number of
// bytes needed to align this field (3) + a single 0xFF byte which marks the end
// of the filesystem (1) + the maximum number of bytes needed to align the contents 
// of a file (3)
#define WOFS_MIN_NEEDED_SIZE      11   

// Filesystem flags
#define ROMFS_FS_FLAG_DIRECT      0x01    // direct mode (the file is mapped in a memory area directly accesible by the CPU)
#define ROMFS_FS_FLAG_WO          0x02    // this FS is actually a WO (Write-Once) FS
#define ROMFS_FS_FLAG_WRITING     0x04    // for WO only: there is already a file opened in write mode

// File system descriptor
typedef struct
{
  uint8_t *pbase;                      // pointer to FS base in memory (only for ROMFS_FS_FLAG_DIRECT)
  uint8_t flags;                       // flags (see above)
  p_fs_read readf;                // pointer to read function (for non-direct mode FS)
  p_fs_write writef;              // pointer to write function (only for ROMFS_FS_FLAG_WO)
  uint32_t max_size;                   // maximum size of the FS (in bytes)
} FSDATA;

#define romfs_fs_set_flag( p, f )     p->flags |= ( f )
#define romfs_fs_clear_flag( p, f )   p->flags &= ( uint8_t )~( f )
#define romfs_fs_is_flag_set( p, f )  ( ( p->flags & ( f ) ) != 0 )

#if defined( BUILD_WOFS )
int wofs_format( void );
int wofs_open(const char *name, int flags);
int wofs_close( int fd );
size_t wofs_write( int fd, const void* ptr, size_t len );
size_t wofs_read( int fd, void* ptr, size_t len);
int wofs_lseek( int fd, int off, int whence );
int wofs_eof( int fd );
int wofs_getc( int fd );
int wofs_ungetc( int c, int fd );
uint8_t wofs_next( uint32_t *start, char* fname, size_t len, size_t *act_len );   // for list file name
#endif
// FS functions
int romfs_init( void );

#endif

