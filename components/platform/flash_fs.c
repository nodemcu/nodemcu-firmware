#include "flash_fs.h"
#include <string.h>

int fs_mode2flag(const char *mode){
  if(strlen(mode)==1){
  	if(strcmp(mode,"w")==0)
  	  return FS_WRONLY|FS_CREAT|FS_TRUNC;
  	else if(strcmp(mode, "r")==0)
  	  return FS_RDONLY;
  	else if(strcmp(mode, "a")==0)
  	  return FS_WRONLY|FS_CREAT|FS_APPEND;
  	else
  	  return FS_RDONLY;
  } else if (strlen(mode)==2){
  	if(strcmp(mode,"r+")==0)
  	  return FS_RDWR;
  	else if(strcmp(mode, "w+")==0)
  	  return FS_RDWR|FS_CREAT|FS_TRUNC;
  	else if(strcmp(mode, "a+")==0)
  	  return FS_RDWR|FS_CREAT|FS_APPEND;
  	else
  	  return FS_RDONLY;
  } else {
  	return FS_RDONLY;
  }
}
