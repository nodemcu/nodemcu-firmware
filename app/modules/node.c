// Module for interfacing with system

#include "lua.h"
#include "lauxlib.h"

#include "ldo.h"
#include "lfunc.h"
#include "lmem.h"
#include "lobject.h"
#include "lopcodes.h"
#include "lstring.h"
#include "lundump.h"

#include "platform.h"
#include "auxmods.h"
#include "lrotable.h"

#include "c_types.h"
#include "romfs.h"
#include "c_string.h"
#include "driver/uart.h"
//#include "spi_flash.h"
#include "user_interface.h"
#include "flash_api.h"
#include "flash_fs.h"
#include "user_version.h"

#define CPU80MHZ 80
#define CPU160MHZ 160

// Lua: restart()
static int node_restart( lua_State* L )
{
  system_restart();
  return 0;
}

// Lua: dsleep( us, option )
static int node_deepsleep( lua_State* L )
{
  s32 us, option;
  //us = luaL_checkinteger( L, 1 );
  // Set deleep option, skip if nil
  if ( lua_isnumber(L, 2) )
  {
    option = lua_tointeger(L, 2);
    if ( option < 0 || option > 4)
      return luaL_error( L, "wrong arg range" );
    else
      deep_sleep_set_option( option );
  }
  // Set deleep time, skip if nil
  if ( lua_isnumber(L, 1) )
  {
    us = lua_tointeger(L, 1);
    // if ( us <= 0 )
    if ( us < 0 )
      return luaL_error( L, "wrong arg range" );
    else
      system_deep_sleep( us );
  }
  return 0;
}

// Lua: dsleep_set_options
// Combined to dsleep( us, option )
// static int node_deepsleep_setoption( lua_State* L )
// {
//   s32 option;
//   option = luaL_checkinteger( L, 1 );
//   if ( option < 0 || option > 4)
//     return luaL_error( L, "wrong arg range" );
//   else
//    deep_sleep_set_option( option );
//   return 0;
// }
// Lua: info()

static int node_info( lua_State* L )
{
  lua_pushinteger(L, NODE_VERSION_MAJOR);
  lua_pushinteger(L, NODE_VERSION_MINOR);
  lua_pushinteger(L, NODE_VERSION_REVISION);
  lua_pushinteger(L, system_get_chip_id());   // chip id
  lua_pushinteger(L, spi_flash_get_id());     // flash id
#if defined(FLASH_SAFE_API)
  lua_pushinteger(L, flash_safe_get_size_byte() / 1024);  // flash size in KB
#else
  lua_pushinteger(L, flash_rom_get_size_byte() / 1024);  // flash size in KB
#endif // defined(FLASH_SAFE_API)
  lua_pushinteger(L, flash_rom_get_mode());
  lua_pushinteger(L, flash_rom_get_speed());
  return 8;
}

// Lua: chipid()
static int node_chipid( lua_State* L )
{
  uint32_t id = system_get_chip_id();
  lua_pushinteger(L, id);
  return 1;
}
// Lua: readvdd33()
static int node_readvdd33( lua_State* L )
{
  uint32_t vdd33 = readvdd33();
  lua_pushinteger(L, vdd33);
  return 1;
}

// Lua: flashid()
static int node_flashid( lua_State* L )
{
  uint32_t id = spi_flash_get_id();
  lua_pushinteger( L, id );
  return 1;
}

// Lua: flashsize()
static int node_flashsize( lua_State* L )
{
  if (lua_type(L, 1) == LUA_TNUMBER)
  {
    flash_rom_set_size_byte(luaL_checkinteger(L, 1));
  }
#if defined(FLASH_SAFE_API)
  uint32_t sz = flash_safe_get_size_byte();
#else
  uint32_t sz = flash_rom_get_size_byte();
#endif // defined(FLASH_SAFE_API)
  lua_pushinteger( L, sz );
  return 1;
}

