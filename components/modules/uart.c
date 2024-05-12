// Module for interfacing with serial

#include "module.h"
#include "lauxlib.h"
#include "platform.h"
#include "linput.h"
#include "lmem.h"

#include <stdint.h>
#include <string.h>

typedef struct {
  int receive_rf;
  int error_rf;
  char *line_buffer;
  size_t line_position;
  uint16_t need_len;
  int16_t end_char;
} uart_cb_cfg_t;

static lua_State *gL = NULL;
static uart_cb_cfg_t uart_cb_cfg[NUM_UART];


static bool uart_on_data_cb(unsigned id, const char *buf, size_t len){
  if(!buf || len==0)
    return false;
  if(uart_cb_cfg[id].receive_rf == LUA_NOREF)
    return false;
  if(!gL)
    return false;

  int top = lua_gettop(gL);
  lua_rawgeti(gL, LUA_REGISTRYINDEX, uart_cb_cfg[id].receive_rf);
  lua_pushlstring(gL, buf, len);
  luaL_pcallx(gL, 1, 0);
  lua_settop(gL, top);
  return !run_input;
}


bool uart_on_error_cb(unsigned id, const char *buf, size_t len){
  if(!buf || len==0)
    return false;
  if(uart_cb_cfg[id].error_rf == LUA_NOREF)
    return false;
  if(!gL)
    return false;

  int top = lua_gettop(gL);
  lua_rawgeti(gL, LUA_REGISTRYINDEX, uart_cb_cfg[id].error_rf);
  lua_pushlstring(gL, buf, len);
  luaL_pcallx(gL, 1, 0);
  lua_settop(gL, top);
  return true;
}


bool uart_has_on_data_cb(unsigned id){
  return uart_cb_cfg[id].receive_rf != LUA_NOREF;
}


void uart_feed_data(unsigned id, const char *buf, size_t len)
{
  if (id >= NUM_UART)
    return;

  uart_cb_cfg_t *cfg = &uart_cb_cfg[id];
  if (!cfg->line_buffer)
    return;

  for (unsigned i = 0; i < len; ++i)
  {
    char ch = buf[i];
    cfg->line_buffer[cfg->line_position] = ch;
    cfg->line_position++;

    uint16_t need_len = cfg->need_len;
    int16_t end_char = cfg->end_char;
    size_t max_wanted =
      (end_char >= 0 && need_len == 0) ? LUA_MAXINPUT : need_len;
    bool at_end = (cfg->line_position >= max_wanted);
    bool end_char_found =
      (end_char >= 0 && (uint8_t)ch == (uint8_t)end_char);
    if (at_end || end_char_found) {
      uart_on_data_cb(id, cfg->line_buffer, cfg->line_position);
      cfg->line_position = 0;
    }
  }
}


