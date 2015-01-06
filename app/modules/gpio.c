// Module for interfacing with GPIO

//#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
#include "platform.h"
#include "auxmods.h"
#include "lrotable.h"

#include "c_types.h"
#include "c_string.h"

#define PULLUP PLATFORM_GPIO_PULLUP
#define FLOAT PLATFORM_GPIO_FLOAT
#define OUTPUT PLATFORM_GPIO_OUTPUT
#define INPUT PLATFORM_GPIO_INPUT
#define INTERRUPT PLATFORM_GPIO_INT
#define HIGH PLATFORM_GPIO_HIGH
#define LOW PLATFORM_GPIO_LOW


#ifdef GPIO_INTERRUPT_ENABLE
static int gpio_cb_ref[GPIO_PIN_NUM];
static lua_State* gL = NULL;

void lua_gpio_unref(unsigned pin){
  if(gpio_cb_ref[pin] != LUA_NOREF){
    if(gL!=NULL)
      luaL_unref(gL, LUA_REGISTRYINDEX, gpio_cb_ref[pin]);
  }
  gpio_cb_ref[pin] = LUA_NOREF;
}

void gpio_intr_callback( unsigned pin, unsigned level )
{
  NODE_DBG("pin:%d, level:%d \n", pin, level);
  if(gpio_cb_ref[pin] == LUA_NOREF)
    return;
  if(!gL)
    return;
  lua_rawgeti(gL, LUA_REGISTRYINDEX, gpio_cb_ref[pin]);
  lua_pushinteger(gL, level);
  lua_call(gL, 1, 0);
}

// Lua: trig( pin, type, function )
static int lgpio_trig( lua_State* L )
{
  unsigned type;
  unsigned pin;
  size_t sl;
  
  pin = luaL_checkinteger( L, 1 );
  MOD_CHECK_ID( gpio, pin );
  if(pin==0)
    return luaL_error( L, "no interrupt for D0" );

  const char *str = luaL_checklstring( L, 2, &sl );
  if (str == NULL)
    return luaL_error( L, "wrong arg type" );

  if(sl == 2 && c_strcmp(str, "up") == 0){
    type = GPIO_PIN_INTR_POSEDGE;
  }else if(sl == 4 && c_strcmp(str, "down") == 0){
    type = GPIO_PIN_INTR_NEGEDGE;
  }else if(sl == 4 && c_strcmp(str, "both") == 0){
    type = GPIO_PIN_INTR_ANYEGDE;
  }else if(sl == 3 && c_strcmp(str, "low") == 0){
    type = GPIO_PIN_INTR_LOLEVEL;
  }else if(sl == 4 && c_strcmp(str, "high") == 0){
    type = GPIO_PIN_INTR_HILEVEL;
  }else{
    type = GPIO_PIN_INTR_DISABLE;
  }

  // luaL_checkanyfunction(L, 3);
  if (lua_type(L, 3) == LUA_TFUNCTION || lua_type(L, 3) == LUA_TLIGHTFUNCTION){
    lua_pushvalue(L, 3);  // copy argument (func) to the top of stack
    if(gpio_cb_ref[pin] != LUA_NOREF)
      luaL_unref(L, LUA_REGISTRYINDEX, gpio_cb_ref[pin]);
    gpio_cb_ref[pin] = luaL_ref(L, LUA_REGISTRYINDEX);
  }

  platform_gpio_intr_init(pin, type);
  return 0;  
}
#endif

