// Module for interfacing with file system

#include "module.h"
#include "lauxlib.h"
#include "lnodeaux.h"
#include "lmem.h"
#include "platform.h"

#include "vfs.h"
#include <string.h>

#define FILE_READ_CHUNK 1024

// use this time/date in absence of a timestamp
#define FILE_TIMEDEF_YEAR 1970
#define FILE_TIMEDEF_MON 01
#define FILE_TIMEDEF_DAY 01
#define FILE_TIMEDEF_HOUR 00
#define FILE_TIMEDEF_MIN 00
#define FILE_TIMEDEF_SEC 00

static int file_fd = 0;
static int file_fd_ref = LUA_NOREF;
static int rtc_cb_ref = LUA_NOREF;

typedef struct _file_fd_ud {
  int fd;
} file_fd_ud;

static void table2tm( lua_State *L, vfs_time *tm )
{
  int idx = lua_gettop( L );

  // extract items from table
  lua_getfield( L, idx, "year" );
  lua_getfield( L, idx, "mon" );
  lua_getfield( L, idx, "day" );
  lua_getfield( L, idx, "hour" );
  lua_getfield( L, idx, "min" );
  lua_getfield( L, idx, "sec" );

  tm->year = luaL_optint( L, ++idx, FILE_TIMEDEF_YEAR );
  tm->mon  = luaL_optint( L, ++idx, FILE_TIMEDEF_MON );
  tm->day  = luaL_optint( L, ++idx, FILE_TIMEDEF_DAY );
  tm->hour = luaL_optint( L, ++idx, FILE_TIMEDEF_HOUR );
  tm->min  = luaL_optint( L, ++idx, FILE_TIMEDEF_MIN );
  tm->sec  = luaL_optint( L, ++idx, FILE_TIMEDEF_SEC );

  // remove items from stack
  lua_pop( L, 6 );
}

static int32_t file_rtc_cb( vfs_time *tm )
{
  int32_t res = VFS_RES_ERR;

  if (rtc_cb_ref != LUA_NOREF) {
    lua_State *L = lua_getstate();

    lua_rawgeti( L, LUA_REGISTRYINDEX, rtc_cb_ref );
    luaL_pcallx( L, 0, 1 );

    if (lua_type( L, lua_gettop( L ) ) == LUA_TTABLE) {
      table2tm( L, tm );
      res = VFS_RES_OK;
    }

    // pop item returned by callback
    lua_pop( L, 1 );
  }

  return res;
}

// Lua: on()
static int file_on(lua_State *L)
{
  enum events{
    ON_RTC = 0
  };
  const char *const eventnames[] = {"rtc", NULL};

  int event = luaL_checkoption(L, 1, "rtc", eventnames);

  switch (event) {
  case ON_RTC:
    luaL_unref(L, LUA_REGISTRYINDEX, rtc_cb_ref);

    if (lua_isfunction(L, 2)) {
      lua_pushvalue(L, 2);  // copy argument (func) to the top of stack
      rtc_cb_ref = luaL_ref(L, LUA_REGISTRYINDEX);
      vfs_register_rtc_cb(file_rtc_cb);
    } else {
      rtc_cb_ref = LUA_NOREF;
      vfs_register_rtc_cb(NULL);
    }
    break;
  default:
    break;
  }

  return 0;
}

// Lua: close()
static int file_close( lua_State* L )
{
  file_fd_ud *ud;

  if (lua_type( L, 1 ) != LUA_TUSERDATA) {
    // fall back to last opened file
    if (file_fd_ref != LUA_NOREF) {
      lua_rawgeti( L, LUA_REGISTRYINDEX, file_fd_ref );
      // top of stack is now default file descriptor
      ud = (file_fd_ud *)luaL_checkudata(L, -1, "file.obj");
      lua_pop( L, 1 );
    } else {
      // no default file currently opened
      return 0;
    }
  } else {
    ud = (file_fd_ud *)luaL_checkudata(L, 1, "file.obj");
  }

  if(ud->fd){
      vfs_close(ud->fd);
      // mark as closed
      ud->fd = 0;
  }

  // unref default file descriptor
  luaL_unref( L, LUA_REGISTRYINDEX, file_fd_ref );
  file_fd_ref = LUA_NOREF;

  return 0;  
}

