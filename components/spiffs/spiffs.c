#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "platform.h"
#include "flash_api.h"
#include "spiffs.h"

#include "spiffs_nucleus.h"

static spiffs fs;

#define LOG_PAGE_SIZE       	256
#define LOG_BLOCK_SIZE		(INTERNAL_FLASH_SECTOR_SIZE * 2)
#define LOG_BLOCK_SIZE_SMALL_FS	(INTERNAL_FLASH_SECTOR_SIZE)
#define MIN_BLOCKS_FS		4
  
static u8_t spiffs_work_buf[LOG_PAGE_SIZE*2];
static u8_t spiffs_fds[sizeof(spiffs_fd) * CONFIG_NODEMCU_SPIFFS_MAX_OPEN_FILES];
#if SPIFFS_CACHE
static u8_t myspiffs_cache[(LOG_PAGE_SIZE+32)*2];
#endif

static s32_t my_spiffs_read(u32_t addr, u32_t size, u8_t *dst) {
  platform_flash_read(dst, addr, size);
  return SPIFFS_OK;
}

static s32_t my_spiffs_write(u32_t addr, u32_t size, u8_t *src) {
  platform_flash_write(src, addr, size);
  return SPIFFS_OK;
}

static s32_t my_spiffs_erase(u32_t addr, u32_t size) {
  u32_t sect_first = platform_flash_get_sector_of_address(addr);
  u32_t sect_last = sect_first;
  while( sect_first <= sect_last )
    if( platform_flash_erase_sector( sect_first ++ ) == PLATFORM_ERR )
      return SPIFFS_ERR_INTERNAL;
  return SPIFFS_OK;
} 

void myspiffs_check_callback(spiffs_check_type type, spiffs_check_report report, u32_t arg1, u32_t arg2){
  // if(SPIFFS_CHECK_PROGRESS == report) return;
  // NODE_ERR("type: %d, report: %d, arg1: %d, arg2: %d\n", type, report, arg1, arg2);
}

/*******************
The W25Q32BV array is organized into 16,384 programmable pages of 256-bytes each. Up to 256 bytes can be programmed at a time. 
Pages can be erased in groups of 16 (4KB sector erase), groups of 128 (32KB block erase), groups of 256 (64KB block erase) or 
the entire chip (chip erase). The W25Q32BV has 1,024 erasable sectors and 64 erasable blocks respectively. 
The small 4KB sectors allow for greater flexibility in applications that require data and parameter storage. 

********************/


static bool get_spiffs_partition (spiffs_config *cfg)
{
  platform_partition_t info;
  uint8_t i = 0;
  while (platform_partition_info (i, &info))
  {
    if (info.type == PLATFORM_PARTITION_TYPE_NODEMCU &&
        info.subtype == PLATFORM_PARTITION_SUBTYPE_NODEMCU_SPIFFS)
    {
      cfg->phys_addr = info.offs;
      cfg->phys_size = info.size;
      return true;
    }
    else
      ++i;
  }
  return false;
}


static bool myspiffs_mount_internal(bool force_mount)
{
  spiffs_config cfg;
  cfg.phys_erase_block = INTERNAL_FLASH_SECTOR_SIZE; // according to datasheet
  cfg.log_page_size = LOG_PAGE_SIZE; // as we said
  cfg.log_block_size = LOG_BLOCK_SIZE;
  cfg.hal_read_f = my_spiffs_read;
  cfg.hal_write_f = my_spiffs_write;
  cfg.hal_erase_f = my_spiffs_erase;
  if (!get_spiffs_partition (&cfg))
    return false;

  if (!force_mount && SPIFFS_probe_fs (&cfg) != cfg.phys_size)
    return false;

  fs.err_code = 0;

  int res = SPIFFS_mount(&fs,
    &cfg,
    spiffs_work_buf,
    spiffs_fds,
    sizeof(spiffs_fds),
    myspiffs_cache,
    sizeof(myspiffs_cache),
    // myspiffs_check_callback);
    0);
  NODE_DBG("mount res: %d, %d\n", res, fs.err_code);
  return res == SPIFFS_OK;
}

bool myspiffs_mount() {
  return myspiffs_mount_internal(false);
}

void myspiffs_unmount() {
  SPIFFS_unmount(&fs);
}

// FS formatting function
// Returns 1 if OK, 0 for error
int myspiffs_format( void )
{
  SPIFFS_unmount(&fs);
  myspiffs_mount_internal(true);
  SPIFFS_unmount(&fs);

  NODE_DBG("Formatting: size 0x%x, addr 0x%x\n", fs.cfg.phys_size, fs.cfg.phys_addr);

  s32_t res = SPIFFS_format (&fs);
  if (res < 0) {
    return 0;
  }

  return myspiffs_mount();
}