// Lua: mode( pin, mode, pullup )
static int lgpio_mode( lua_State* L )
{
  unsigned mode, pullup = FLOAT;
  unsigned pin;

  pin = luaL_checkinteger( L, 1 );
  MOD_CHECK_ID( gpio, pin );
  mode = luaL_checkinteger( L, 2 );
  if ( mode!=OUTPUT && mode!=INPUT && mode!=INTERRUPT)
    return luaL_error( L, "wrong arg type" );
  if(pin==0 && mode==INTERRUPT)
    return luaL_error( L, "no interrupt for D0" );
  if(lua_isnumber(L, 3))
    pullup = lua_tointeger( L, 3 );
  if(pullup!=FLOAT)
    pullup = PULLUP;
#ifdef GPIO_INTERRUPT_ENABLE
  gL = L;   // save to local gL, for callback function
  if (mode!=INTERRUPT){     // disable interrupt
    if(gpio_cb_ref[pin] != LUA_NOREF){
      luaL_unref(L, LUA_REGISTRYINDEX, gpio_cb_ref[pin]);
    }
    gpio_cb_ref[pin] = LUA_NOREF;
  }
#endif
  int r = platform_gpio_mode( pin, mode, pullup );
  if( r<0 )
    return luaL_error( L, "wrong pin num." );
  return 0;  
}

// Lua: read( pin )
static int lgpio_read( lua_State* L )
{
  unsigned pin;
  
  pin = luaL_checkinteger( L, 1 );
  MOD_CHECK_ID( gpio, pin );

  unsigned level = platform_gpio_read( pin );
  lua_pushinteger( L, level );
  return 1; 
}

// Lua: write( pin, level )
static int lgpio_write( lua_State* L )
{
  unsigned level;
  unsigned pin;
  
  pin = luaL_checkinteger( L, 1 );
  MOD_CHECK_ID( gpio, pin );
  level = luaL_checkinteger( L, 2 );
  if ( level!=HIGH && level!=LOW )
    return luaL_error( L, "wrong arg type" );
  platform_gpio_write(pin, level);
  return 0;  
}

// Module function map
#define MIN_OPT_LEVEL 2
#include "lrodefs.h"
const LUA_REG_TYPE gpio_map[] = 
{
  { LSTRKEY( "mode" ), LFUNCVAL( lgpio_mode ) },
  { LSTRKEY( "read" ), LFUNCVAL( lgpio_read ) },
  { LSTRKEY( "write" ), LFUNCVAL( lgpio_write ) },
#ifdef GPIO_INTERRUPT_ENABLE
  { LSTRKEY( "trig" ), LFUNCVAL( lgpio_trig ) },
#endif
#if LUA_OPTIMIZE_MEMORY > 0
#ifdef GPIO_INTERRUPT_ENABLE
  { LSTRKEY( "INT" ), LNUMVAL( INTERRUPT ) },
#endif
  { LSTRKEY( "OUTPUT" ), LNUMVAL( OUTPUT ) },
  { LSTRKEY( "INPUT" ), LNUMVAL( INPUT ) },
  { LSTRKEY( "HIGH" ), LNUMVAL( HIGH ) },
  { LSTRKEY( "LOW" ), LNUMVAL( LOW ) },
  { LSTRKEY( "FLOAT" ), LNUMVAL( FLOAT ) },
  { LSTRKEY( "PULLUP" ), LNUMVAL( PULLUP ) },
#endif
  { LNILKEY, LNILVAL }
};

LUALIB_API int luaopen_gpio( lua_State *L )
{
#ifdef GPIO_INTERRUPT_ENABLE
  int i;
  for(i=0;i<GPIO_PIN_NUM;i++){
    gpio_cb_ref[i] = LUA_NOREF;
  }
  platform_gpio_init(gpio_intr_callback);
#endif

#if LUA_OPTIMIZE_MEMORY > 0
  return 0;
#else // #if LUA_OPTIMIZE_MEMORY > 0
  luaL_register( L, AUXLIB_GPIO, gpio_map );
  // Add constants
#ifdef GPIO_INTERRUPT_ENABLE
  MOD_REG_NUMBER( L, "INT", INTERRUPT );
#endif
  MOD_REG_NUMBER( L, "OUTPUT", OUTPUT );
  MOD_REG_NUMBER( L, "INPUT", INPUT );
  MOD_REG_NUMBER( L, "HIGH", HIGH );
  MOD_REG_NUMBER( L, "LOW", LOW );
  MOD_REG_NUMBER( L, "FLOAT", FLOAT );
  MOD_REG_NUMBER( L, "PULLUP", PULLUP );
  return 1;
#endif // #if LUA_OPTIMIZE_MEMORY > 0  
}
