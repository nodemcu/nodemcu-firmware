

#include "vfs.h"



// ---------------------------------------------------------------------------
// RTC system interface
//
static sint32_t (*rtc_cb)( vfs_time *tm ) = NULL;

// called by operating system
void vfs_register_rtc_cb( sint32_t (*cb)( const struct vfs_time *tm ) )
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


// ---------------------------------------------------------------------------
// file system functions
//
vfs_vol *vfs_mount( const char *name, int num )
{
  vfs_fs_fns *fs_fns;
  const char *outname;

#ifdef BUILD_SPIFFS
  if (fs_fns = myspiffs_realm( name, &outname )) {
    return fs_fns->mount( outname, num );
  }
#endif

#ifdef BUILD_FATFS
  if (fs_fns = myfatfs_realm( name, &outname )) {
    return fs_fns->mount( outname, num );
  }
#endif

  return NULL;
}

int vfs_open( const char *name, const char *mode )
{
  vfs_fs_fns *fs_fns;
  const char *outname;

#ifdef BUILD_SPIFFS
  if (fs_fns = myspiffs_realm( name, &outname )) {
    return (int)fs_fns->open( outname, mode );
  }
#endif

#ifdef BUILD_FATFS
  if (fs_fns = myfatfs_realm( name, &outname )) {
    return (int)fs_fns->open( outname, mode );
  }
#endif

  return 0;
}

vfs_dir *vfs_opendir( const char *name )
{
  vfs_fs_fns *fs_fns;
  const char *outname;

#ifdef BUILD_SPIFFS
  if (fs_fns = myspiffs_realm( name, &outname )) {
    return fs_fns->opendir( outname );
  }
#endif

#ifdef BUILD_FATFS
  if (fs_fns = myfatfs_realm( name, &outname )) {
    return fs_fns->opendir( outname );
  }
#endif

  return NULL;
}

vfs_item *vfs_stat( const char *name )
{
  vfs_fs_fns *fs_fns;
  const char *outname;

#ifdef BUILD_SPIFFS
  if (fs_fns = myspiffs_realm( name, &outname )) {
    return fs_fns->stat( outname );
  }
#endif

#ifdef BUILD_FATFS
  if (fs_fns = myfatfs_realm( name, &outname )) {
    return fs_fns->stat( outname );
  }
#endif

  return NULL;
}

sint32_t vfs_remove( const char *name )
{
  vfs_fs_fns *fs_fns;
  const char *outname;

#ifdef BUILD_SPIFFS
  if (fs_fns = myspiffs_realm( name, &outname )) {
    return fs_fns->remove( outname );
  }
#endif

#ifdef BUILD_FATFS
  if (fs_fns = myfatfs_realm( name, &outname )) {
    return fs_fns->remove( outname );
  }
#endif

  return VFS_RES_ERR;
}

sint32_t vfs_rename( const char *oldname, const char *newname )
{
  vfs_fs_fns *fs_fns;
  const char *oldoutname, *newoutname;

#ifdef BUILD_SPIFFS
  if (myspiffs_realm( oldname, &oldoutname )) {
    if (fs_fns = myspiffs_realm( newname, &newoutname )) {
      return fs_fns->rename( oldoutname, newoutname );
    }
  }
#endif

#ifdef BUILD_FATFS
  if (myfatfs_realm( oldname, &oldoutname )) {
    if (fs_fns = myfatfs_realm( newname, &newoutname )) {
      return fs_fns->rename( oldoutname, newoutname );
    }
  }
#endif

  return -1;
}

sint32_t vfs_mkdir( const char *name )
{
  vfs_fs_fns *fs_fns;
  const char *outname;

#ifdef BUILD_SPIFFS
  // not supported
#endif

#ifdef BUILD_FATFS
  if (fs_fns = myfatfs_realm( name, &outname )) {
    return fs_fns->mkdir( outname );
  }
#endif

  return VFS_RES_ERR;
}

sint32_t vfs_fsinfo( const char *name, uint32_t *total, uint32_t *used )
{
  vfs_fs_fns *fs_fns;
  const char *outname;

  if (!name) name = "";  // current drive

#ifdef BUILD_SPIFFS
  if (fs_fns = myspiffs_realm( name, &outname )) {
    return fs_fns->fsinfo( total, used );
  }
#endif

#ifdef BUILD_FATFS
  if (fs_fns = myfatfs_realm( name, &outname )) {
    // not supported
    *total = *used = 0;
    return VFS_RES_OK;
  }
#endif

  return VFS_RES_ERR;
}

sint32_t vfs_fscfg( const char *name, uint32_t *phys_addr, uint32_t *phys_size)
{
  vfs_fs_fns *fs_fns;
  const char *outname;

#ifdef BUILD_SPIFFS
  if (fs_fns = myspiffs_realm( "FLASH:", &outname )) {
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
  const char *outname;

#ifdef BUILD_SPIFFS
  if (fs_fns = myspiffs_realm( "FLASH:", &outname )) {
    return fs_fns->format();
  }
#endif

#ifdef BUILD_FATFS
  // not supported
#endif

  // Error
  return 0;
}

sint32_t vfs_chdrive( const char *ldrv )
{
  vfs_fs_fns *fs_fns;
  const char *outname;
  int ok = VFS_RES_OK;
  int any = VFS_RES_ERR;

#ifdef BUILD_SPIFFS
  if (fs_fns = myspiffs_realm( "FLASH:", &outname )) {
    if (fs_fns->chdrive( ldrv ) >= 0) {
      any = VFS_RES_OK;
    }
  } else {
    ok = VFS_RES_ERR;
  }
#endif

#ifdef BUILD_FATFS
  if (fs_fns = myfatfs_realm( "SD0:", &outname )) {
    if (fs_fns->chdrive( ldrv ) >= 0) {
      any = VFS_RES_OK;
    }
  } else {
    ok = VFS_RES_ERR;
  }
#endif

  return (ok == VFS_RES_OK) && (any == VFS_RES_OK) ? VFS_RES_OK : VFS_RES_ERR;
}

sint32_t vfs_errno( const char *name )
{
  vfs_fs_fns *fs_fns;
  const char *outname;

  if (!name) name = "";  // current drive

#ifdef BUILD_SPIFFS
  if (fs_fns = myspiffs_realm( name, &outname )) {
    return fs_fns->ferrno( );
  }
#endif

#ifdef BUILD_FATFS
  if (fs_fns = myfatfs_realm( name, &outname )) {
    return fs_fns->ferrno( );
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
    const char *outname;

#ifdef BUILD_SPIFFS
    if (fs_fns = myspiffs_realm( name, &outname )) {
      return fs_fns->ferrno( );
    }
#endif

#ifdef BUILD_FATFS
    if (fs_fns = myfatfs_realm( name, &outname )) {
      return fs_fns->ferrno( );
    }
#endif
  }
}


void vfs_clearerr( const char *name )
{
  vfs_fs_fns *fs_fns;
  const char *outname;

  if (!name) name = "";  // current drive

#ifdef BUILD_SPIFFS
  if (fs_fns = myspiffs_realm( name, &outname )) {
    fs_fns->clearerr( );
  }
#endif

#ifdef BUILD_FATFS
  if (fs_fns = myfatfs_realm( name, &outname )) {
    fs_fns->clearerr( );
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