// Lua: heap()
static int node_heap( lua_State* L )
{
  uint32_t sz = system_get_free_heap_size();
  lua_pushinteger(L, sz);
  return 1;
}

static lua_State *gL = NULL;

#ifdef DEVKIT_VERSION_0_9
extern int led_high_count;  // this is defined in lua.c
extern int led_low_count;
// Lua: led(low, high)
static int node_led( lua_State* L )
{
  int low, high;
  if ( lua_isnumber(L, 1) )
  {
    low = lua_tointeger(L, 1);
    if ( low < 0 ) {
      return luaL_error( L, "wrong arg type" );
    }
  } else {
    low = LED_LOW_COUNT_DEFAULT; // default to LED_LOW_COUNT_DEFAULT
  }
  if ( lua_isnumber(L, 2) )
  {
    high = lua_tointeger(L, 2);
    if ( high < 0 ) {
      return luaL_error( L, "wrong arg type" );
    }
  } else {
    high = LED_HIGH_COUNT_DEFAULT; // default to LED_HIGH_COUNT_DEFAULT
  }
  led_high_count = (uint32_t)high / READLINE_INTERVAL;
  led_low_count = (uint32_t)low / READLINE_INTERVAL;
  return 0;
}

static int long_key_ref = LUA_NOREF;
static int short_key_ref = LUA_NOREF;

void default_long_press(void *arg) {
  if (led_high_count == 12 && led_low_count == 12) {
    led_low_count = led_high_count = 6;
  } else {
    led_low_count = led_high_count = 12;
  }
  // led_high_count = 1000 / READLINE_INTERVAL;
  // led_low_count = 1000 / READLINE_INTERVAL;
  // NODE_DBG("default_long_press is called. hc: %d, lc: %d\n", led_high_count, led_low_count);
}

void default_short_press(void *arg) {
  system_restart();
}

void key_long_press(void *arg) {
  NODE_DBG("key_long_press is called.\n");
  if (long_key_ref == LUA_NOREF) {
    default_long_press(arg);
    return;
  }
  if (!gL)
    return;
  lua_rawgeti(gL, LUA_REGISTRYINDEX, long_key_ref);
  lua_call(gL, 0, 0);
}

void key_short_press(void *arg) {
  NODE_DBG("key_short_press is called.\n");
  if (short_key_ref == LUA_NOREF) {
    default_short_press(arg);
    return;
  }
  if (!gL)
    return;
  lua_rawgeti(gL, LUA_REGISTRYINDEX, short_key_ref);
  lua_call(gL, 0, 0);
}

// Lua: key(type, function)
static int node_key( lua_State* L )
{
  int *ref = NULL;
  size_t sl;

  const char *str = luaL_checklstring( L, 1, &sl );
  if (str == NULL)
    return luaL_error( L, "wrong arg type" );

  if (sl == 5 && c_strcmp(str, "short") == 0) {
    ref = &short_key_ref;
  } else if (sl == 4 && c_strcmp(str, "long") == 0) {
    ref = &long_key_ref;
  } else {
    ref = &short_key_ref;
  }
  gL = L;
  // luaL_checkanyfunction(L, 2);
  if (lua_type(L, 2) == LUA_TFUNCTION || lua_type(L, 2) == LUA_TLIGHTFUNCTION) {
    lua_pushvalue(L, 2);  // copy argument (func) to the top of stack
    if (*ref != LUA_NOREF)
      luaL_unref(L, LUA_REGISTRYINDEX, *ref);
    *ref = luaL_ref(L, LUA_REGISTRYINDEX);
  } else {    // unref the key press function
    if (*ref != LUA_NOREF)
      luaL_unref(L, LUA_REGISTRYINDEX, *ref);
    *ref = LUA_NOREF;
  }

  return 0;
}
#endif

