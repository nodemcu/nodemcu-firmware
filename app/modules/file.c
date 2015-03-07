/// @file file.c Define file module for interfacing with file system

/// @cond
/// This should not be documented./// @cond
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
#include "platform.h"
#include "auxmods.h"
#include "lrotable.h"

#include "c_types.h"
#include "flash_fs.h"
#include "c_string.h"

#define INVALID_FD (FS_OPEN_OK - 1)
static volatile int file_fd = INVALID_FD;
/// @endcond

/// Implements the lua method file.close.
/// It closes the current file opened in file_open().
// Lua: close()
static int file_close( lua_State* L )
{
  if (INVALID_FD != file_fd){
    fs_close(file_fd);
    file_fd = INVALID_FD;
  }
  return 0;  
}

/// Implements the lua method file.open.
/// Open a file.
/// @param filename the file name.
/// @param  mode Optional. The open mode, default is "r".
/// @throws error if filename is too long.
/// @retval false open file failed.
/// @retval true open file succeeded.
/// @attention You can only open one file at one time.
// Lua: open(filename, mode)
static int file_open( lua_State* L )
{
  size_t len;
  file_close();

  const char *fname = luaL_checklstring( L, 1, &len );
  if( len > FS_NAME_MAX_LENGTH )
    return luaL_error(L, "filename too long");
  const char *mode = luaL_optstring(L, 2, "r");

  file_fd = fs_open(fname, fs_mode2flag(mode));

  if(file_fd < FS_OPEN_OK){
    file_fd = INVALID_FD;
    lua_pushboolean(L, 0);
  } else {
    lua_pushboolean(L, 1);
  }
  return 1; 
}

/// Implements the lua method file.format.
/// It closes the current file and formats the file system.
// Lua: format()
static int file_format( lua_State* L )
{
  size_t len;
  file_close(L);
  if ( fs_format() != 0 )
  {
    NODE_ERR( "\ni*** ERROR ***: unable to format. FS might be compromised.\n" );
    NODE_ERR( "It is advised to re-flash the NodeMCU image.\n" );
  }
  else {
    NODE_ERR( "format done.\n" );
  }
  return 0; 
}

/// Implements the lua method file.seek.
/// Seeks current file to position basded on mode.
/// @param The mode. Can be one of: set, cur, end.
/// @param The offest.
/// @throws error if file not opened.
/// @retval false seek failed.
/// @retval true seek succeeded.
// Lua: seek(mode, offest)
static int file_seek (lua_State *L) 
{
  static const int mode[] = {FS_SEEK_SET, FS_SEEK_CUR, FS_SEEK_END};
  static const char *const modenames[] = {"set", "cur", "end", NULL};
  if(INVALID_FD == file_fd)
    return luaL_error(L, "open a file first");
  int op = luaL_checkoption(L, 1, "cur", modenames);
  long offset = luaL_optlong(L, 2, 0);
  op = fs_seek(file_fd, offset, mode[op]);
  if (op)
    lua_pushboolean(L, 0);  /* error */
  else
    lua_pushboolean(L, 1);
  return 1;
}

#if defined(BUILD_WOFS)
/// Implements the lua method file.list.
/// It lists the files in the file system.
/// @return filenames a table consist of file names and their lengths.
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

/// Implements the lua method file.list.
/// It lists the files in the file system.
/// @return filenames a table consist of file names and their lengths.
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

/// Implements the lua method file.tell.
/// Tell the current position of current file.
/// @throws error if file not opened.
/// @return the currentposition of file.
static int file_tell (lua_State* L) {
  if (INVALID_FD == file_fd) {
    return luaL_error(L, "open a file first");
  }
  lua_pushinteger(L, fs_tell(file_fd));
  return 1;
}


/// Implements the lua method file.remove.
/// Close current file and remove the file from file system.
/// @param filename
/// @throws error if filename is too long.
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

