// Module for interfacing with serial

#include "module.h"
#include "lauxlib.h"
#include "platform.h"

#include "c_types.h"
#include "c_string.h"
#include "rom.h"

static int uart_receive_rf = LUA_NOREF;
bool run_input = true;
bool uart_on_data_cb(const char *buf, size_t len){
  if(!buf || len==0)
    return false;
  if(uart_receive_rf == LUA_NOREF)
    return false;
  lua_State *L = lua_getstate();
  if(!L)
    return false;
  lua_rawgeti(L, LUA_REGISTRYINDEX, uart_receive_rf);
  lua_pushlstring(L, buf, len);
  lua_call(L, 1, 0);
  return !run_input;
}

uint16_t need_len = 0;
int16_t end_char = -1;
// Lua: uart.on("method", [number/char], function, [run_input])
static int l_uart_on( lua_State* L )
{
  size_t sl, el;
  int32_t run = 1;
  uint8_t stack = 1;
  const char *method = luaL_checklstring( L, stack, &sl );
  stack++;
  if (method == NULL)
    return luaL_error( L, "wrong arg type" );

  if( lua_type( L, stack ) == LUA_TNUMBER )
  {
    need_len = ( uint16_t )luaL_checkinteger( L, stack );
    stack++;
    end_char = -1;
    if( need_len > 255 ){
      need_len = 255;
      return luaL_error( L, "wrong arg range" );
    }
  }
  else if(lua_isstring(L, stack))
  {
    const char *end = luaL_checklstring( L, stack, &el );
    stack++;
    if(el!=1){
      return luaL_error( L, "wrong arg range" );
    }
    end_char = (int16_t)end[0];
    need_len = 0;
  }

  // luaL_checkanyfunction(L, stack);
  if (lua_type(L, stack) == LUA_TFUNCTION || lua_type(L, stack) == LUA_TLIGHTFUNCTION){
    if ( lua_isnumber(L, stack+1) ){
      run = lua_tointeger(L, stack+1);
    }
    lua_pushvalue(L, stack);  // copy argument (func) to the top of stack
  } else {
    lua_pushnil(L);
  }
  if(sl == 4 && c_strcmp(method, "data") == 0){
    run_input = true;
    if(uart_receive_rf != LUA_NOREF){
      luaL_unref(L, LUA_REGISTRYINDEX, uart_receive_rf);
      uart_receive_rf = LUA_NOREF;
    }
    if(!lua_isnil(L, -1)){
      uart_receive_rf = luaL_ref(L, LUA_REGISTRYINDEX);
      if(run==0)
        run_input = false;
    } else {
      lua_pop(L, 1);
    }
  }else{
    lua_pop(L, 1);
    return luaL_error( L, "method not supported" );
  }
  return 0; 
}

bool uart0_echo = true;
// Lua: actualbaud = setup( id, baud, databits, parity, stopbits, echo )
static int l_uart_setup( lua_State* L )
{
  uint32_t id, databits, parity, stopbits, echo = 1;
  uint32_t baud, res;
  
  id = luaL_checkinteger( L, 1 );
  MOD_CHECK_ID( uart, id );

  baud = luaL_checkinteger( L, 2 );
  databits = luaL_checkinteger( L, 3 );
  parity = luaL_checkinteger( L, 4 );
  stopbits = luaL_checkinteger( L, 5 );
  if(lua_isnumber(L,6)){
    echo = lua_tointeger(L,6);
    if(echo!=0)
      uart0_echo = true;
    else
      uart0_echo = false;
  }

  res = platform_uart_setup( id, baud, databits, parity, stopbits );
  lua_pushinteger( L, res );
  return 1;
}

// uart.getconfig(id)
static int l_uart_getconfig( lua_State* L )
{
  uint32_t id, databits, parity, stopbits;
  uint32_t baud;
  
  id = luaL_checkinteger( L, 1 );
  MOD_CHECK_ID( uart, id );

  platform_uart_get_config(id, &baud, &databits, &parity, &stopbits);

  lua_pushinteger(L, baud);
  lua_pushinteger(L, databits);
  lua_pushinteger(L, parity);
  lua_pushinteger(L, stopbits);
  return 4;
}

// Lua: alt( set )
static int l_uart_alt( lua_State* L )
{
  unsigned set;
  
  set = luaL_checkinteger( L, 1 );

  platform_uart_alt( set );
  return 0;
}

// Lua: write( id, string1, [string2], ..., [stringn] )
static int l_uart_write( lua_State* L )
{
  int id;
  const char* buf;
  size_t len, i;
  int total = lua_gettop( L ), s;
  
  id = luaL_checkinteger( L, 1 );
  MOD_CHECK_ID( uart, id );
  for( s = 2; s <= total; s ++ )
  {
    if( lua_type( L, s ) == LUA_TNUMBER )
    {
      len = lua_tointeger( L, s );
      if( len > 255 )
        return luaL_error( L, "invalid number" );
      platform_uart_send( id, ( u8 )len );
    }
    else
    {
      luaL_checktype( L, s, LUA_TSTRING );
      buf = lua_tolstring( L, s, &len );
      for( i = 0; i < len; i ++ )
        platform_uart_send( id, buf[ i ] );
    }
  }
  return 0;
}

// Module function map
static const LUA_REG_TYPE uart_map[] =  {
  { LSTRKEY( "setup" ), LFUNCVAL( l_uart_setup ) },
  { LSTRKEY( "getconfig" ), LFUNCVAL( l_uart_getconfig ) },
  { LSTRKEY( "write" ), LFUNCVAL( l_uart_write ) },
  { LSTRKEY( "on" ),    LFUNCVAL( l_uart_on ) },
  { LSTRKEY( "alt" ),   LFUNCVAL( l_uart_alt ) },
  { LSTRKEY( "STOPBITS_1" ),   LNUMVAL( PLATFORM_UART_STOPBITS_1 ) },
  { LSTRKEY( "STOPBITS_1_5" ), LNUMVAL( PLATFORM_UART_STOPBITS_1_5 ) },
  { LSTRKEY( "STOPBITS_2" ),   LNUMVAL( PLATFORM_UART_STOPBITS_2 ) },
  { LSTRKEY( "PARITY_NONE" ),  LNUMVAL( PLATFORM_UART_PARITY_NONE ) },
  { LSTRKEY( "PARITY_EVEN" ),  LNUMVAL( PLATFORM_UART_PARITY_EVEN ) },
  { LSTRKEY( "PARITY_ODD" ),   LNUMVAL( PLATFORM_UART_PARITY_ODD ) },
  { LNILKEY, LNILVAL }
};

NODEMCU_MODULE(UART, "uart", uart_map, NULL);
