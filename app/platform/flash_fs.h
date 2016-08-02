
#ifndef __FLASH_FS_H__
#define __FLASH_FS_H__

#include "user_config.h"

#if defined( BUILD_SPIFFS )

#include "myspiffs.h"

#define FS_OPEN_OK	1

#define FS_RDONLY SPIFFS_RDONLY
#define FS_WRONLY SPIFFS_WRONLY
#define FS_RDWR SPIFFS_RDWR
#define FS_APPEND SPIFFS_APPEND
#define FS_TRUNC SPIFFS_TRUNC
#define FS_CREAT SPIFFS_CREAT
#define FS_EXCL SPIFFS_EXCL

#define FS_SEEK_SET SPIFFS_SEEK_SET
#define FS_SEEK_CUR SPIFFS_SEEK_CUR
#define FS_SEEK_END SPIFFS_SEEK_END

#define fs_open myspiffs_open
#define fs_close myspiffs_close
#define fs_write myspiffs_write
#define fs_read myspiffs_read
#define fs_seek myspiffs_lseek
#define fs_eof myspiffs_eof
#define fs_getc myspiffs_getc
#define fs_ungetc myspiffs_ungetc
#define fs_flush myspiffs_flush
#define fs_error myspiffs_error
#define fs_clearerr myspiffs_clearerr
#define fs_tell myspiffs_tell

#define fs_format myspiffs_format
#define fs_check myspiffs_check
#define fs_rename myspiffs_rename
#define fs_size myspiffs_size

#define fs_mount myspiffs_mount
#define fs_unmount myspiffs_unmount

#define FS_NAME_MAX_LENGTH SPIFFS_OBJ_NAME_LEN

#endif

int fs_mode2flag(const char *mode);

#endif // #ifndef __FLASH_FS_H__