static int file_obj_free( lua_State *L )
{
  file_fd_ud *ud = (file_fd_ud *)luaL_checkudata(L, 1, "file.obj");
  if (ud->fd) {
    // close file if it's still open
    vfs_close(ud->fd);
  }

  return 0;
}

#ifdef CONFIG_NODEMCU_BUILD_SPIFFS
// Lua: format()
static int file_format( lua_State* L )
{
  file_close(L);
  if( !vfs_format() )
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

static int file_fscfg (lua_State *L)
{
  uint32_t phys_addr, phys_size;

  vfs_fscfg("/FLASH", &phys_addr, &phys_size);

  lua_pushinteger (L, phys_addr);
  lua_pushinteger (L, phys_size);
  return 2;
}
#endif

// Lua: open(filename, mode)
static int file_open( lua_State* L )
{
  size_t len;

  // unref last file descriptor to allow gc'ing if not kept by user script
  luaL_unref( L, LUA_REGISTRYINDEX, file_fd_ref );
  file_fd_ref = LUA_NOREF;

  const char *fname = luaL_checklstring( L, 1, &len );
  const char *basename = vfs_basename( fname );
  luaL_argcheck(L, strlen(basename) <= CONFIG_NODEMCU_FS_OBJ_NAME_LEN && strlen(fname) == len, 1, "filename invalid");

  const char *mode = luaL_optstring(L, 2, "r");

  file_fd = vfs_open(fname, mode);

  if(!file_fd){
    lua_pushnil(L);
  } else {
    file_fd_ud *ud = (file_fd_ud *) lua_newuserdata( L, sizeof( file_fd_ud ) );
    ud->fd = file_fd;
    luaL_getmetatable( L, "file.obj" );
    lua_setmetatable( L, -2 );

    // store reference to opened file
    lua_pushvalue( L, -1 );
    file_fd_ref = luaL_ref( L, LUA_REGISTRYINDEX );
  }
  return 1; 
}

// Lua: list()
static int file_list( lua_State* L )
{
  vfs_dir  *dir;

  if ((dir = vfs_opendir(""))) {
    lua_newtable( L );
    struct vfs_stat stat;
    while (vfs_readdir(dir, &stat) == VFS_RES_OK) {
      lua_pushinteger(L, stat.size);
      lua_setfield(L, -2, stat.name);
    }
    vfs_closedir(dir);
    return 1;
  }
  return 0;
}

static int get_file_obj( lua_State *L, int *argpos )
{
  if (lua_type( L, 1 ) == LUA_TUSERDATA) {
    file_fd_ud *ud = (file_fd_ud *)luaL_checkudata(L, 1, "file.obj");
    *argpos = 2;
    return ud->fd;
  } else {
    *argpos = 1;
    return file_fd;
  }
}

#define GET_FILE_OBJ int argpos; \
  int fd = get_file_obj( L, &argpos );

static int file_seek (lua_State *L)
{
  GET_FILE_OBJ;

  static const int mode[] = {VFS_SEEK_SET, VFS_SEEK_CUR, VFS_SEEK_END};
  static const char *const modenames[] = {"set", "cur", "end", NULL};
  if(!fd)
    return luaL_error(L, "open a file first");
  int op = luaL_checkoption(L, argpos, "cur", modenames);
  long offset = luaL_optlong(L, ++argpos, 0);
  op = vfs_lseek(fd, offset, mode[op]);
  if (op < 0)
    lua_pushnil(L);  /* error */
  else
    lua_pushinteger(L, vfs_tell(fd));
  return 1;
}

// Lua: exists(filename)
static int file_exists( lua_State* L )
{
  size_t len;
  const char *fname = luaL_checklstring( L, 1, &len );    
  const char *basename = vfs_basename( fname );
  luaL_argcheck(L, strlen(basename) <= CONFIG_NODEMCU_FS_OBJ_NAME_LEN && strlen(fname) == len, 1, "filename invalid");

  struct vfs_stat stat;
  lua_pushboolean(L, vfs_stat((char *)fname, &stat) == VFS_RES_OK ? 1 : 0);

  return 1;
}

// Lua: remove(filename)
static int file_remove( lua_State* L )
{
  size_t len;
  const char *fname = luaL_checklstring( L, 1, &len );    
  const char *basename = vfs_basename( fname );
  luaL_argcheck(L, strlen(basename) <= CONFIG_NODEMCU_FS_OBJ_NAME_LEN && strlen(fname) == len, 1, "filename invalid");
  vfs_remove((char *)fname);
  return 0;
}

// Lua: flush()
static int file_flush( lua_State* L )
{
  GET_FILE_OBJ;

  if(!fd)
    return luaL_error(L, "open a file first");
  if(vfs_flush(fd) == 0)
    lua_pushboolean(L, 1);
  else
    lua_pushnil(L);
  return 1;
}

// Lua: rename("oldname", "newname")
static int file_rename( lua_State* L )
{
  size_t len;

  const char *oldname = luaL_checklstring( L, 1, &len );
  const char *basename = vfs_basename( oldname );
  luaL_argcheck(L, strlen(basename) <= CONFIG_NODEMCU_FS_OBJ_NAME_LEN && strlen(oldname) == len, 1, "filename invalid");
  
  const char *newname = luaL_checklstring( L, 2, &len );  
  basename = vfs_basename( newname );
  luaL_argcheck(L, strlen(basename) <= CONFIG_NODEMCU_FS_OBJ_NAME_LEN && strlen(newname) == len, 2, "filename invalid");

  if(0 <= vfs_rename( oldname, newname )){
    lua_pushboolean(L, 1);
  } else {
    lua_pushboolean(L, 0);
  }
  return 1;
}

// Lua: stat(filename)
static int file_stat( lua_State* L )
{
  size_t len;
  const char *fname = luaL_checklstring( L, 1, &len );    
  luaL_argcheck( L, strlen(fname) <= CONFIG_NODEMCU_FS_OBJ_NAME_LEN && strlen(fname) == len, 1, "filename invalid" );

  struct vfs_stat stat;
  if (vfs_stat( (char *)fname, &stat ) != VFS_RES_OK) {
    lua_pushnil( L );
    return 1;
  }

  lua_createtable( L, 0, 7 );

  lua_pushinteger( L, stat.size );
  lua_setfield( L, -2, "size" );

  lua_pushstring( L, stat.name );
  lua_setfield( L, -2, "name" );

  lua_pushboolean( L, stat.is_dir );
  lua_setfield( L, -2, "is_dir" );

  lua_pushboolean( L, stat.is_rdonly );
  lua_setfield( L, -2, "is_rdonly" );

  lua_pushboolean( L, stat.is_hidden );
  lua_setfield( L, -2, "is_hidden" );

  lua_pushboolean( L, stat.is_sys );
  lua_setfield( L, -2, "is_sys" );

  lua_pushboolean( L, stat.is_arch );
  lua_setfield( L, -2, "is_arch" );

  // time stamp as sub-table
  lua_createtable( L, 0, 6 );

  lua_pushinteger( L, stat.tm_valid ? stat.tm.year : FILE_TIMEDEF_YEAR );
  lua_setfield( L, -2, "year" );

  lua_pushinteger( L, stat.tm_valid ? stat.tm.mon : FILE_TIMEDEF_MON );
  lua_setfield( L, -2, "mon" );

  lua_pushinteger( L, stat.tm_valid ? stat.tm.day : FILE_TIMEDEF_DAY );
  lua_setfield( L, -2, "day" );

  lua_pushinteger( L, stat.tm_valid ? stat.tm.hour : FILE_TIMEDEF_HOUR );
  lua_setfield( L, -2, "hour" );

  lua_pushinteger( L, stat.tm_valid ? stat.tm.min : FILE_TIMEDEF_MIN );
  lua_setfield( L, -2, "min" );

  lua_pushinteger( L, stat.tm_valid ? stat.tm.sec : FILE_TIMEDEF_SEC );
  lua_setfield( L, -2, "sec" );

  lua_setfield( L, -2, "time" );

  return 1;
}

// g_read()
static int file_g_read( lua_State* L, int n, int16_t end_char, int fd )
{
  int i, j;
  luaL_Buffer b;
  char p[LUAL_BUFFERSIZE/2];

  if(!fd)
    return luaL_error(L, "open a file first");

  luaL_buffinit(L, &b);

  for (j = 0; j < n; j += sizeof(p)) { 
    int nwanted = (n - j >= sizeof(p)) ? sizeof(p) : n - j;
    int nread   = vfs_read(fd, p, nwanted);

    if (nread == VFS_RES_ERR || nread == 0) {
      if (j > 0) {
        break;
      }
      lua_pushnil(L);
      return 1;
    }

    for (i = 0; i < nread; ++i) {
      luaL_addchar(&b, p[i]);
      if (p[i] == end_char) {
        vfs_lseek(fd, -nread + i + 1, VFS_SEEK_CUR); //reposition after end char found
        nread = 0;   // force break on outer loop
        break;
      }
    }

    if (nread < nwanted)
      break;
  }
  luaL_pushresult(&b);
  return 1;
}

// Lua: read()
// file.read() will read all byte in file
// file.read(10) will read 10 byte from file, or EOF is reached.
// file.read('q') will read until 'q' or EOF is reached. 
static int file_read( lua_State* L )
{
  unsigned need_len = FILE_READ_CHUNK;
  int16_t end_char = EOF;
  size_t el;

  GET_FILE_OBJ;

  if( lua_type( L, argpos ) == LUA_TNUMBER )
  {
    need_len = ( unsigned )luaL_checkinteger( L, argpos );
  }
  else if(lua_isstring(L, argpos))
  {
    const char *end = luaL_checklstring( L, argpos, &el );
    if(el!=1){
      return luaL_error( L, "wrong arg range" );
    }
    end_char = (int16_t)end[0];
  }

  return file_g_read(L, need_len, end_char, fd);
}

// Lua: readline()
static int file_readline( lua_State* L )
{
  GET_FILE_OBJ;

  return file_g_read(L, FILE_READ_CHUNK, '\n', fd);
}


// Lua: getfile(filename)
static int file_getfile( lua_State* L )
{
  // Warning this code C calls other file_* routines to avoid duplication code.  These
  // use Lua stack addressing of arguments, so this does Lua stack maniplation to
  // align these
  int ret_cnt = 0;
  lua_settop(L ,1);
  // Stack [1] = FD
  file_open(L);
  // Stack [1] = filename; [2] = FD or nil
  if (!lua_isnil(L, -1)) {
    lua_remove(L, 1);  // dump filename, so [1] = FD
    file_fd_ud *ud = (file_fd_ud *)luaL_checkudata(L, 1, "file.obj");
    ret_cnt = file_g_read(L, LUAI_MAXINT32, EOF, ud->fd);
    // Stack [1] = FD; [2] = contents if ret_cnt = 1;
    file_close(L);     // leaves Stack unchanged if [1] = FD
    lua_remove(L, 1);  // Dump FD leaving contents as [1] / ToS
  }
  return ret_cnt;
}

// Lua: write("string")
static int file_write( lua_State* L )
{
  GET_FILE_OBJ;

  if(!fd)
    return luaL_error(L, "open a file first");
  size_t l, rl;
  const char *s = luaL_checklstring(L, argpos, &l);
  rl = vfs_write(fd, s, l);
  if(rl==l)
    lua_pushboolean(L, 1);
  else
    lua_pushnil(L);
  return 1;
}

// Lua: writeline("string")
static int file_writeline( lua_State* L )
{
  GET_FILE_OBJ;

  if(!fd)
    return luaL_error(L, "open a file first");
  size_t l, rl;
  const char *s = luaL_checklstring(L, argpos, &l);
  rl = vfs_write(fd, s, l);
  if(rl==l){
    rl = vfs_write(fd, "\n", 1);
    if(rl==1)
      lua_pushboolean(L, 1);
    else
      lua_pushnil(L);
  }
  else{
    lua_pushnil(L);
  }
  return 1;
}

// Lua: fsinfo()
static int file_fsinfo( lua_State* L )
{
  u32_t total, used;
  if (vfs_fsinfo("", &total, &used)) {
    return luaL_error(L, "file system failed");
  }
  NODE_DBG("total: %d, used:%d\n", total, used);
  if(total>0x7FFFFFFF || used>0x7FFFFFFF || used > total)
  {
    return luaL_error(L, "file system error");
  }
  lua_pushinteger(L, total-used);
  lua_pushinteger(L, used);
  lua_pushinteger(L, total);
  return 3;
}

// Lua: getfile(filename)
static int file_putfile( lua_State* L )
{
  // Warning this code C calls other file_* routines to avoid duplication code.  These
  // use Lua stack addressing of arguments, so this does Lua stack maniplation to
  // align these
  int ret_cnt = 0;
  lua_settop(L, 2);
  lua_pushvalue(L, 2); //dup contents onto the ToS [3]
  lua_pushliteral(L, "w+");
  lua_replace(L, 2);
  // Stack [1] = filename; [2] "w+" [3] contents;
  file_open(L);
  // Stack [1] = filename; [2] "w+" [3] contents; [4] FD or nil

  if (!lua_isnil(L, -1)) {
    lua_remove(L, 2);  //dump "w+" attribute literal
    lua_replace(L, 1);
    // Stack [1] = FD; [2] contents
    file_write(L);
    // Stack [1] = FD; [2] contents; [3] result status
    lua_remove(L, 2);  //dump contents
    file_close(L);
    lua_remove(L, 1); // Dump FD leaving status as ToS
  }
  return 1;
}



typedef struct {
  vfs_vol *vol;
} volume_type;

#ifdef CONFIG_NODEMCU_BUILD_FATFS
// Lua: success = file.chdir("/SD0/")
static int file_chdir( lua_State *L )
{
  const char *path = luaL_checkstring( L, 1 );

  lua_pushboolean( L, 0 <= vfs_chdir( path ) );
  return 1;
}
#endif

LROT_BEGIN(file_obj, NULL, LROT_MASK_GC_INDEX)
  LROT_FUNCENTRY( __gc,      file_obj_free )
  LROT_TABENTRY ( __index,   file_obj )
  LROT_FUNCENTRY( close,     file_close )
  LROT_FUNCENTRY( read,      file_read )
  LROT_FUNCENTRY( readline,  file_readline )
  LROT_FUNCENTRY( write,     file_write )
  LROT_FUNCENTRY( writeline, file_writeline )
  LROT_FUNCENTRY( seek,      file_seek )
  LROT_FUNCENTRY( flush,     file_flush )
LROT_END(file_obj, NULL, LROT_MASK_GC_INDEX)

// Module function map
LROT_BEGIN(file, NULL, 0)
  LROT_FUNCENTRY( list,      file_list )
  LROT_FUNCENTRY( open,      file_open )
  LROT_FUNCENTRY( close,     file_close )
  LROT_FUNCENTRY( write,     file_write )
  LROT_FUNCENTRY( writeline, file_writeline )
  LROT_FUNCENTRY( read,      file_read )
  LROT_FUNCENTRY( readline,  file_readline )
#ifdef CONFIG_NODEMCU_BUILD_SPIFFS
  LROT_FUNCENTRY( format,    file_format )
  LROT_FUNCENTRY( fscfg,     file_fscfg )
#endif
  LROT_FUNCENTRY( remove,    file_remove )
  LROT_FUNCENTRY( seek,      file_seek )
  LROT_FUNCENTRY( flush,     file_flush )
  LROT_FUNCENTRY( rename,    file_rename )
  LROT_FUNCENTRY( exists,    file_exists )
  LROT_FUNCENTRY( getcontents,  file_getfile )
  LROT_FUNCENTRY( putcontents,  file_putfile )
  LROT_FUNCENTRY( fsinfo,    file_fsinfo )
  LROT_FUNCENTRY( on,        file_on )
  LROT_FUNCENTRY( stat,      file_stat )
#ifdef CONFIG_NODEMCU_BUILD_FATFS
  LROT_FUNCENTRY( chdir,     file_chdir )
#endif
LROT_END(file, NULL, 0)

int luaopen_file( lua_State *L ) {
  luaL_rometatable( L, "file.obj",  LROT_TABLEREF(file_obj));
  return 0;
}

NODEMCU_MODULE(FILE, "file", file, luaopen_file);
