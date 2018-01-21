
#ifndef __VFS_H__
#define __VFS_H__

#include "vfs_int.h"

// DEPRECATED, DON'T USE
// Check for fd != 0 instead
#define FS_OPEN_OK 1

// ---------------------------------------------------------------------------
// file functions
//

// vfs_close - close file descriptor and free memory
//   fd: file descriptor
//   Returns: VFS_RES_OK or negative value in case of error
static sint32_t vfs_close( int fd ) {
  vfs_file *f = (vfs_file *)fd;
  return f ? f->fns->close( f ) : VFS_RES_ERR;
}

// vfs_read - read data from file
//   fd: file descriptor
//   ptr: destination data buffer
//   len: requested length
//   Returns: Number of bytes read, or VFS_RES_ERR in case of error
static sint32_t vfs_read( int fd, void *ptr, size_t len ) {
  vfs_file *f = (vfs_file *)fd;
  return f ? f->fns->read( f, ptr, len ) : VFS_RES_ERR;
}

// vfs_write - write data to file
//   fd: file descriptor
//   ptr: source data buffer
//   len: requested length
//   Returns: Number of bytes written, or VFS_RES_ERR in case of error
static sint32_t vfs_write( int fd, const void *ptr, size_t len ) {
  vfs_file *f = (vfs_file *)fd;
  return f ? f->fns->write( f, ptr, len ) : VFS_RES_ERR;
}

int vfs_getc( int fd );

int vfs_ungetc( int c, int fd );

// vfs_lseek - move read/write pointer
//   fd: file descriptor
//   off: offset
//   whence: VFS_SEEK_SET - set pointer to off
//           VFS_SEEK_CUR - set pointer to current position + off
//           VFS_SEEK_END - set pointer to end of file + off
//   Returns: New position, or VFS_RES_ERR in case of error
static sint32_t vfs_lseek( int fd, sint32_t off, int whence ) {
  vfs_file *f = (vfs_file *)fd;
  return f ? f->fns->lseek( f, off, whence ) : VFS_RES_ERR;
}

// vfs_eof - test for end-of-file
//   fd: file descriptor
//   Returns: 0 if not at end, != 0 if end of file
static sint32_t vfs_eof( int fd ) {
  vfs_file *f = (vfs_file *)fd;
  return f ? f->fns->eof( f ) : VFS_RES_ERR;
}

// vfs_tell - get read/write position
//   fd: file descriptor
//   Returns: Current position
static sint32_t vfs_tell( int fd ) {
  vfs_file *f = (vfs_file *)fd;
  return f ? f->fns->tell( f ) : VFS_RES_ERR;
}

// vfs_flush - flush write cache to file
//   fd: file descriptor
//   Returns: VFS_RES_OK, or VFS_RES_ERR in case of error
static sint32_t vfs_flush( int fd ) {
  vfs_file *f = (vfs_file *)fd;
  return f ? f->fns->flush( f ) : VFS_RES_ERR;
}

// vfs_size - get current file size
//   fd: file descriptor
//   Returns: File size
static uint32_t vfs_size( int fd ) {
  vfs_file *f = (vfs_file *)fd;
  return f ? f->fns->size( f ) : 0;
}

// vfs_ferrno - get file system specific errno
//   fd: file descriptor
//   Returns: errno
sint32_t vfs_ferrno( int fd );

// ---------------------------------------------------------------------------
// dir functions
//

// vfs_closedir - close directory descriptor and free memory
//   dd: dir descriptor
//   Returns: VFS_RES_OK, or VFS_RES_ERR in case of error
static sint32_t vfs_closedir( vfs_dir *dd ) { return dd->fns->close( dd ); }

// vfs_readdir - read next directory item
//   dd: dir descriptor
//   buf:  pre-allocated stat structure to be filled in
//   Returns: VFS_RES_OK if next item found, otherwise VFS_RES_ERR
static sint32_t vfs_readdir( vfs_dir *dd, struct vfs_stat *buf ) { return dd->fns->readdir( dd, buf ); }

// ---------------------------------------------------------------------------
// volume functions
//

// vfs_umount - unmount logical drive and free memory
//   vol: volume object
//   Returns: VFS_RES_OK, or VFS_RES_ERR in case of error
static sint32_t vfs_umount( vfs_vol *vol ) { return vol->fns->umount( vol ); }

// ---------------------------------------------------------------------------
// file system functions
//

// vfs_mount - unmount logical drive
//   name: name of logical drive
//   num: drive's physical number (eg. SS/CS pin), negative values are ignored
//   Returns: Volume object, or NULL in case of error
vfs_vol  *vfs_mount( const char *name, int num );

// vfs_open - open file
//   name: file name
//   mode: open mode
//   Returns: File descriptor, or NULL in case of error
int vfs_open( const char *name, const char *mode );

// vfs_opendir - open directory
//   name: dir name
//   Returns: Directory descriptor, or NULL in case of error
vfs_dir  *vfs_opendir( const char *name );

// vfs_stat - stat file or directory
//   name: file or directory name
//   buf:  pre-allocated structure to be filled in
//   Returns: VFS_RES_OK, or VFS_RES_ERR in case of error
sint32_t  vfs_stat( const char *name, struct vfs_stat *buf );

// vfs_remove - remove file or directory
//   name: file or directory name
//   Returns: VFS_RES_OK, or VFS_RES_ERR in case of error
sint32_t  vfs_remove( const char *name );

// vfs_rename - rename file or directory
//   name: file or directory name
//   Returns: VFS_RES_OK, or VFS_RES_ERR in case of error
sint32_t  vfs_rename( const char *oldname, const char *newname );

// vfs_mkdir - create directory
//   name: directory name
//   Returns: VFS_RES_OK, or VFS_RES_ERR in case of error
sint32_t  vfs_mkdir( const char *name );

// vfs_fsinfo - get file system info
//   name: logical drive identifier
//   total: receives total amount
//   used: received used amount
//   Returns: VFS_RES_OK, or VFS_RES_ERR in case of error
sint32_t  vfs_fsinfo( const char *name, uint32_t *total, uint32_t *used );

// vfs_format - format file system
//   Returns: 1, or 0 in case of error
sint32_t  vfs_format( void );

// vfs_chdir - change default directory
//   path: new default directory
//   Returns: VFS_RES_OK, or VFS_RES_ERR in case of error
sint32_t  vfs_chdir( const char *path );

// vfs_fscfg - query configuration settings of file system
//   phys_addr: pointer to store physical address information
//   phys_size: pointer to store physical size information
//   Returns: VFS_RES_OK, or VFS_RES_ERR in case of error
sint32_t vfs_fscfg( const char *name, uint32_t *phys_addr, uint32_t *phys_size);

// vfs_errno - get file system specific errno
//   name: logical drive identifier
//   Returns: errno
sint32_t  vfs_errno( const char *name );

// vfs_clearerr - cleaer file system specific errno
void      vfs_clearerr( const char *name );

// vfs_register_rtc_cb - register callback function for RTC query
//   cb: pointer to callback function
void vfs_register_rtc_cb( sint32_t (*cb)( vfs_time *tm ) );

// vfs_basename - identify basename (incl. extension)
//   path: full file system path
//   Returns: pointer to basename within path string
const char *vfs_basename( const char *path );

#endif
