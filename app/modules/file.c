// Module for interfacing with file system

#include "module.h"
#include "lauxlib.h"
#include "platform.h"

#include "c_types.h"
#include "vfs.h"
#include "c_string.h"

static int file_fd = 0;
static int rtc_cb_ref = LUA_NOREF;


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

  tm->year = luaL_optint( L, ++idx, 2016 );
  tm->mon  = luaL_optint( L, ++idx, 6 );
  tm->day  = luaL_optint( L, ++idx, 21 );
  tm->hour = luaL_optint( L, ++idx, 0 );
  tm->min  = luaL_optint( L, ++idx, 0 );
  tm->sec  = luaL_optint( L, ++idx, 0 );

  // remove items from stack
  lua_pop( L, 6 );
}

static sint32_t file_rtc_cb( vfs_time *tm )
{
  sint32_t res = VFS_RES_ERR;

  if (rtc_cb_ref != LUA_NOREF) {
    lua_State *L = lua_getstate();

    lua_rawgeti( L, LUA_REGISTRYINDEX, rtc_cb_ref );
    lua_call( L, 0, 1 );

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

    if ((lua_type(L, 2) == LUA_TFUNCTION) ||
        (lua_type(L, 2) == LUA_TLIGHTFUNCTION)) {
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
  if(file_fd){
    vfs_close(file_fd);
    file_fd = 0;
  }
  return 0;  
}

// Lua: format()
static int file_format( lua_State* L )
{
  size_t len;
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

// Lua: open(filename, mode)
static int file_open( lua_State* L )
{
  size_t len;
  if(file_fd){
    vfs_close(file_fd);
    file_fd = 0;
  }

  const char *fname = luaL_checklstring( L, 1, &len );
  const char *basename = vfs_basename( fname );
  luaL_argcheck(L, c_strlen(basename) <= FS_OBJ_NAME_LEN && c_strlen(fname) == len, 1, "filename invalid");

  const char *mode = luaL_optstring(L, 2, "r");

  file_fd = vfs_open(fname, mode);

  if(!file_fd){
    lua_pushnil(L);
  } else {
    lua_pushboolean(L, 1);
  }
  return 1; 
}

// Lua: list()
static int file_list( lua_State* L )
{
  vfs_dir  *dir;
  vfs_item *item;

  if (dir = vfs_opendir("")) {
    lua_newtable( L );
    while (item = vfs_readdir(dir)) {
      lua_pushinteger(L, vfs_item_size(item));
      lua_setfield(L, -2, vfs_item_name(item));
      vfs_closeitem(item);
    }
    vfs_closedir(dir);
    return 1;
  }
  return 0;
}

static int file_seek (lua_State *L) 
{
  static const int mode[] = {VFS_SEEK_SET, VFS_SEEK_CUR, VFS_SEEK_END};
  static const char *const modenames[] = {"set", "cur", "end", NULL};
  if(!file_fd)
    return luaL_error(L, "open a file first");
  int op = luaL_checkoption(L, 1, "cur", modenames);
  long offset = luaL_optlong(L, 2, 0);
  op = vfs_lseek(file_fd, offset, mode[op]);
  if (op < 0)
    lua_pushnil(L);  /* error */
  else
    lua_pushinteger(L, vfs_tell(file_fd));
  return 1;
}

// Lua: exists(filename)
static int file_exists( lua_State* L )
{
  size_t len;
  const char *fname = luaL_checklstring( L, 1, &len );    
  const char *basename = vfs_basename( fname );
  luaL_argcheck(L, c_strlen(basename) <= FS_OBJ_NAME_LEN && c_strlen(fname) == len, 1, "filename invalid");

  vfs_item *stat = vfs_stat((char *)fname);

  lua_pushboolean(L, stat ? 1 : 0);

  if (stat) vfs_closeitem(stat);

  return 1;
}

// Lua: remove(filename)
static int file_remove( lua_State* L )
{
  size_t len;
  const char *fname = luaL_checklstring( L, 1, &len );    
  const char *basename = vfs_basename( fname );
  luaL_argcheck(L, c_strlen(basename) <= FS_OBJ_NAME_LEN && c_strlen(fname) == len, 1, "filename invalid");
  file_close(L);
  vfs_remove((char *)fname);
  return 0;
}

// Lua: flush()
static int file_flush( lua_State* L )
{
  if(!file_fd)
    return luaL_error(L, "open a file first");
  if(vfs_flush(file_fd) == 0)
    lua_pushboolean(L, 1);
  else
    lua_pushnil(L);
  return 1;
}

// Lua: rename("oldname", "newname")
static int file_rename( lua_State* L )
{
  size_t len;
  if(file_fd){
    vfs_close(file_fd);
    file_fd = 0;
  }

  const char *oldname = luaL_checklstring( L, 1, &len );
  const char *basename = vfs_basename( oldname );
  luaL_argcheck(L, c_strlen(basename) <= FS_OBJ_NAME_LEN && c_strlen(oldname) == len, 1, "filename invalid");
  
  const char *newname = luaL_checklstring( L, 2, &len );  
  basename = vfs_basename( newname );
  luaL_argcheck(L, c_strlen(basename) <= FS_OBJ_NAME_LEN && c_strlen(newname) == len, 2, "filename invalid");

  if(0 <= vfs_rename( oldname, newname )){
    lua_pushboolean(L, 1);
  } else {
    lua_pushboolean(L, 0);
  }
  return 1;
}

// g_read()
static int file_g_read( lua_State* L, int n, int16_t end_char )
{
  if(n <= 0 || n > LUAL_BUFFERSIZE)
    n = LUAL_BUFFERSIZE;
  if(end_char < 0 || end_char >255)
    end_char = EOF;
  
  luaL_Buffer b;
  if(!file_fd)
    return luaL_error(L, "open a file first");

  luaL_buffinit(L, &b);
  char *p = luaL_prepbuffer(&b);
  int i;

  n = vfs_read(file_fd, p, n);
  for (i = 0; i < n; ++i)
    if (p[i] == end_char)
    {
      ++i;
      break;
    }

  if(i==0){
    luaL_pushresult(&b);  /* close buffer */
    return (lua_objlen(L, -1) > 0);  /* check whether read something */
  }

  vfs_lseek(file_fd, -(n - i), VFS_SEEK_CUR);
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
  if(!file_fd)
    return luaL_error(L, "open a file first");
  size_t l, rl;
  const char *s = luaL_checklstring(L, 1, &l);
  rl = vfs_write(file_fd, s, l);
  if(rl==l)
    lua_pushboolean(L, 1);
  else
    lua_pushnil(L);
  return 1;
}

// Lua: writeline("string")
static int file_writeline( lua_State* L )
{
  if(!file_fd)
    return luaL_error(L, "open a file first");
  size_t l, rl;
  const char *s = luaL_checklstring(L, 1, &l);
  rl = vfs_write(file_fd, s, l);
  if(rl==l){
    rl = vfs_write(file_fd, "\n", 1);
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

typedef struct {
  vfs_vol *vol;
} volume_type;

// Lua: vol = file.mount("/SD0")
static int file_mount( lua_State *L )
{
  const char *ldrv = luaL_checkstring( L, 1 );
  int num = luaL_optint( L, 2, -1 );
  volume_type *vol = (volume_type *)lua_newuserdata( L, sizeof( volume_type ) );

  if (vol->vol = vfs_mount( ldrv, num )) {
    /* set its metatable */
    luaL_getmetatable(L, "vfs.vol");
    lua_setmetatable(L, -2);
    return 1;
  } else {
    // remove created userdata
    lua_pop( L, 1 );
    return 0;
  }
}

// Lua: success = file.chdir("/SD0/")
static int file_chdir( lua_State *L )
{
  const char *path = luaL_checkstring( L, 1 );

  lua_pushboolean( L, 0 <= vfs_chdir( path ) );
  return 1;
}

static int file_vol_umount( lua_State *L )
{
  volume_type *vol = luaL_checkudata( L, 1, "file.vol" );
  luaL_argcheck( L, vol, 1, "volume expected" );

  lua_pushboolean( L, 0 <= vfs_umount( vol->vol ) );

  // invalidate vfs descriptor, it has been free'd anyway
  vol->vol = NULL;
  return 1;
}


static const LUA_REG_TYPE file_vol_map[] =
{
  { LSTRKEY( "umount" ),   LFUNCVAL( file_vol_umount )},
  //{ LSTRKEY( "getfree" ),  LFUNCVAL( file_vol_getfree )},
  //{ LSTRKEY( "getlabel" ), LFUNCVAL( file_vol_getlabel )},
  //{ LSTRKEY( "__gc" ),     LFUNCVAL( file_vol_free ) },
  { LSTRKEY( "__index" ),  LROVAL( file_vol_map ) },
  { LNILKEY, LNILVAL }
};

// Module function map
static const LUA_REG_TYPE file_map[] = {
  { LSTRKEY( "list" ),      LFUNCVAL( file_list ) },
  { LSTRKEY( "open" ),      LFUNCVAL( file_open ) },
  { LSTRKEY( "close" ),     LFUNCVAL( file_close ) },
  { LSTRKEY( "write" ),     LFUNCVAL( file_write ) },
  { LSTRKEY( "writeline" ), LFUNCVAL( file_writeline ) },
  { LSTRKEY( "read" ),      LFUNCVAL( file_read ) },
  { LSTRKEY( "readline" ),  LFUNCVAL( file_readline ) },
#ifdef BUILD_SPIFFS
  { LSTRKEY( "format" ),    LFUNCVAL( file_format ) },
  { LSTRKEY( "fscfg" ),     LFUNCVAL( file_fscfg ) },
#endif
  { LSTRKEY( "remove" ),    LFUNCVAL( file_remove ) },
  { LSTRKEY( "seek" ),      LFUNCVAL( file_seek ) },
  { LSTRKEY( "flush" ),     LFUNCVAL( file_flush ) },
  { LSTRKEY( "rename" ),    LFUNCVAL( file_rename ) },
  { LSTRKEY( "exists" ),    LFUNCVAL( file_exists ) },  
  { LSTRKEY( "fsinfo" ),    LFUNCVAL( file_fsinfo ) },
  { LSTRKEY( "on" ),        LFUNCVAL( file_on ) },
#ifdef BUILD_FATFS
  { LSTRKEY( "mount" ),     LFUNCVAL( file_mount ) },
  { LSTRKEY( "chdir" ),     LFUNCVAL( file_chdir ) },
#endif
  { LNILKEY, LNILVAL }
};

int luaopen_file( lua_State *L ) {
  luaL_rometatable( L, "file.vol",  (void *)file_vol_map );
  return 0;
}

NODEMCU_MODULE(FILE, "file", file_map, luaopen_file);