extern lua_Load gLoad;
extern os_timer_t lua_timer;
extern void dojob(lua_Load *load);
// Lua: input("string")
static int node_input( lua_State* L )
{
  size_t l = 0;
  const char *s = luaL_checklstring(L, 1, &l);
  if (s != NULL && l > 0 && l < LUA_MAXINPUT - 1)
  {
    lua_Load *load = &gLoad;
    if (load->line_position == 0) {
      c_memcpy(load->line, s, l);
      load->line[l + 1] = '\0';
      load->line_position = c_strlen(load->line) + 1;
      load->done = 1;
      NODE_DBG("Get command:\n");
      NODE_DBG(load->line); // buggy here
      NODE_DBG("\nResult(if any):\n");
      os_timer_disarm(&lua_timer);
      os_timer_setfn(&lua_timer, (os_timer_func_t *)dojob, load);
      os_timer_arm(&lua_timer, READLINE_INTERVAL, 0);   // no repeat
    }
  }
  return 0;
}

static int output_redir_ref = LUA_NOREF;
static int serial_debug = 1;
void output_redirect(const char *str) {
  // if(c_strlen(str)>=TX_BUFF_SIZE){
  //   NODE_ERR("output too long.\n");
  //   return;
  // }

  if (output_redir_ref == LUA_NOREF || !gL) {
    uart0_sendStr(str);
    return;
  }

  if (serial_debug != 0) {
    uart0_sendStr(str);
  }

  lua_rawgeti(gL, LUA_REGISTRYINDEX, output_redir_ref);
  lua_pushstring(gL, str);
  lua_call(gL, 1, 0);   // this call back function should never user output.
}

// Lua: output(function(c), debug)
static int node_output( lua_State* L )
{
  gL = L;
  // luaL_checkanyfunction(L, 1);
  if (lua_type(L, 1) == LUA_TFUNCTION || lua_type(L, 1) == LUA_TLIGHTFUNCTION) {
    lua_pushvalue(L, 1);  // copy argument (func) to the top of stack
    if (output_redir_ref != LUA_NOREF)
      luaL_unref(L, LUA_REGISTRYINDEX, output_redir_ref);
    output_redir_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  } else {    // unref the key press function
    if (output_redir_ref != LUA_NOREF)
      luaL_unref(L, LUA_REGISTRYINDEX, output_redir_ref);
    output_redir_ref = LUA_NOREF;
    serial_debug = 1;
    return 0;
  }

  if ( lua_isnumber(L, 2) )
  {
    serial_debug = lua_tointeger(L, 2);
    if (serial_debug != 0)
      serial_debug = 1;
  } else {
    serial_debug = 1; // default to 1
  }

  return 0;
}

static int writer(lua_State* L, const void* p, size_t size, void* u)
{
  UNUSED(L);
  int file_fd = *( (int *)u );
  if ((FS_OPEN_OK - 1) == file_fd)
    return 1;
  NODE_DBG("get fd:%d,size:%d\n", file_fd, size);

  if (size != 0 && (size != fs_write(file_fd, (const char *)p, size)) )
    return 1;
  NODE_DBG("write fd:%d,size:%d\n", file_fd, size);
  return 0;
}