#if 0
void test_spiffs() {
  char buf[12];

  // Surely, I've mounted spiffs before entering here
  
  spiffs_file fd = SPIFFS_open(&fs, "my_file", SPIFFS_CREAT | SPIFFS_TRUNC | SPIFFS_RDWR, 0);
  if (SPIFFS_write(&fs, fd, (u8_t *)"Hello world", 12) < 0) NODE_DBG("errno %i\n", SPIFFS_errno(&fs));
  SPIFFS_close(&fs, fd); 

  fd = SPIFFS_open(&fs, "my_file", SPIFFS_RDWR, 0);
  if (SPIFFS_read(&fs, fd, (u8_t *)buf, 12) < 0) NODE_DBG("errno %i\n", SPIFFS_errno(&fs));
  SPIFFS_close(&fs, fd);

  NODE_DBG("--> %s <--\n", buf);
}
#endif


// ***************************************************************************
// vfs API
// ***************************************************************************

#include <stdlib.h>
#include "vfs_int.h"

#define MY_LDRV_ID "FLASH"

// default current drive
static int is_current_drive = true;

// forward declarations
static int32_t myspiffs_vfs_close( const struct vfs_file *fd );
static int32_t myspiffs_vfs_read( const struct vfs_file *fd, void *ptr, size_t len );
static int32_t myspiffs_vfs_write( const struct vfs_file *fd, const void *ptr, size_t len );
static int32_t myspiffs_vfs_lseek( const struct vfs_file *fd, int32_t off, int whence );
static int32_t myspiffs_vfs_eof( const struct vfs_file *fd );
static int32_t myspiffs_vfs_tell( const struct vfs_file *fd );
static int32_t myspiffs_vfs_flush( const struct vfs_file *fd );
static uint32_t myspiffs_vfs_size( const struct vfs_file *fd );
static int32_t myspiffs_vfs_ferrno( const struct vfs_file *fd );

static int32_t  myspiffs_vfs_closedir( const struct vfs_dir *dd );
static int32_t  myspiffs_vfs_readdir( const struct vfs_dir *dd, struct vfs_stat *buf );

static vfs_vol  *myspiffs_vfs_mount( const char *name, int num );
static vfs_file *myspiffs_vfs_open( const char *name, const char *mode );
static vfs_dir  *myspiffs_vfs_opendir( const char *name );
static int32_t myspiffs_vfs_stat( const char *name, struct vfs_stat *buf );
static int32_t  myspiffs_vfs_remove( const char *name );
static int32_t  myspiffs_vfs_rename( const char *oldname, const char *newname );
static int32_t  myspiffs_vfs_fsinfo( uint32_t *total, uint32_t *used );
static int32_t  myspiffs_vfs_fscfg( uint32_t *phys_addr, uint32_t *phys_size );
static int32_t  myspiffs_vfs_format( void );
static int32_t  myspiffs_vfs_errno( void );
static void      myspiffs_vfs_clearerr( void );

// ---------------------------------------------------------------------------
// function tables
//
static vfs_fs_fns myspiffs_fs_fns = {
  .mount    = myspiffs_vfs_mount,
  .open     = myspiffs_vfs_open,
  .opendir  = myspiffs_vfs_opendir,
  .stat     = myspiffs_vfs_stat,
  .remove   = myspiffs_vfs_remove,
  .rename   = myspiffs_vfs_rename,
  .mkdir    = NULL,
  .fsinfo   = myspiffs_vfs_fsinfo,
  .fscfg    = myspiffs_vfs_fscfg,
  .format   = myspiffs_vfs_format,
  .chdrive  = NULL,
  .chdir    = NULL,
  .ferrno   = myspiffs_vfs_errno,
  .clearerr = myspiffs_vfs_clearerr
};

static vfs_file_fns myspiffs_file_fns = {
  .close     = myspiffs_vfs_close,
  .read      = myspiffs_vfs_read,
  .write     = myspiffs_vfs_write,
  .lseek     = myspiffs_vfs_lseek,
  .eof       = myspiffs_vfs_eof,
  .tell      = myspiffs_vfs_tell,
  .flush     = myspiffs_vfs_flush,
  .size      = myspiffs_vfs_size,
  .ferrno    = myspiffs_vfs_ferrno
};

static vfs_dir_fns myspiffs_dd_fns = {
  .close     = myspiffs_vfs_closedir,
  .readdir   = myspiffs_vfs_readdir
};


// ---------------------------------------------------------------------------
// specific struct extensions
//
struct myvfs_file {
  struct vfs_file vfs_file;
  spiffs_file fh;
};

