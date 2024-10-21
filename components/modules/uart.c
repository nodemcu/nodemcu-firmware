// Module for interfacing with serial

#include "module.h"
#include "lauxlib.h"
#include "platform.h"
#include "serial_common.h"
#include "linput.h"
#include "lmem.h"

#include <stdint.h>
#include <string.h>

static serial_input_cfg_t *uart_cb_cfg[NUM_UART];


bool uart_on_error_cb(unsigned id, const char *buf, size_t len){
  return serial_input_report_error(uart_cb_cfg[id], buf, len);
}


bool uart_has_on_data_cb(unsigned id){
  return serial_input_has_data_cb(uart_cb_cfg[id]);
}


void uart_feed_data(unsigned id, const char *buf, size_t len)
{
  if (id >= NUM_UART)
    return;

  serial_input_feed_data(uart_cb_cfg[id], buf, len);
}


static int ensure_valid_id(lua_State *L, int id)
{
  MOD_CHECK_ID(uart, id);

  int console = -1;
#if defined(CONFIG_ESP_CONSOLE_UART_DEFAULT) || defined(CONFIG_ESP_CONSOLE_UART_CUSTOM)
  console = CONFIG_ESP_CONSOLE_UART_NUM;
#endif

  if (id == console)
    return luaL_error(L,
      "uart in use by system console; use the 'console' module instead");

  return 0;
}


// Lua: uart.on([id], "method", [number/char], function, [run_input])
static int uart_on( lua_State* L )
{
  int id = 0;
  if (lua_isnumber(L, 1))
  {
    id = luaL_checkinteger(L, 1);
    lua_remove(L, 1);
  }

  ensure_valid_id(L, id);

  return serial_input_register(L, uart_cb_cfg[id]);
}


// Lua: actualbaud = setup( id, baud, databits, parity, stopbits, echo )
static int uart_setup( lua_State* L )
{
  unsigned id, databits, parity, stopbits;
  uint32_t baud, res;
  uart_pins_t pins;
  uart_pins_t* pins_to_use = NULL;
  
  memset(&pins, 0, sizeof(pins));
  id = luaL_checkinteger( L, 1 );
  ensure_valid_id(L, id);
  baud = luaL_checkinteger( L, 2 );
  databits = luaL_checkinteger( L, 3 );
  parity = luaL_checkinteger( L, 4 );
  stopbits = luaL_checkinteger( L, 5 );
  if (!lua_isnoneornil(L, 6)) {
    luaL_checktable(L, 6);

    lua_getfield (L, 6, "tx");
    pins.tx_pin = luaL_checkint(L, -1);
    lua_getfield (L, 6, "rx");
    pins.rx_pin = luaL_checkint(L, -1);
    lua_getfield (L, 6, "cts");
    pins.cts_pin = luaL_optint(L, -1, -1);
    lua_getfield (L, 6, "rts");
    pins.rts_pin = luaL_optint(L, -1, -1);

    lua_getfield (L, 6, "tx_inverse");
    pins.tx_inverse = lua_toboolean(L, -1);
    lua_getfield (L, 6, "rx_inverse");
    pins.rx_inverse = lua_toboolean(L, -1);
    lua_getfield (L, 6, "cts_inverse");
    pins.cts_inverse = lua_toboolean(L, -1);
    lua_getfield (L, 6, "rts_inverse");
    pins.rts_inverse = lua_toboolean(L, -1);

    lua_getfield (L, 6, "flow_control");
    pins.flow_control = luaL_optint(L, -1, PLATFORM_UART_FLOW_NONE);

    pins_to_use = &pins;
  }

  res = platform_uart_setup( id, baud, databits, parity, stopbits, pins_to_use );
  lua_pushinteger( L, res );
  return 1;
}

static int uart_setmode(lua_State* L)
{
  unsigned id, mode;

  id = luaL_checkinteger( L, 1 );
  ensure_valid_id(L, id);
  mode = luaL_checkinteger( L, 2 );

  platform_uart_setmode(id, mode);

  return 0;
}

// Lua: write( id, string1, [string2], ..., [stringn] )
static int uart_write( lua_State* L )
{
  unsigned id;
  const char* buf;
  size_t len;
  int total = lua_gettop( L ), s;
  
  id = luaL_checkinteger( L, 1 );
  ensure_valid_id(L, id);
  for( s = 2; s <= total; s ++ )
  {
    if( lua_type( L, s ) == LUA_TNUMBER )
    {
      len = lua_tointeger( L, s );
      if( len > 255 )
        return luaL_error( L, "invalid number" );
      platform_uart_send( id, (uint8_t)len );
    }
    else
    {
      luaL_checktype( L, s, LUA_TSTRING );
      buf = lua_tolstring( L, s, &len );
      platform_uart_send_multi( id, buf, len );
    }
  }
  platform_uart_flush( id );
  return 0;
}

