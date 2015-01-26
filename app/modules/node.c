// Module for interfacing with system

//#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
#include "platform.h"
#include "auxmods.h"
#include "lrotable.h"

#include "c_types.h"
#include "romfs.h"
#include "c_string.h"
#include "driver/uart.h"
#include "spi_flash.h"
#include "flash_api.h"

// Lua: restart()
static int node_restart( lua_State* L )
{
  system_restart();
  return 0;  
}

// Lua: dsleep( us )
static int node_deepsleep( lua_State* L )
{
  s32 us;
  us = luaL_checkinteger( L, 1 );
  if ( us <= 0 )
    return luaL_error( L, "wrong arg range" );
  system_deep_sleep( us );
  return 0;  
}

// Lua: info()
static int node_info( lua_State* L )
{
  lua_pushinteger(L, NODE_VERSION_MAJOR);
  lua_pushinteger(L, NODE_VERSION_MINOR);
  lua_pushinteger(L, NODE_VERSION_REVISION);
  lua_pushinteger(L, system_get_chip_id());   // chip id
  lua_pushinteger(L, spi_flash_get_id());     // flash id
  lua_pushinteger(L, flash_get_size_byte() / 1024);  // flash size in KB
  lua_pushinteger(L, flash_get_mode());
  lua_pushinteger(L, flash_get_speed());
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
  //uint32_t sz = 0;
  //if(lua_type(L, 1) == LUA_TNUMBER)
  //{
  //  sz = luaL_checkinteger(L, 1);
  //  if(sz > 0)
  //  {
  //    flash_set_size_byte(sz);
  //  }
  //}
  uint32_t sz = flash_get_size_byte();
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

extern int led_high_count;  // this is defined in lua.c
extern int led_low_count;
// Lua: led(low, high)
static int node_led( lua_State* L )
{
  int low, high;
  if ( lua_isnumber(L, 1) )
  {
    low = lua_tointeger(L, 1);
    if ( low < 0 ){
      return luaL_error( L, "wrong arg type" );
    }
  } else {
    low = LED_LOW_COUNT_DEFAULT; // default to LED_LOW_COUNT_DEFAULT
  }
  if ( lua_isnumber(L, 2) )
  {
    high = lua_tointeger(L, 2);
    if ( high < 0 ){
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
static lua_State *gL = NULL;

void default_long_press(void *arg){
  if(led_high_count == 12 && led_low_count == 12){
    led_low_count = led_high_count = 6;
  } else {
    led_low_count = led_high_count = 12;
  }
  // led_high_count = 1000 / READLINE_INTERVAL;
  // led_low_count = 1000 / READLINE_INTERVAL;
  // NODE_DBG("default_long_press is called. hc: %d, lc: %d\n", led_high_count, led_low_count);
}

void default_short_press(void *arg){
  system_restart();
}

void key_long_press(void *arg){
  NODE_DBG("key_long_press is called.\n");
  if(long_key_ref == LUA_NOREF){
    default_long_press(arg);
    return;
  }
  if(!gL)
    return;
  lua_rawgeti(gL, LUA_REGISTRYINDEX, long_key_ref);
  lua_call(gL, 0, 0);
}

void key_short_press(void *arg){
  NODE_DBG("key_short_press is called.\n");
  if(short_key_ref == LUA_NOREF){
    default_short_press(arg);
    return;
  }
  if(!gL)
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

  if(sl == 5 && c_strcmp(str, "short") == 0){
    ref = &short_key_ref;
  }else if(sl == 4 && c_strcmp(str, "long") == 0){
    ref = &long_key_ref;
  }else{
    ref = &short_key_ref;
  }
  gL = L;
  // luaL_checkanyfunction(L, 2);
  if (lua_type(L, 2) == LUA_TFUNCTION || lua_type(L, 2) == LUA_TLIGHTFUNCTION){
    lua_pushvalue(L, 2);  // copy argument (func) to the top of stack
    if(*ref != LUA_NOREF)
      luaL_unref(L, LUA_REGISTRYINDEX, *ref);
    *ref = luaL_ref(L, LUA_REGISTRYINDEX);
  } else {    // unref the key press function
    if(*ref != LUA_NOREF)
      luaL_unref(L, LUA_REGISTRYINDEX, *ref);
    *ref = LUA_NOREF; 
  }

  return 0;  
}

extern lua_Load gLoad;
extern os_timer_t lua_timer;
extern void dojob(lua_Load *load);
// Lua: input("string")
static int node_input( lua_State* L )
{
  size_t l=0;
  const char *s = luaL_checklstring(L, 1, &l);
  if (s != NULL && l > 0 && l < LUA_MAXINPUT - 1)
  {
    lua_Load *load = &gLoad;
    if(load->line_position == 0){
      c_memcpy(load->line, s, l);
      load->line[l+1] = '\0';
      load->line_position = c_strlen(load->line)+1;
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
void output_redirect(const char *str){
  // if(c_strlen(str)>=TX_BUFF_SIZE){
  //   NODE_ERR("output too long.\n");
  //   return;
  // }

  if(output_redir_ref == LUA_NOREF || !gL){
    uart0_sendStr(str);
    return;
  }

  if(serial_debug!=0){
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
  if (lua_type(L, 1) == LUA_TFUNCTION || lua_type(L, 1) == LUA_TLIGHTFUNCTION){
    lua_pushvalue(L, 1);  // copy argument (func) to the top of stack
    if(output_redir_ref != LUA_NOREF)
      luaL_unref(L, LUA_REGISTRYINDEX, output_redir_ref);
    output_redir_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  } else {    // unref the key press function
    if(output_redir_ref != LUA_NOREF)
      luaL_unref(L, LUA_REGISTRYINDEX, output_redir_ref);
    output_redir_ref = LUA_NOREF; 
    serial_debug = 1;
    return 0;
  }

  if ( lua_isnumber(L, 2) )
  {
    serial_debug = lua_tointeger(L, 2);
    if(serial_debug!=0)
      serial_debug = 1;
  } else {
    serial_debug = 1; // default to 1
  }

  return 0; 
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
  { LSTRKEY( "key" ), LFUNCVAL( node_key ) },
  { LSTRKEY( "led" ), LFUNCVAL( node_led ) },
  { LSTRKEY( "input" ), LFUNCVAL( node_input ) },
  { LSTRKEY( "output" ), LFUNCVAL( node_output ) },
  { LSTRKEY( "readvdd33" ), LFUNCVAL( node_readvdd33) },
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