// Lua: uart.on([id], "method", [number/char], function, [run_input])
static int uart_on( lua_State* L )
{
  unsigned id = CONFIG_ESP_CONSOLE_UART_NUM;
  size_t sl, el;
  int32_t run = 1;
  uint8_t stack = 1;
  const char *method;
  
  if( lua_isnumber( L, stack ) ) {
    id = ( unsigned )luaL_checkinteger( L, stack );
    MOD_CHECK_ID( uart, id );
    stack++;
  }

  uart_cb_cfg_t *cfg = &uart_cb_cfg[id];

  method = luaL_checklstring( L, stack, &sl );
  stack++;
  if (method == NULL)
    return luaL_error( L, "wrong arg type" );

  if( lua_type( L, stack ) == LUA_TNUMBER )
  {
    cfg->need_len = (uint16_t)luaL_checkinteger(L, stack);
    stack++;
    cfg->end_char = -1;
    if(cfg->need_len > 255)
    {
      cfg->need_len = 255;
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
    cfg->end_char = (int16_t)end[0];
    cfg->need_len = 0;
  }

  if (lua_isfunction(L, stack)) {
    if ( lua_isnumber(L, stack+1) ){
      run = lua_tointeger(L, stack+1);
    }
    lua_pushvalue(L, stack);  // copy argument (func) to the top of stack
  } else {
    lua_pushnil(L);
  }
  if(sl == 4 && strcmp(method, "data") == 0){
    if(id == CONFIG_ESP_CONSOLE_UART_NUM)
      run_input = true;
    if(cfg->receive_rf != LUA_NOREF){
      luaL_unref(L, LUA_REGISTRYINDEX, cfg->receive_rf);
      cfg->receive_rf = LUA_NOREF;
    }
    if(!lua_isnil(L, -1)){
      cfg->receive_rf = luaL_ref(L, LUA_REGISTRYINDEX);
      gL = L;
      if(id == CONFIG_ESP_CONSOLE_UART_NUM && run==0)
        run_input = false;
    } else {
      lua_pop(L, 1);
    }
  } else if(sl == 5 && strcmp(method, "error") == 0){
    if(cfg->error_rf != LUA_NOREF){
      luaL_unref(L, LUA_REGISTRYINDEX, cfg->error_rf);
      cfg->error_rf = LUA_NOREF;
    }
    if(!lua_isnil(L, -1)){
      cfg->error_rf = luaL_ref(L, LUA_REGISTRYINDEX);
      gL = L;
    } else {
      lua_pop(L, 1);
    }
  } else {
    lua_pop(L, 1);
    return luaL_error( L, "method not supported" );
  }
  return 0; 
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
  MOD_CHECK_ID( uart, id );
  baud = luaL_checkinteger( L, 2 );
  databits = luaL_checkinteger( L, 3 );
  parity = luaL_checkinteger( L, 4 );
  stopbits = luaL_checkinteger( L, 5 );
  if (!lua_isnoneornil(L, 6)) {
    if(id == CONFIG_ESP_CONSOLE_UART_NUM){
      input_echo = luaL_checkinteger(L, 6) > 0;
    } else {
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
  }

  res = platform_uart_setup( id, baud, databits, parity, stopbits, pins_to_use );
  lua_pushinteger( L, res );
  return 1;
}

static int uart_setmode(lua_State* L)
{
  unsigned id, mode;

  id = luaL_checkinteger( L, 1 );
  MOD_CHECK_ID( uart, id );
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
  MOD_CHECK_ID( uart, id );
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
  MOD_CHECK_ID( uart, id );
  platform_uart_stop( id );
  if (uart_cb_cfg[id].line_buffer)
  {
    luaM_freemem(L, uart_cb_cfg[id].line_buffer, LUA_MAXINPUT);
    uart_cb_cfg[id].line_buffer = NULL;
  }
  return 0;
}

// Lua: start( id )
static int uart_start( lua_State* L )
{
  unsigned id;
  int err;
  id = luaL_checkinteger( L, 1 );
  MOD_CHECK_ID( uart, id );
  if (!uart_cb_cfg[id].line_buffer)
    uart_cb_cfg[id].line_buffer = luaM_malloc(L, LUA_MAXINPUT);
  err = platform_uart_start( id );
  lua_pushboolean( L, err == 0 );
  return 1;
}

static int uart_getconfig(lua_State* L) {
    uint32_t id, baud, databits, parity, stopbits;

    id = luaL_checkinteger(L, 1);
    MOD_CHECK_ID(uart, id);

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
  MOD_CHECK_ID(uart, id);
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
  MOD_CHECK_ID(uart, id);
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
    uart_cb_cfg_t *cfg = &uart_cb_cfg[id];
    cfg->receive_rf = LUA_NOREF;
    cfg->error_rf = LUA_NOREF;
    cfg->line_buffer = NULL;
    cfg->line_position = 0;
    cfg->need_len = 0;
    cfg->end_char = -1;
  }
  return 0;
}

NODEMCU_MODULE(UART, "uart", uart, luaopen_uart);