#define toproto(L,i) (clvalue(L->top+(i))->l.p)
// Lua: compile(filename) -- compile lua file into lua bytecode, and save to .lc
static int node_compile( lua_State* L )
{
  Proto* f;
  int file_fd = FS_OPEN_OK - 1;
  size_t len;
  const char *fname = luaL_checklstring( L, 1, &len );
  if ( len > FS_NAME_MAX_LENGTH )
    return luaL_error(L, "filename too long");

  char output[FS_NAME_MAX_LENGTH];
  c_strcpy(output, fname);
  // check here that filename end with ".lua".
  if (len < 4 || (c_strcmp( output + len - 4, ".lua") != 0) )
    return luaL_error(L, "not a .lua file");

  output[c_strlen(output) - 2] = 'c';
  output[c_strlen(output) - 1] = '\0';
  NODE_DBG(output);
  NODE_DBG("\n");
  if (luaL_loadfsfile(L, fname) != 0) {
    return luaL_error(L, lua_tostring(L, -1));
  }

  f = toproto(L, -1);

  int stripping = 1;      /* strip debug information? */

  file_fd = fs_open(output, fs_mode2flag("w+"));
  if (file_fd < FS_OPEN_OK)
  {
    return luaL_error(L, "cannot open/write to file");
  }

  lua_lock(L);
  int result = luaU_dump(L, f, writer, &file_fd, stripping);
  lua_unlock(L);

  fs_flush(file_fd);
  fs_close(file_fd);
  file_fd = FS_OPEN_OK - 1;

  if (result == LUA_ERR_CC_INTOVERFLOW) {
    return luaL_error(L, "value too big or small for target integer type");
  }
  if (result == LUA_ERR_CC_NOTINTEGER) {
    return luaL_error(L, "target lua_Number is integral but fractional value found");
  }

  return 0;
}

// Lua: setcpufreq(mhz)
// mhz is either CPU80MHZ od CPU160MHZ
static int node_setcpufreq(lua_State* L)
{
  // http://www.esp8266.com/viewtopic.php?f=21&t=1369
  uint32_t new_freq = luaL_checkinteger(L, 1);
  if (new_freq == CPU160MHZ){
    REG_SET_BIT(0x3ff00014, BIT(0));
    os_update_cpu_frequency(CPU160MHZ);
  } else {
    REG_CLR_BIT(0x3ff00014,  BIT(0));
    os_update_cpu_frequency(CPU80MHZ);
  }
  new_freq = ets_get_cpu_frequency();
  lua_pushinteger(L, new_freq);
  return 1;
}

// Module function map
#define MIN_OPT_LEVEL 2
#include "lrodefs.h"
const LUA_REG_TYPE node_map[] =
{
  { LSTRKEY( "restart" ), LFUNCVAL( node_restart ) },
  { LSTRKEY( "dsleep" ), LFUNCVAL( node_deepsleep ) },
  { LSTRKEY( "info" ), LFUNCVAL( node_info ) },
  { LSTRKEY( "chipid" ), LFUNCVAL( node_chipid ) },
  { LSTRKEY( "flashid" ), LFUNCVAL( node_flashid ) },
  { LSTRKEY( "flashsize" ), LFUNCVAL( node_flashsize) },
  { LSTRKEY( "heap" ), LFUNCVAL( node_heap ) },
#ifdef DEVKIT_VERSION_0_9
  { LSTRKEY( "key" ), LFUNCVAL( node_key ) },
  { LSTRKEY( "led" ), LFUNCVAL( node_led ) },
#endif
  { LSTRKEY( "input" ), LFUNCVAL( node_input ) },
  { LSTRKEY( "output" ), LFUNCVAL( node_output ) },
  { LSTRKEY( "readvdd33" ), LFUNCVAL( node_readvdd33) },
  { LSTRKEY( "compile" ), LFUNCVAL( node_compile) },
  { LSTRKEY( "CPU80MHZ" ), LNUMVAL( CPU80MHZ ) },
  { LSTRKEY( "CPU160MHZ" ), LNUMVAL( CPU160MHZ ) },
  { LSTRKEY( "setcpufreq" ), LFUNCVAL( node_setcpufreq) },
// Combined to dsleep(us, option)
// { LSTRKEY( "dsleepsetoption" ), LFUNCVAL( node_deepsleep_setoption) },
#if LUA_OPTIMIZE_MEMORY > 0

#endif
  { LNILKEY, LNILVAL }
};

LUALIB_API int luaopen_node( lua_State *L )
{
#if LUA_OPTIMIZE_MEMORY > 0
  return 0;
#else // #if LUA_OPTIMIZE_MEMORY > 0
  luaL_register( L, AUXLIB_NODE, node_map );
  // Add constants

  return 1;
#endif // #if LUA_OPTIMIZE_MEMORY > 0
}
