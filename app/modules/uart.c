// Module for interfacing with serial

#include "module.h"
#include "lauxlib.h"
#include "platform.h"

#include <stdint.h>
#include <string.h>
#include "rom.h"
#include "driver/input.h"

static int uart_receive_rf = LUA_NOREF;

void uart_on_data_cb(const char *buf, size_t len){
  lua_State *L = lua_getstate();
  lua_rawgeti(L, LUA_REGISTRYINDEX, uart_receive_rf);
  lua_pushlstring(L, buf, len);
  luaL_pcallx(L, 1, 0);
}

// Lua: uart.on("method", [number/char], function, [run_input])
static int l_uart_on( lua_State* L )
{
  size_t el;
  int stack = 2, data_len = -1;
  char end_char = 0;
  const char *method = lua_tostring( L, 1);
  bool run_input = true;
  luaL_argcheck(L, method && !strcmp(method, "data"), 1, "method not supported");

  if (lua_type( L, stack ) == LUA_TNUMBER) {
    data_len = luaL_checkinteger( L, stack );
    luaL_argcheck(L, data_len >= 0 && data_len < LUA_MAXINPUT, stack, "wrong arg range");
    stack++;
  } else if (lua_isstring(L, stack)) {
    const char *end = luaL_checklstring( L, stack, &el );
    end_char = end[0];
    stack++;
    if(el!=1) {
      return luaL_error( L, "wrong arg range" );
    }
  }

  if (lua_isfunction(L, stack)) {
    if (lua_isnumber(L, stack+1) && lua_tointeger(L, stack+1) == 0) {
      run_input = false;
    }
    lua_pushvalue(L, stack);
    luaL_unref(L, LUA_REGISTRYINDEX, uart_receive_rf);
    uart_receive_rf = luaL_ref(L, LUA_REGISTRYINDEX);
  } else {
    luaL_unref(L, LUA_REGISTRYINDEX, uart_receive_rf);
    uart_receive_rf = LUA_NOREF;
  }

  if (uart_receive_rf == LUA_NOREF) {
    input_setup_receive(NULL, 0, 0, 1);
  } else
    input_setup_receive(uart_on_data_cb, data_len, end_char, run_input);
  return 0;
}

bool uart0_echo = true;
// Lua: actualbaud = setup( id, baud, databits, parity, stopbits, echo )
static int l_uart_setup( lua_State* L )
{
  uint32_t id, databits, parity, stopbits;
  uint32_t baud, res;

  id = luaL_checkinteger( L, 1 );
  MOD_CHECK_ID( uart, id );

  baud = luaL_checkinteger( L, 2 );
  databits = luaL_checkinteger( L, 3 );
  parity = luaL_checkinteger( L, 4 );
  stopbits = luaL_checkinteger( L, 5 );
  if (lua_isnumber(L,6)) {
    input_setecho(lua_tointeger(L,6) ? true : false);
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
LROT_BEGIN(uart, NULL, 0)
  LROT_FUNCENTRY( setup, l_uart_setup )
  LROT_FUNCENTRY( getconfig, l_uart_getconfig )
  LROT_FUNCENTRY( write, l_uart_write )
  LROT_FUNCENTRY( on, l_uart_on )
  LROT_FUNCENTRY( alt, l_uart_alt )
  LROT_NUMENTRY( STOPBITS_1, PLATFORM_UART_STOPBITS_1 )
  LROT_NUMENTRY( STOPBITS_1_5, PLATFORM_UART_STOPBITS_1_5 )
  LROT_NUMENTRY( STOPBITS_2, PLATFORM_UART_STOPBITS_2 )
  LROT_NUMENTRY( PARITY_NONE, PLATFORM_UART_PARITY_NONE )
  LROT_NUMENTRY( PARITY_EVEN, PLATFORM_UART_PARITY_EVEN )
  LROT_NUMENTRY( PARITY_ODD, PLATFORM_UART_PARITY_ODD )
LROT_END(uart, NULL, 0)


NODEMCU_MODULE(UART, "uart", uart, NULL);
