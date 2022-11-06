// Module for interfacing with file system

#include "module.h"
#include "lauxlib.h"
#include "platform.h"
#include "esp_spiffs.h"

#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

static const char *default_fs_label =
  ((CONFIG_NODEMCU_DEFAULT_SPIFFS_LABEL &&
    (CONFIG_NODEMCU_DEFAULT_SPIFFS_LABEL)[0]) ?
      CONFIG_NODEMCU_DEFAULT_SPIFFS_LABEL : NULL);


// Lua: format()
static int file_format( lua_State* L )
{
  if(esp_spiffs_format(default_fs_label) != ESP_OK)
  {
    NODE_ERR( "\n*** ERROR ***: unable to format. FS might be compromised.\n" );
    NODE_ERR( "It is advised to re-flash the NodeMCU image.\n" );
    luaL_error(L, "Failed to format file system");
  }
  else{
    NODE_ERR( "format done.\n" );
  }
  return 0;
}


// Lua: list()
static int file_list( lua_State* L )
{
  const char *dirname = luaL_optstring(L, 1, NULL);

  DIR *dir;
  if ((dir = opendir(dirname ? dirname : "/"))) {
    lua_newtable( L );
    struct dirent *e;
    while ((e = readdir(dir))) {
      char *fname = NULL;
      if (dirname) {
        asprintf(&fname, "%s/%s", dirname, e->d_name);
        if (!fname) {
          closedir(dir);
          return luaL_error(L, "no memory");
        }
      } else {
        fname = e->d_name;
      }
      struct stat st = { 0, };
      int err = stat(fname, &st);
      if (err) {
        // We historically ignored this error, so just warn (although it
        // shouldn't really happen now).
        NODE_ERR("Failed to stat %s err=%d\n", fname, err);
      }
      if (dirname) {
        free(fname);
      }
      lua_pushinteger(L, st.st_size);
      lua_setfield(L, -2, e->d_name);
    }
    closedir(dir);
    return 1;
  }
  return 0;
}


// Lua: exists(filename)
static int file_exists( lua_State* L )
{
  size_t len;
  const char *fname = luaL_checklstring( L, 1, &len );    

  struct stat st;
  lua_pushboolean(L, stat(fname, &st) == 0 ? 1 : 0);

  return 1;
}


// Lua: remove(filename)
static int file_remove( lua_State* L )
{
  size_t len;
  const char *fname = luaL_checklstring( L, 1, &len );    
  unlink(fname);
  return 0;
}


// Lua: rename("oldname", "newname")
static int file_rename( lua_State* L )
{
  const char *oldname = luaL_checkstring( L, 1);
  const char *newname = luaL_checkstring( L, 2);
  if (rename(oldname, newname) == 0) {
    lua_pushboolean(L, 1);
  } else {
    lua_pushboolean(L, 0);
  }
  return 1;
}


// Lua: fsinfo()
static int file_fsinfo( lua_State* L )
{
  size_t total, used;
  if (esp_spiffs_info(default_fs_label, &total, &used) != ESP_OK)
    return luaL_error(L, "spiffs file system not mounted");

  if(total>0x7FFFFFFF || used>0x7FFFFFFF || used > total)
  {
    return luaL_error(L, "file system error");
  }
  lua_pushinteger(L, total-used);
  lua_pushinteger(L, used);
  lua_pushinteger(L, total);
  return 3;
}


// Module function map
LROT_BEGIN(file, NULL, 0)
  LROT_FUNCENTRY( list,      file_list )
  LROT_FUNCENTRY( format,    file_format )
  LROT_FUNCENTRY( remove,    file_remove )
  LROT_FUNCENTRY( rename,    file_rename )
  LROT_FUNCENTRY( exists,    file_exists )
  LROT_FUNCENTRY( fsinfo,    file_fsinfo )
LROT_END(file, NULL, 0)


NODEMCU_MODULE(FILE, "file", file, NULL);
