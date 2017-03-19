

#include "c_stdlib.h"
#include "c_stdio.h"
#include "vfs.h"


#define LDRV_TRAVERSAL 0


// ---------------------------------------------------------------------------
// RTC system interface
//
static sint32_t (*rtc_cb)( vfs_time *tm ) = NULL;

// called by operating system
void vfs_register_rtc_cb( sint32_t (*cb)( vfs_time *tm ) )
{
  // allow NULL pointer to unregister callback function
  rtc_cb = cb;
}

// called by file system drivers
sint32_t vfs_get_rtc( vfs_time *tm )
{
  if (rtc_cb) {
    return rtc_cb( tm );
  }

  return VFS_RES_ERR;
}


static int dir_level = 1;

static const char *normalize_path( const char *path )
{
#if ! LDRV_TRAVERSAL
  return path;
#else
  const char *temp = path;
  size_t len;

  while ((len = c_strlen( temp )) >= 2) {
    if (temp[0] == '.' && temp[1] == '.') {
      --dir_level;
      if (len >= 4 && dir_level > 0) {
        // prepare next step
        temp = &(temp[4]);
      } else {
        // we're at top, the remainder is expected be an absolute path
        temp = &(temp[3]);
      }
    } else {
      break;
    }
  }

  if (dir_level > 0) {
    // no traversal on root level
    return path;
  } else {
    // path traverses via root
    return temp;
  }
#endif
}


// ---------------------------------------------------------------------------
// file system functions
//
vfs_vol *vfs_mount( const char *name, int num )
{
  vfs_fs_fns *fs_fns;
  const char *normname = normalize_path( name );
  char *outname;

#ifdef BUILD_SPIFFS
  if (fs_fns = myspiffs_realm( normname, &outname, FALSE )) {
    return fs_fns->mount( outname, num );
  }
#endif

#ifdef BUILD_FATFS
  if (fs_fns = myfatfs_realm( normname, &outname, FALSE )) {
    vfs_vol *r = fs_fns->mount( outname, num );
    c_free( outname );
    return r;
  }
#endif

  return NULL;
}

int vfs_open( const char *name, const char *mode )
{
  vfs_fs_fns *fs_fns;
  const char *normname = normalize_path( name );
  char *outname;

#ifdef BUILD_SPIFFS
  if (fs_fns = myspiffs_realm( normname, &outname, FALSE )) {
    return (int)fs_fns->open( outname, mode );
  }
#endif

#ifdef BUILD_FATFS
  if (fs_fns = myfatfs_realm( normname, &outname, FALSE )) {
    int r = (int)fs_fns->open( outname, mode );
    c_free( outname );
    return r;
  }
#endif

  return 0;
}

vfs_dir *vfs_opendir( const char *name )
{
  vfs_fs_fns *fs_fns;
  const char *normname = normalize_path( name );
  char *outname;

#ifdef BUILD_SPIFFS
  if (fs_fns = myspiffs_realm( normname, &outname, FALSE )) {
    return fs_fns->opendir( outname );
  }
#endif

#ifdef BUILD_FATFS
  if (fs_fns = myfatfs_realm( normname, &outname, FALSE )) {
    vfs_dir *r = fs_fns->opendir( outname );
    c_free( outname );
    return r;
  }
#endif

  return NULL;
}

sint32_t vfs_stat( const char *name, struct vfs_stat *buf )
{
  vfs_fs_fns *fs_fns;
  const char *normname = normalize_path( name );
  char *outname;

#ifdef BUILD_SPIFFS
  if (fs_fns = myspiffs_realm( normname, &outname, FALSE )) {
    return fs_fns->stat( outname, buf );
  }
#endif

#ifdef BUILD_FATFS
  if (fs_fns = myfatfs_realm( normname, &outname, FALSE )) {
    sint32_t r = fs_fns->stat( outname, buf );
    c_free( outname );
    return r;
  }
#endif

  return VFS_RES_ERR;
}

