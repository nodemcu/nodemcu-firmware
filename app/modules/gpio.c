// Module for interfacing with GPIO

#include "module.h"
#include "lauxlib.h"
#include "platform.h"

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

// This task is scheduled by the ISP if pin_trigger[pin] is true and is used
// to intitied the Lua-land gpio.trig() callback function
static void gpio_intr_callback_task (task_param_t param, uint8 priority)
{
  unsigned pin = param >> 1;
  unsigned level = param & 1;
  UNUSED(priority);
  
  NODE_DBG("pin:%d, level:%d \n", pin, level);
  if(gpio_cb_ref[pin] != LUA_NOREF) {
    // GPIO callbacks are run in L0 and inlcude the level as a parameter
    lua_State *L = lua_getstate();
NODE_DBG("Calling: %08x\n", gpio_cb_ref[pin]);
    // The trigger is reset before the callback, as the callback itself might reset the trigger
    pin_trigger[pin] = true;
    lua_rawgeti(L, LUA_REGISTRYINDEX, gpio_cb_ref[pin]);
    lua_pushinteger(L, level);
    lua_call(L, 1, 0);
  }
}

// Lua: trig( pin, type, function )
static int lgpio_trig( lua_State* L )
{
  unsigned type = GPIO_PIN_INTR_DISABLE;
  unsigned pin = luaL_checkinteger( L, 1 );
  size_t sl;
  const char *str = luaL_checklstring( L, 2, &sl );
  const char * const opt[] = {"", "up", "down", "both", "low", "high"};
  
  luaL_argcheck(L, pin>0 && pin< NUM_GPIO, 1, "Invalid interrupt pin");
  luaL_argcheck(L, str != NULL, 2, "trigger type ommitted" );

  if      (str[0] == 'u') type = GPIO_PIN_INTR_POSEDGE;
  else if (str[0] == 'd') type = GPIO_PIN_INTR_NEGEDGE;
  else if (str[0] == 'b') type = GPIO_PIN_INTR_ANYEDGE;
  else if (str[0] == 'l') type = GPIO_PIN_INTR_LOLEVEL;
  else if (str[0] == 'h') type = GPIO_PIN_INTR_HILEVEL;
  
  if (c_strcmp(str,opt[type])) {
    type = GPIO_PIN_INTR_DISABLE;
  } else {
    int cbtype = lua_type(L, 3);
    luaL_argcheck(L, cbtype == LUA_TFUNCTION || cbtype == LUA_TLIGHTFUNCTION, 3,
      "invalid callback type");
    lua_pushvalue(L, 3);  // copy argument (func) to the top of stack
    gpio_cb_ref[pin] = luaL_ref(L, LUA_REGISTRYINDEX);
  }
NODE_DBG("Pin data: %d %d %08x, %d %d %d %d, %08x\n", 
          pin, type, pin_mux[pin], pin_num[pin], pin_func[pin], pin_int_type[pin], pin_trigger[pin], gpio_cb_ref[pin]);
  platform_gpio_intr_init(pin, type);
  return 0;  
}
#endif

// Lua: mode( pin, mode, pullup )
static int lgpio_mode( lua_State* L )
{
  unsigned pin = luaL_checkinteger( L, 1 );
  unsigned mode = luaL_checkinteger( L, 2 );
  unsigned pullup = luaL_optinteger( L, 3, FLOAT );
  
  luaL_argcheck(L, pin< NUM_GPIO && (mode!=INTERRUPT || pin>0), 1, "Invalid pin");
  luaL_argcheck(L, mode==OUTPUT || mode==INPUT 
 #ifdef GPIO_INTERRUPT_ENABLE 
                                               || mode==INTERRUPT
 #endif
                                                                  , 2,  "wrong arg type" );  
  if(pullup!=FLOAT) pullup = PULLUP;

NODE_DBG("pin,mode,pullup= %d %d %d\n",pin,mode,pullup);
NODE_DBG("Pin data at mode: %d %08x, %d %d %d %d, %08x\n", 
          pin, pin_mux[pin], pin_num[pin], pin_func[pin], pin_int_type[pin], pin_trigger[pin], gpio_cb_ref[pin]);

#ifdef GPIO_INTERRUPT_ENABLE
  if (mode != INTERRUPT){     // disable interrupt
    if(gpio_cb_ref[pin] != LUA_NOREF){
      luaL_unref(L, LUA_REGISTRYINDEX, gpio_cb_ref[pin]);
      gpio_cb_ref[pin] = LUA_NOREF;
    }
  }
#endif

  if( platform_gpio_mode( pin, mode, pullup ) < 0 )
    return luaL_error( L, "wrong pin num." );

  return 0;  
}

// Lua: read( pin )
static int lgpio_read( lua_State* L )
{
  unsigned pin = luaL_checkinteger( L, 1 );
  MOD_CHECK_ID( gpio, pin );
  lua_pushinteger( L, platform_gpio_read( pin ) );
  return 1; 
}

// Lua: write( pin, level )
static int lgpio_write( lua_State* L )
{
  unsigned pin = luaL_checkinteger( L, 1 );
  unsigned level = luaL_checkinteger( L, 2 );
  
  MOD_CHECK_ID( gpio, pin );
  luaL_argcheck(L, level==HIGH || level==LOW, 2, "wrong level type" );

  platform_gpio_write(pin, level);
  return 0;  
}

