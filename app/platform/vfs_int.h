// internal definitions for vfs

#ifndef __VFS_INT_H__
#define __VFS_INT_H__

#include <c_string.h>
#include <c_stdint.h>

#if 0
#include "spiffs.h"

#include "fatfs_prefix_lib.h"
#include "ff.h"
#endif


#define VFS_EOF -1

enum vfs_filesystems {
  VFS_FS_NONE = 0,
  VFS_FS_SPIFFS,
  VFS_FS_FATFS
};

enum vfs_seek {
  VFS_SEEK_SET = 0,
  VFS_SEEK_CUR,
  VFS_SEEK_END
};

enum vfs_result {
  VFS_RES_OK  = 0,
  VFS_RES_ERR = -1
};


struct vfs_time {
  int year, mon, day;
  int hour, min, sec;
};
typedef struct vfs_time vfs_time;

// generic file descriptor
struct vfs_file {
  int fs_type;
  const struct vfs_file_fns *fns;
};
typedef const struct vfs_file vfs_file;

// stat data
struct vfs_stat {
  uint32_t size;
  char name[FS_OBJ_NAME_LEN+1];
  struct vfs_time tm;
  uint8_t tm_valid;
  uint8_t is_dir;
  uint8_t is_rdonly;
  uint8_t is_hidden;
  uint8_t is_sys;
  uint8_t is_arch;
};

// file descriptor functions
struct vfs_file_fns {
  sint32_t (*close)( const struct vfs_file *fd );
  sint32_t (*read)( const struct vfs_file *fd, void *ptr, size_t len );
  sint32_t (*write)( const struct vfs_file *fd, const void *ptr, size_t len );
  sint32_t (*lseek)( const struct vfs_file *fd, sint32_t off, int whence );
  sint32_t (*eof)( const struct vfs_file *fd );
  sint32_t (*tell)( const struct vfs_file *fd );
  sint32_t (*flush)( const struct vfs_file *fd );
  uint32_t (*size)( const struct vfs_file *fd );
  sint32_t (*ferrno)( const struct vfs_file *fd );
};
typedef const struct vfs_file_fns vfs_file_fns;

// generic dir descriptor
struct vfs_dir {
  int fs_type;
  const struct vfs_dir_fns *fns;
};
typedef const struct vfs_dir vfs_dir;

// dir descriptor functions
struct vfs_dir_fns {
  sint32_t (*close)( const struct vfs_dir *dd );
  sint32_t (*readdir)( const struct vfs_dir *dd, struct vfs_stat *buf );
};
typedef const struct vfs_dir_fns vfs_dir_fns;

// generic volume descriptor
struct vfs_vol {
  int fs_type;
  const struct vfs_vol_fns *fns;
};
typedef const struct vfs_vol vfs_vol;

// volume functions
struct vfs_vol_fns {
  sint32_t (*umount)( const struct vfs_vol *vol );
};
typedef const struct vfs_vol_fns vfs_vol_fns;

struct vfs_fs_fns {
  vfs_vol  *(*mount)( const char *name, int num );
  vfs_file *(*open)( const char *name, const char *mode );
  vfs_dir  *(*opendir)( const char *name );
  sint32_t  (*stat)( const char *name, struct vfs_stat *buf );
  sint32_t  (*remove)( const char *name );
  sint32_t  (*rename)( const char *oldname, const char *newname );
  sint32_t  (*mkdir)( const char *name );
  sint32_t  (*fsinfo)( uint32_t *total, uint32_t *used );
  sint32_t  (*fscfg)( uint32_t *phys_addr, uint32_t *phys_size );
  sint32_t  (*format)( void );
  sint32_t  (*chdrive)( const char * );
  sint32_t  (*chdir)( const char * );
  sint32_t  (*ferrno)( void );
  void      (*clearerr)( void );
};
typedef const struct vfs_fs_fns vfs_fs_fns;


vfs_fs_fns *myspiffs_realm( const char *inname, char **outname, int set_current_drive );
vfs_fs_fns *myfatfs_realm( const char *inname, char **outname, int set_current_drive );

sint32_t vfs_get_rtc( vfs_time *tm );

#endif