sint32_t vfs_remove( const char *name )
{
  vfs_fs_fns *fs_fns;
  const char *normname = normalize_path( name );
  char *outname;

#ifdef BUILD_SPIFFS
  if (fs_fns = myspiffs_realm( normname, &outname, FALSE )) {
    return fs_fns->remove( outname );
  }
#endif

#ifdef BUILD_FATFS
  if (fs_fns = myfatfs_realm( normname, &outname, FALSE )) {
    sint32_t r = fs_fns->remove( outname );
    c_free( outname );
    return r;
  }
#endif

  return VFS_RES_ERR;
}

sint32_t vfs_rename( const char *oldname, const char *newname )
{
  vfs_fs_fns *fs_fns;
  const char *normoldname = normalize_path( oldname );
  const char *normnewname = normalize_path( newname );
  char *oldoutname, *newoutname;

#ifdef BUILD_SPIFFS
  if (myspiffs_realm( normoldname, &oldoutname, FALSE )) {
    if (fs_fns = myspiffs_realm( normnewname, &newoutname, FALSE )) {
      return fs_fns->rename( oldoutname, newoutname );
    }
  }
#endif

#ifdef BUILD_FATFS
  if (myfatfs_realm( normoldname, &oldoutname, FALSE )) {
    if (fs_fns = myfatfs_realm( normnewname, &newoutname, FALSE )) {
      sint32_t r = fs_fns->rename( oldoutname, newoutname );
      c_free( oldoutname );
      c_free( newoutname );
      return r;
    }
    c_free( oldoutname );
  }
#endif

  return -1;
}

sint32_t vfs_mkdir( const char *name )
{
  vfs_fs_fns *fs_fns;
  const char *normname = normalize_path( name );
  char *outname;

#ifdef BUILD_SPIFFS
  // not supported
#endif

#ifdef BUILD_FATFS
  if (fs_fns = myfatfs_realm( normname, &outname, FALSE )) {
    sint32_t r = fs_fns->mkdir( outname );
    c_free( outname );
    return r;
  }
#endif

  return VFS_RES_ERR;
}

sint32_t vfs_fsinfo( const char *name, uint32_t *total, uint32_t *used )
{
  vfs_fs_fns *fs_fns;
  char *outname;

  if (!name) name = "";  // current drive

  const char *normname = normalize_path( name );

#ifdef BUILD_SPIFFS
  if (fs_fns = myspiffs_realm( normname, &outname, FALSE )) {
    return fs_fns->fsinfo( total, used );
  }
#endif

#ifdef BUILD_FATFS
  if (fs_fns = myfatfs_realm( normname, &outname, FALSE )) {
    c_free( outname );
    return fs_fns->fsinfo( total, used );
  }
#endif

  return VFS_RES_ERR;
}

sint32_t vfs_fscfg( const char *name, uint32_t *phys_addr, uint32_t *phys_size)
{
  vfs_fs_fns *fs_fns;
  char *outname;

#ifdef BUILD_SPIFFS
  if (fs_fns = myspiffs_realm( "/FLASH", &outname, FALSE )) {
    return fs_fns->fscfg( phys_addr, phys_size );
  }
#endif

#ifdef BUILD_FATFS
  // not supported
#endif

  // Error
  return VFS_RES_ERR;
}

sint32_t vfs_format( void )
{
  vfs_fs_fns *fs_fns;
  char *outname;

#ifdef BUILD_SPIFFS
  if (fs_fns = myspiffs_realm( "/FLASH", &outname, FALSE )) {
    return fs_fns->format();
  }
#endif

#ifdef BUILD_FATFS
  // not supported
#endif

  // Error
  return 0;
}

