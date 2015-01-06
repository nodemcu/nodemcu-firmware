#include "flash_fs.h"
#include "c_string.h"

#if defined( BUILD_WOFS )
#include "romfs.h"
#elif defined( BUILD_SPIFFS )
#include "spiffs.h"
#endif

int fs_mode2flag(const char *mode){
  if(c_strlen(mode)==1){
  	if(c_strcmp(mode,"w")==0)
  	  return FS_WRONLY|FS_CREAT|FS_TRUNC;
  	else if(c_strcmp(mode, "r")==0)
  	  return FS_RDONLY;
  	else if(c_strcmp(mode, "a")==0)
  	  return FS_WRONLY|FS_CREAT|FS_APPEND;
  	else
  	  return FS_RDONLY;
  } else if (c_strlen(mode)==2){
  	if(c_strcmp(mode,"r+")==0)
  	  return FS_RDWR;
  	else if(c_strcmp(mode, "w+")==0)
  	  return FS_RDWR|FS_CREAT|FS_TRUNC;
  	else if(c_strcmp(mode, "a+")==0)
  	  return FS_RDWR|FS_CREAT|FS_APPEND;
  	else
  	  return FS_RDONLY;
  } else {
  	return FS_RDONLY;
  }
}