struct myvfs_dir {
  struct vfs_dir vfs_dir;
  spiffs_DIR d;
};


// ---------------------------------------------------------------------------
// dir functions
//
#define GET_DIR_D(descr) \
  const struct myvfs_dir *mydd = (const struct myvfs_dir *)descr; \
  spiffs_DIR *d = (spiffs_DIR *)&(mydd->d);

static int32_t myspiffs_vfs_closedir( const struct vfs_dir *dd ) {
  GET_DIR_D(dd);

  int32_t res = SPIFFS_closedir( d );

  // free descriptor memory
  free( (void *)dd );

  return res;
}

static int32_t myspiffs_vfs_readdir( const struct vfs_dir *dd, struct vfs_stat *buf ) {
  GET_DIR_D(dd);
  struct spiffs_dirent dirent;

  if (SPIFFS_readdir( d, &dirent )) {
    memset( buf, 0, sizeof( struct vfs_stat ) );
    // copy entries to  item
    // fill in supported stat entries
    strncpy( buf->name, (char *)dirent.name, CONFIG_NODEMCU_FS_OBJ_NAME_LEN+1 );
    buf->name[CONFIG_NODEMCU_FS_OBJ_NAME_LEN] = '\0';
    buf->size = dirent.size;
    return VFS_RES_OK;
  }

  return VFS_RES_ERR;
}


// ---------------------------------------------------------------------------
// file functions
//
#define GET_FILE_FH(descr) \
  const struct myvfs_file *myfd = (const struct myvfs_file *)descr; \
  spiffs_file fh = myfd->fh;

static int32_t myspiffs_vfs_close( const struct vfs_file *fd ) {
  GET_FILE_FH(fd);

  int32_t res = SPIFFS_close( &fs, fh );

  // free descriptor memory
  free( (void *)fd );

  return res;
}

static int32_t myspiffs_vfs_read( const struct vfs_file *fd, void *ptr, size_t len ) {
  GET_FILE_FH(fd);

  int32_t n = SPIFFS_read( &fs, fh, ptr, len );

  return n >= 0 ? n : VFS_RES_ERR;
}

static int32_t myspiffs_vfs_write( const struct vfs_file *fd, const void *ptr, size_t len ) {
  GET_FILE_FH(fd);

  int32_t n = SPIFFS_write( &fs, fh, (void *)ptr, len );

  return n >= 0 ? n : VFS_RES_ERR;
}

static int32_t myspiffs_vfs_lseek( const struct vfs_file *fd, int32_t off, int whence ) {
  GET_FILE_FH(fd);
  int spiffs_whence;

  switch (whence) {
  default:
  case VFS_SEEK_SET:
    spiffs_whence = SPIFFS_SEEK_SET;
    break;
  case VFS_SEEK_CUR:
    spiffs_whence = SPIFFS_SEEK_CUR;
    break;
  case VFS_SEEK_END:
    spiffs_whence = SPIFFS_SEEK_END;
    break;
  }

  int32_t res = SPIFFS_lseek( &fs, fh, off, spiffs_whence );
  return res >= 0 ? res : VFS_RES_ERR;
}

static int32_t myspiffs_vfs_eof( const struct vfs_file *fd ) {
  GET_FILE_FH(fd);

  return SPIFFS_eof( &fs, fh );
}

static int32_t myspiffs_vfs_tell( const struct vfs_file *fd ) {
  GET_FILE_FH(fd);

  return SPIFFS_tell( &fs, fh );
}

static int32_t myspiffs_vfs_flush( const struct vfs_file *fd ) {
  GET_FILE_FH(fd);

  return SPIFFS_fflush( &fs, fh ) >= 0 ? VFS_RES_OK : VFS_RES_ERR;
}

static uint32_t myspiffs_vfs_size( const struct vfs_file *fd ) {
  GET_FILE_FH(fd);
  int32_t curpos = SPIFFS_tell( &fs, fh );
  int32_t size = SPIFFS_lseek( &fs, fh, 0, SPIFFS_SEEK_END );
  (void) SPIFFS_lseek( &fs, fh, curpos, SPIFFS_SEEK_SET );
   return size;
}

static int32_t myspiffs_vfs_ferrno( const struct vfs_file *fd ) {
  return SPIFFS_errno( &fs );
}