sint32_t vfs_chdir( const char *path )
{
  vfs_fs_fns *fs_fns;
  const char *normpath = normalize_path( path );
  const char *level;
  char *outname;
  int ok = VFS_RES_ERR;

#if LDRV_TRAVERSAL
  // track dir level
  if (normpath[0] == '/') {
    dir_level = 0;
    level = &(normpath[1]);
  } else {
    level = normpath;
  }
  while (c_strlen( level ) > 0) {
    dir_level++;
    if (level = c_strchr( level, '/' )) {
      level++;
    } else {
      break;
    }
  }
#endif

#ifdef BUILD_SPIFFS
  if (fs_fns = myspiffs_realm( normpath, &outname, TRUE )) {
    // our SPIFFS integration doesn't support directories
    if (c_strlen( outname ) == 0) {
      ok = VFS_RES_OK;
    }
  }
#endif

#ifdef BUILD_FATFS
  if (fs_fns = myfatfs_realm( normpath, &outname, TRUE )) {
    if (c_strchr( outname, ':' )) {
      // need to set FatFS' default drive
      fs_fns->chdrive( outname );
      // and force chdir to root in case path points only to root
      fs_fns->chdir( "/" );
    }
    if (fs_fns->chdir( outname ) == VFS_RES_OK) {
      ok = VFS_RES_OK;
    }
    c_free( outname );
  }
#endif

  return ok == VFS_RES_OK ? VFS_RES_OK : VFS_RES_ERR;
}

sint32_t vfs_errno( const char *name )
{
  vfs_fs_fns *fs_fns;
  char *outname;

  if (!name) name = "";  // current drive

  const char *normname = normalize_path( name );

#ifdef BUILD_SPIFFS
  if (fs_fns = myspiffs_realm( normname, &outname, FALSE )) {
    return fs_fns->ferrno( );
  }
#endif

#ifdef BUILD_FATFS
  if (fs_fns = myfatfs_realm( normname, &outname, FALSE )) {
    sint32_t r = fs_fns->ferrno( );
    c_free( outname );
    return r;
  }
#endif

  return VFS_RES_ERR;
}

sint32_t vfs_ferrno( int fd )
{
  vfs_file *f = (vfs_file *)fd;

  if (f) {
    return f->fns->ferrno ? f->fns->ferrno( f ) : 0;
  } else {
    vfs_fs_fns *fs_fns;
    const char *name = "";  // current drive
    char *outname;

#ifdef BUILD_SPIFFS
    if (fs_fns = myspiffs_realm( name, &outname, FALSE )) {
      return fs_fns->ferrno( );
    }
#endif

#ifdef BUILD_FATFS
    if (fs_fns = myfatfs_realm( name, &outname, FALSE )) {
      sint32_t r = fs_fns->ferrno( );
      c_free( outname );
      return r;
    }
#endif
  }
}


void vfs_clearerr( const char *name )
{
  vfs_fs_fns *fs_fns;
  char *outname;

  if (!name) name = "";  // current drive

  const char *normname = normalize_path( name );

#ifdef BUILD_SPIFFS
  if (fs_fns = myspiffs_realm( normname, &outname, FALSE )) {
    fs_fns->clearerr( );
  }
#endif

#ifdef BUILD_FATFS
  if (fs_fns = myfatfs_realm( normname, &outname, FALSE )) {
    fs_fns->clearerr( );
    c_free( outname );
  }
#endif
}

const char *vfs_basename( const char *path )
{
  const char *basename;

  // deduce basename (incl. extension) for length check
  if (basename = c_strrchr( path, '/' )) {
    basename++;
  } else if (basename = c_strrchr( path, ':' )) {
    basename++;
  } else {
    basename = path;
  }

  return basename;
}


// ---------------------------------------------------------------------------
// supplementary functions
//
int vfs_getc( int fd )
{
  unsigned char c = 0xFF;
  sint32_t res;

  if(!vfs_eof( fd )) {
    if (1 != vfs_read( fd, &c, 1 )) {
      NODE_DBG("getc errno %i\n", vfs_ferrno( fd ));
      return VFS_EOF;
    } else {
      return (int)c;
    }
  }
  return VFS_EOF;
}

int vfs_ungetc( int c, int fd )
{
  return vfs_lseek( fd, -1, VFS_SEEK_CUR );
}
