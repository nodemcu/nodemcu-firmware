// Module for interfacing with file system

#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
#include "platform.h"
#include "auxmods.h"
#include "lrotable.h"

#include "c_types.h"
#include "flash_fs.h"
#include "c_string.h"

static volatile int file_fd = FS_OPEN_OK - 1;

// Lua: open(filename, mode)
static int file_open( lua_State* L )
{
  size_t len;
  if((FS_OPEN_OK - 1)!=file_fd){
    fs_close(file_fd);
    file_fd = FS_OPEN_OK - 1;
  }

  const char *fname = luaL_checklstring( L, 1, &len );
  if( len > FS_NAME_MAX_LENGTH )
    return luaL_error(L, "filename too long");
  const char *mode = luaL_optstring(L, 2, "r");

  file_fd = fs_open(fname, fs_mode2flag(mode));

  if(file_fd < FS_OPEN_OK){
    file_fd = FS_OPEN_OK - 1;
    lua_pushnil(L);
  } else {
    lua_pushboolean(L, 1);
  }
  return 1; 
}

// Lua: close()
static int file_close( lua_State* L )
{
  if((FS_OPEN_OK - 1)!=file_fd){
    fs_close(file_fd);
    file_fd = FS_OPEN_OK - 1;
  }
  return 0;  
}

// Lua: format()
static int file_format( lua_State* L )
{
  size_t len;
  file_close(L);
  if( !fs_format() )
  {
    NODE_ERR( "\ni*** ERROR ***: unable to format. FS might be compromised.\n" );
    NODE_ERR( "It is advised to re-flash the NodeMCU image.\n" );
  }
  else{
    NODE_ERR( "format done.\n" );
  }
  return 0; 
}

#if defined(BUILD_WOFS)
// Lua: list()
static int file_list( lua_State* L )
{
  uint32_t start = 0;
  size_t act_len = 0;
  char fsname[ FS_NAME_MAX_LENGTH + 1 ];
  lua_newtable( L );
  while( FS_FILE_OK == wofs_next(&start, fsname, FS_NAME_MAX_LENGTH, &act_len) ){
    lua_pushinteger(L, act_len);
    lua_setfield( L, -2, fsname );
  }
  return 1;
}

#elif defined(BUILD_SPIFFS)

extern spiffs fs;

// Lua: list()
static int file_list( lua_State* L )
{
  spiffs_DIR d;
  struct spiffs_dirent e;
  struct spiffs_dirent *pe = &e;

  lua_newtable( L );
  SPIFFS_opendir(&fs, "/", &d);
  while ((pe = SPIFFS_readdir(&d, pe))) {
    // NODE_ERR("  %s size:%i\n", pe->name, pe->size);
    lua_pushinteger(L, pe->size);
    lua_setfield( L, -2, pe->name );
  }
  SPIFFS_closedir(&d);
  return 1;
}

static int file_seek (lua_State *L) 
{
  static const int mode[] = {FS_SEEK_SET, FS_SEEK_CUR, FS_SEEK_END};
  static const char *const modenames[] = {"set", "cur", "end", NULL};
  if((FS_OPEN_OK - 1)==file_fd)
    return luaL_error(L, "open a file first");
  int op = luaL_checkoption(L, 1, "cur", modenames);
  long offset = luaL_optlong(L, 2, 0);
  op = fs_seek(file_fd, offset, mode[op]);
  if (op)
    lua_pushnil(L);  /* error */
  else
    lua_pushinteger(L, fs_tell(file_fd));
  return 1;
}

// Lua: remove(filename)
static int file_remove( lua_State* L )
{
  size_t len;
  const char *fname = luaL_checklstring( L, 1, &len );
  if( len > FS_NAME_MAX_LENGTH )
    return luaL_error(L, "filename too long");
  file_close(L);
  SPIFFS_remove(&fs, (char *)fname);
  return 0;  
}

// Lua: flush()
static int file_flush( lua_State* L )
{
  if((FS_OPEN_OK - 1)==file_fd)
    return luaL_error(L, "open a file first");
  if(fs_flush(file_fd) == 0)
    lua_pushboolean(L, 1);
  else
    lua_pushnil(L);
  return 1;
}
#if 0
// Lua: check()
static int file_check( lua_State* L )
{
  file_close(L);
  lua_pushinteger(L, fs_check());
  return 1;
}
#endif

// Lua: rename("oldname", "newname")
static int file_rename( lua_State* L )
{
  size_t len;
  if((FS_OPEN_OK - 1)!=file_fd){
    fs_close(file_fd);
    file_fd = FS_OPEN_OK - 1;
  }

  const char *oldname = luaL_checklstring( L, 1, &len );
  if( len > FS_NAME_MAX_LENGTH )
    return luaL_error(L, "filename too long");

  const char *newname = luaL_checklstring( L, 2, &len );
  if( len > FS_NAME_MAX_LENGTH )
    return luaL_error(L, "filename too long");

  if(SPIFFS_OK==myspiffs_rename( oldname, newname )){
    lua_pushboolean(L, 1);
  } else {
    lua_pushboolean(L, 0);
  }
  return 1;
}