static int fs_mode2flag(const char *mode){
  if(strlen(mode)==1){
  	if(strcmp(mode,"w")==0)
  	  return SPIFFS_WRONLY|SPIFFS_CREAT|SPIFFS_TRUNC;
  	else if(strcmp(mode, "r")==0)
  	  return SPIFFS_RDONLY;
  	else if(strcmp(mode, "a")==0)
  	  return SPIFFS_WRONLY|SPIFFS_CREAT|SPIFFS_APPEND;
  	else
  	  return SPIFFS_RDONLY;
  } else if (strlen(mode)==2){
  	if(strcmp(mode,"r+")==0)
  	  return SPIFFS_RDWR;
  	else if(strcmp(mode, "w+")==0)
  	  return SPIFFS_RDWR|SPIFFS_CREAT|SPIFFS_TRUNC;
  	else if(strcmp(mode, "a+")==0)
  	  return SPIFFS_RDWR|SPIFFS_CREAT|SPIFFS_APPEND;
  	else
  	  return SPIFFS_RDONLY;
  } else {
  	return SPIFFS_RDONLY;
  }
}

// ---------------------------------------------------------------------------
// filesystem functions
//
static vfs_file *myspiffs_vfs_open( const char *name, const char *mode ) {
  struct myvfs_file *fd;
  int flags = fs_mode2flag( mode );

  if ((fd = (struct myvfs_file *)malloc( sizeof( struct myvfs_file ) ))) {
    if (0 < (fd->fh = SPIFFS_open( &fs, name, flags, 0 ))) {
      fd->vfs_file.fs_type = VFS_FS_SPIFFS;
      fd->vfs_file.fns     = &myspiffs_file_fns;
      return (vfs_file *)fd;
    } else {
      free( fd );
    }
  }

  return NULL;
}

static vfs_dir *myspiffs_vfs_opendir( const char *name ){
  struct myvfs_dir *dd;

  if ((dd = (struct myvfs_dir *)malloc( sizeof( struct myvfs_dir ) ))) {
    if (SPIFFS_opendir( &fs, name, &(dd->d) )) {
      dd->vfs_dir.fs_type = VFS_FS_SPIFFS;
      dd->vfs_dir.fns     = &myspiffs_dd_fns;
      return (vfs_dir *)dd;
    } else {
      free( dd );
    }
  }

  return NULL;
}

static int32_t myspiffs_vfs_stat( const char *name, struct vfs_stat *buf ) {
  spiffs_stat stat;

  if (0 <= SPIFFS_stat( &fs, name, &stat )) {
    memset( buf, 0, sizeof( struct vfs_stat ) );

    // fill in supported stat entries
    strncpy( buf->name, (char *)stat.name, CONFIG_NODEMCU_FS_OBJ_NAME_LEN+1 );
    buf->name[CONFIG_NODEMCU_FS_OBJ_NAME_LEN] = '\0';
    buf->size = stat.size;
    return VFS_RES_OK;
  } else {
    return VFS_RES_ERR;
  }
}

static int32_t myspiffs_vfs_remove( const char *name ) {
  return SPIFFS_remove( &fs, name );
}

static int32_t myspiffs_vfs_rename( const char *oldname, const char *newname ) {
  return SPIFFS_rename( &fs, oldname, newname );
}

static int32_t myspiffs_vfs_fsinfo( uint32_t *total, uint32_t *used ) {
  return SPIFFS_info( &fs, total, used );
}

static int32_t myspiffs_vfs_fscfg( uint32_t *phys_addr, uint32_t *phys_size ) {
  *phys_addr = fs.cfg.phys_addr;
  *phys_size = fs.cfg.phys_size;
  return VFS_RES_OK;
}

static vfs_vol  *myspiffs_vfs_mount( const char *name, int num ) {
  // volume descriptor not supported, just return true / false
  return myspiffs_mount() ? (vfs_vol *)1 : NULL;
}

static int32_t myspiffs_vfs_format( void ) {
  return myspiffs_format();
}

static int32_t myspiffs_vfs_errno( void ) {
  return SPIFFS_errno( &fs );
}

static void myspiffs_vfs_clearerr( void ) {
  SPIFFS_clearerr( &fs );
}


// ---------------------------------------------------------------------------
// VFS interface functions
//
vfs_fs_fns *myspiffs_realm( const char *inname, char **outname, int set_current_drive ) {
  if (inname[0] == '/') {
    size_t idstr_len = strlen( MY_LDRV_ID );
    // logical drive is specified, check if it's our id
    if (0 == strncmp( &(inname[1]), MY_LDRV_ID, idstr_len )) {
      *outname = (char *)&(inname[1 + idstr_len]);
      if (*outname[0] == '/') {
        // skip leading /
        (*outname)++;
      }

      if (set_current_drive) is_current_drive = true;
      return &myspiffs_fs_fns;
    }
  } else {
    // no logical drive in patchspec, are we current drive?
    if (is_current_drive) {
      *outname = (char *)inname;
      return &myspiffs_fs_fns;
    }
  }

  if (set_current_drive) is_current_drive = false;
  return NULL;
}