#define DELAY_TABLE_MAX_LEN 256
#define noInterrupts ets_intr_lock
#define interrupts ets_intr_unlock
#define delayMicroseconds os_delay_us
#define DIRECT_WRITE(pin, level)    (GPIO_OUTPUT_SET(GPIO_ID_PIN(pin_num[pin]), level))
// Lua: serout( pin, firstLevel, delay_table, [repeatNum] )
// -- serout( pin, firstLevel, delay_table, [repeatNum] )
// gpio.mode(1,gpio.OUTPUT,gpio.PULLUP)
// gpio.serout(1,1,{30,30,60,60,30,30})  -- serial one byte, b10110010
// gpio.serout(1,1,{30,70},8)  -- serial 30% pwm 10k, lasts 8 cycles
// gpio.serout(1,1,{3,7},8)  -- serial 30% pwm 100k, lasts 8 cycles
// gpio.serout(1,1,{0,0},8)  -- serial 50% pwm as fast as possible, lasts 8 cycles
// gpio.mode(1,gpio.OUTPUT,gpio.PULLUP)
// gpio.serout(1,0,{20,10,10,20,10,10,10,100}) -- sim uart one byte 0x5A at about 100kbps
// gpio.serout(1,1,{8,18},8) -- serial 30% pwm 38k, lasts 8 cycles
static int lgpio_serout( lua_State* L )
{
  unsigned level;
  unsigned pin;
  unsigned table_len = 0;
  unsigned repeat = 0;
  int delay_table[DELAY_TABLE_MAX_LEN];
  
  pin = luaL_checkinteger( L, 1 );
  MOD_CHECK_ID( gpio, pin );
  level = luaL_checkinteger( L, 2 );
  if ( level!=HIGH && level!=LOW )
    return luaL_error( L, "wrong arg type" );
  if( lua_istable( L, 3 ) )
  {
    table_len = lua_objlen( L, 3 );
    if (table_len <= 0 || table_len>DELAY_TABLE_MAX_LEN)
      return luaL_error( L, "wrong arg range" );
    int i;
    for( i = 0; i < table_len; i ++ )
    {
      lua_rawgeti( L, 3, i + 1 );
      delay_table[i] = ( int )luaL_checkinteger( L, -1 );
      lua_pop( L, 1 );
      if( delay_table[i] < 0 || delay_table[i] > 1000000 )    // can not delay more than 1000000 us
        return luaL_error( L, "delay must < 1000000 us" );
    }
  } else {
    return luaL_error( L, "wrong arg range" );
  } 

  if(lua_isnumber(L, 4))
    repeat = lua_tointeger( L, 4 );
  if( repeat < 0 || repeat > DELAY_TABLE_MAX_LEN )
    return luaL_error( L, "delay must < 256" );

  if(repeat==0)
    repeat = 1;
  int j;
  bool skip_loop = true;
  do
  {
    if(skip_loop){    // skip the first loop.
      skip_loop = false;
      continue;
    }
    for(j=0;j<table_len;j++){
      /* Direct Write is a ROM function which already disables interrupts for the atomic bit */
      DIRECT_WRITE(pin, level);
      delayMicroseconds(delay_table[j]);
      level=!level;
    }
    repeat--;
  } while (repeat>0); 

  return 0;  
}
#undef DELAY_TABLE_MAX_LEN

// Module function map
static const LUA_REG_TYPE gpio_map[] = {
  { LSTRKEY( "mode" ),   LFUNCVAL( lgpio_mode ) },
  { LSTRKEY( "read" ),   LFUNCVAL( lgpio_read ) },
  { LSTRKEY( "write" ),  LFUNCVAL( lgpio_write ) },
  { LSTRKEY( "serout" ), LFUNCVAL( lgpio_serout ) },
#ifdef GPIO_INTERRUPT_ENABLE
  { LSTRKEY( "trig" ),   LFUNCVAL( lgpio_trig ) },
  { LSTRKEY( "INT" ),    LNUMVAL( INTERRUPT ) },
#endif
  { LSTRKEY( "OUTPUT" ), LNUMVAL( OUTPUT ) },
  { LSTRKEY( "INPUT" ),  LNUMVAL( INPUT ) },
  { LSTRKEY( "HIGH" ),   LNUMVAL( HIGH ) },
  { LSTRKEY( "LOW" ),    LNUMVAL( LOW ) },
  { LSTRKEY( "FLOAT" ),  LNUMVAL( FLOAT ) },
  { LSTRKEY( "PULLUP" ), LNUMVAL( PULLUP ) },
  { LNILKEY, LNILVAL }
};

int luaopen_gpio( lua_State *L ) {
#ifdef GPIO_INTERRUPT_ENABLE
  int i;
  for(i=0;i<GPIO_PIN_NUM;i++){
    gpio_cb_ref[i] = LUA_NOREF;
  }
  platform_gpio_init(task_get_id(gpio_intr_callback_task));
#endif
  return 0;
}

NODEMCU_MODULE(GPIO, "gpio", gpio_map, luaopen_gpio);