// Lua: fsinfo()
static int file_fsinfo( lua_State* L )
{
  uint32_t total, used;
  SPIFFS_info(&fs, &total, &used);
  NODE_DBG("total: %d, used:%d\n", total, used);
  if(total>0x7FFFFFFF || used>0x7FFFFFFF || used > total)
  {
    return luaL_error(L, "file system error");;
  }
  lua_pushinteger(L, total-used);
  lua_pushinteger(L, used);
  lua_pushinteger(L, total);
  return 3;
}

#endif

// g_read()
static int file_g_read( lua_State* L, int n, int16_t end_char )
{
  if(n< 0 || n>LUAL_BUFFERSIZE) 
    n = LUAL_BUFFERSIZE;
  if(end_char < 0 || end_char >255)
    end_char = EOF;
  int ec = (int)end_char;
  
  luaL_Buffer b;
  if((FS_OPEN_OK - 1)==file_fd)
    return luaL_error(L, "open a file first");

  luaL_buffinit(L, &b);
  char *p = luaL_prepbuffer(&b);
  int c = EOF;
  int i = 0;

  do{
    c = fs_getc(file_fd);
    if(c==EOF){
      break;
    }
    p[i++] = (char)(0xFF & c);
  }while((c!=EOF) && (c!=ec) && (i<n) );

#if 0
  if(i>0 && p[i-1] == '\n')
    i--;    /* do not include `eol' */
#endif
    
  if(i==0){
    luaL_pushresult(&b);  /* close buffer */
    return (lua_objlen(L, -1) > 0);  /* check whether read something */
  }

  luaL_addsize(&b, i);
  luaL_pushresult(&b);  /* close buffer */
  return 1;  /* read at least an `eol' */ 
}

// Lua: read()
// file.read() will read all byte in file
// file.read(10) will read 10 byte from file, or EOF is reached.
// file.read('q') will read until 'q' or EOF is reached. 
static int file_read( lua_State* L )
{
  unsigned need_len = LUAL_BUFFERSIZE;
  int16_t end_char = EOF;
  size_t el;
  if( lua_type( L, 1 ) == LUA_TNUMBER )
  {
    need_len = ( unsigned )luaL_checkinteger( L, 1 );
    if( need_len > LUAL_BUFFERSIZE ){
      need_len = LUAL_BUFFERSIZE;
    }
  }
  else if(lua_isstring(L, 1))
  {
    const char *end = luaL_checklstring( L, 1, &el );
    if(el!=1){
      return luaL_error( L, "wrong arg range" );
    }
    end_char = (int16_t)end[0];
  }

  return file_g_read(L, need_len, end_char);
}

// Lua: readline()
static int file_readline( lua_State* L )
{
  return file_g_read(L, LUAL_BUFFERSIZE, '\n');
}

// Lua: write("string")
static int file_write( lua_State* L )
{
  if((FS_OPEN_OK - 1)==file_fd)
    return luaL_error(L, "open a file first");
  size_t l, rl;
  const char *s = luaL_checklstring(L, 1, &l);
  rl = fs_write(file_fd, s, l);
  if(rl==l)
    lua_pushboolean(L, 1);
  else
    lua_pushnil(L);
  return 1;
}

// Lua: writeline("string")
static int file_writeline( lua_State* L )
{
  if((FS_OPEN_OK - 1)==file_fd)
    return luaL_error(L, "open a file first");
  size_t l, rl;
  const char *s = luaL_checklstring(L, 1, &l);
  rl = fs_write(file_fd, s, l);
  if(rl==l){
    rl = fs_write(file_fd, "\n", 1);
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

// Module function map
#define MIN_OPT_LEVEL 2
#include "lrodefs.h"
const LUA_REG_TYPE file_map[] = 
{
  { LSTRKEY( "list" ), LFUNCVAL( file_list ) },
  { LSTRKEY( "open" ), LFUNCVAL( file_open ) },
  { LSTRKEY( "close" ), LFUNCVAL( file_close ) },
  { LSTRKEY( "write" ), LFUNCVAL( file_write ) },
  { LSTRKEY( "writeline" ), LFUNCVAL( file_writeline ) },
  { LSTRKEY( "read" ), LFUNCVAL( file_read ) },
  { LSTRKEY( "readline" ), LFUNCVAL( file_readline ) },
  { LSTRKEY( "format" ), LFUNCVAL( file_format ) },
#if defined(BUILD_WOFS)
#elif defined(BUILD_SPIFFS)
  { LSTRKEY( "remove" ), LFUNCVAL( file_remove ) },
  { LSTRKEY( "seek" ), LFUNCVAL( file_seek ) },
  { LSTRKEY( "flush" ), LFUNCVAL( file_flush ) },
  // { LSTRKEY( "check" ), LFUNCVAL( file_check ) },
  { LSTRKEY( "rename" ), LFUNCVAL( file_rename ) },
  { LSTRKEY( "fsinfo" ), LFUNCVAL( file_fsinfo ) },
#endif
  
#if LUA_OPTIMIZE_MEMORY > 0

#endif
  { LNILKEY, LNILVAL }
};

LUALIB_API int luaopen_file( lua_State *L )
{
#if LUA_OPTIMIZE_MEMORY > 0
  return 0;
#else // #if LUA_OPTIMIZE_MEMORY > 0
  luaL_register( L, AUXLIB_FILE, file_map );
  // Add constants

  return 1;
#endif // #if LUA_OPTIMIZE_MEMORY > 0  
}