// Lua: stop( id )
static int uart_stop( lua_State* L )
{
  unsigned id;
  id = luaL_checkinteger( L, 1 );
  ensure_valid_id(L, id);
  platform_uart_stop( id );
  return 0;
}

// Lua: start( id )
static int uart_start( lua_State* L )
{
  unsigned id;
  int err;
  id = luaL_checkinteger( L, 1 );
  ensure_valid_id(L, id);
  err = platform_uart_start( id );
  lua_pushboolean( L, err == 0 );
  return 1;
}

static int uart_getconfig(lua_State* L) {
    uint32_t id, baud, databits, parity, stopbits;

    id = luaL_checkinteger(L, 1);
    ensure_valid_id(L, id);

    int err = platform_uart_get_config(id, &baud, &databits, &parity, &stopbits);
    if (err) {
      luaL_error(L, "Error reading UART config");
    }

    lua_pushinteger(L, baud);
    lua_pushinteger(L, databits);
    lua_pushinteger(L, parity);
    lua_pushinteger(L, stopbits);
    return 4;
}

static int uart_wakeup (lua_State *L)
{
  uint32_t id = luaL_checkinteger(L, 1);
  ensure_valid_id(L, id);
  int threshold = luaL_checkinteger(L, 2);
  int err = platform_uart_set_wakeup_threshold(id, threshold);
  if (err) {
    return luaL_error(L, "Error %d from uart_set_wakeup_threshold()", err);
  }
  return 0;
}

static int luart_tx_flush (lua_State *L)
{
  uint32_t id = luaL_checkinteger(L, 1);
  ensure_valid_id(L, id);
  platform_uart_flush(id);
  return 0;
}

// Module function map
LROT_BEGIN(uart, NULL, 0)
  LROT_FUNCENTRY( setup,                      uart_setup )
  LROT_FUNCENTRY( write,                      uart_write )
  LROT_FUNCENTRY( start,                      uart_start )
  LROT_FUNCENTRY( stop,                       uart_stop )
  LROT_FUNCENTRY( on,                         uart_on )
  LROT_FUNCENTRY( setmode,                    uart_setmode )
  LROT_FUNCENTRY( getconfig,                  uart_getconfig )
  LROT_FUNCENTRY( wakeup,                     uart_wakeup )
  LROT_FUNCENTRY( txflush,                    luart_tx_flush )
  LROT_NUMENTRY( STOPBITS_1,                  PLATFORM_UART_STOPBITS_1 )
  LROT_NUMENTRY( STOPBITS_1_5,                PLATFORM_UART_STOPBITS_1_5 )
  LROT_NUMENTRY( STOPBITS_2,                  PLATFORM_UART_STOPBITS_2 )
  LROT_NUMENTRY( PARITY_NONE,                 PLATFORM_UART_PARITY_NONE )
  LROT_NUMENTRY( PARITY_EVEN,                 PLATFORM_UART_PARITY_EVEN )
  LROT_NUMENTRY( PARITY_ODD,                  PLATFORM_UART_PARITY_ODD )
  LROT_NUMENTRY( FLOWCTRL_NONE,               PLATFORM_UART_FLOW_NONE )
  LROT_NUMENTRY( FLOWCTRL_CTS,                PLATFORM_UART_FLOW_CTS )
  LROT_NUMENTRY( FLOWCTRL_RTS,                PLATFORM_UART_FLOW_RTS )
  LROT_NUMENTRY( MODE_UART,                   PLATFORM_UART_MODE_UART )
  LROT_NUMENTRY( MODE_RS485_COLLISION_DETECT, PLATFORM_UART_MODE_RS485_COLLISION_DETECT )
  LROT_NUMENTRY( MODE_RS485_APP_CONTROL,      PLATFORM_UART_MODE_RS485_APP_CONTROL )
  LROT_NUMENTRY( MODE_RS485_HALF_DUPLEX,      PLATFORM_UART_MODE_HALF_DUPLEX )
  LROT_NUMENTRY( MODE_IRDA,                   PLATFORM_UART_MODE_IRDA )
LROT_END(uart, NULL, 0)

int luaopen_uart( lua_State *L ) {
  for(int id = 0; id < sizeof(uart_cb_cfg)/sizeof(uart_cb_cfg[0]); id++)
  {
    uart_cb_cfg[id] = serial_input_new();
    if (!uart_cb_cfg[id])
      return luaL_error(L, "out of mem");
  }
  return 0;
}

NODEMCU_MODULE(UART, "uart", uart, luaopen_uart);