/// Implements the lua method file.flush.
/// Flush the buffer to file.
/// @return the result of flush.
/// @throws error if file not opened.
/// @retval false flush failed.
/// @retval true flush succeeded.
// Lua: flush()
static int file_flush( lua_State* L )
{
  if(INVALID_FD==file_fd)
    return luaL_error(L, "open a file first");
  if(fs_flush(file_fd) == SPIFFS_OK)
    lua_pushboolean(L, 1);
  else
    lua_pushboolean(L, 0);
  return 1;
}


/// Implements the lua method file.check.
/// Check the file system.
/// @return true check succeeded.
/// @return false check failed.
static int file_check( lua_State* L )
{
  file_close(L);
  if (fs_check() == SPIFFS_OK)
    lua_pushboolean(L, 1);
  else
    lua_pushboolean(L, 0);
  return 1;
}

/// Implements the lua method file.rename.
/// Rename file after closing current file.
/// @param oldname old file name.
/// @param newname new file name.
/// @throws error if either file name is too long.
/// @retval true if rename succeeded.
/// @retval false if rename failed.

// Lua: rename("oldname", "newname")
static int file_rename( lua_State* L )
{
  size_t len;
  file_close(L);

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

#endif

/// @cond
/// This should not be documented.

// g_read()
static int file_g_read( lua_State* L, int n, int16_t end_char )
{
  if(n< 0 || n>LUAL_BUFFERSIZE) 
    n = LUAL_BUFFERSIZE;
  if(end_char < 0 || end_char >255)
    end_char = EOF;
  int ec = (int)end_char;
  
  luaL_Buffer b;
  if(INVALID_FD==file_fd)
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

/// @endcond

// Lua: read()
/// Implements the lua method file.read.
/// file.read() will read all bytes in file
/// file.read(10) will read 10 byte from file, or EOF is reached.
/// file.read('q') will read until 'q' or EOF is reached.
/// @return the content.
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

/// Implements the lua method file.readline.
/// Reads a line until '\n'.
/// @return the line.
// Lua: readline()
static int file_readline( lua_State* L )
{
  return file_g_read(L, LUAL_BUFFERSIZE, '\n');
}

/// Implements the lua method file.write.
/// Write a string into file.
/// @param string
/// @throws error if file not opened
/// @retval true write succeeded.
/// @retval false write failed.
// Lua: write("string")
static int file_write( lua_State* L )
{
  if(INVALID_FD==file_fd)
    return luaL_error(L, "open a file first");
  size_t l, rl;
  const char *s = luaL_checklstring(L, 1, &l);
  rl = fs_write(file_fd, s, l);
  if(rl==l)
    lua_pushboolean(L, 1);
  else
    lua_pushboolean(L, 0);
  return 1;
}

/// Implements the lua method file.writeline.
/// Write a string and append line feed(LF, '\\n') into file.
/// @param string
/// @throws error if file not opened
/// @retval true write succeeded.
/// @retval nil write failed.
// Lua: writeline("string")
static int file_writeline( lua_State* L )
{
  if(INVALID_FD==file_fd)
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

/// @cond
/// This should not be documented.
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
  { LSTRKEY( "list" ), LFUNCVAL( file_list ) },
#elif defined(BUILD_SPIFFS)
  { LSTRKEY( "list" ), LFUNCVAL( file_list ) },
  { LSTRKEY( "remove" ), LFUNCVAL( file_remove ) },
  { LSTRKEY( "seek" ), LFUNCVAL( file_seek ) },
  { LSTRKEY( "tell" ), LFUNCVAL( file_tell ) },
  { LSTRKEY( "flush" ), LFUNCVAL( file_flush ) },
  { LSTRKEY( "check" ), LFUNCVAL( file_check ) },
  { LSTRKEY( "rename" ), LFUNCVAL( file_rename ) },
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
  luaL_register( L, AUXLIB_NODE, file_map );
  // Add constants

  return 1;
#endif // #if LUA_OPTIMIZE_MEMORY > 0  
}
/// @endcond
