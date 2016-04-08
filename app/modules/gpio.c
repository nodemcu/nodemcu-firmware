// Module for interfacing with GPIO

#include "module.h"
#include "lauxlib.h"
#include "lmem.h"
#include "platform.h"
#include "user_interface.h"
#include "c_types.h"
#include "c_string.h"
#include "gpio.h"

#define PULLUP PLATFORM_GPIO_PULLUP
#define FLOAT PLATFORM_GPIO_FLOAT
#define OUTPUT PLATFORM_GPIO_OUTPUT
#define OPENDRAIN PLATFORM_GPIO_OPENDRAIN
#define INPUT PLATFORM_GPIO_INPUT
#define INTERRUPT PLATFORM_GPIO_INT
#define HIGH PLATFORM_GPIO_HIGH
#define LOW PLATFORM_GPIO_LOW

#ifdef GPIO_INTERRUPT_ENABLE

// We also know that the non-level interrupt types are < LOLEVEL, and that
// HILEVEL is > LOLEVEL. Since this is burned into the hardware it is not
// going to change.
#define INTERRUPT_TYPE_IS_LEVEL(x)	((x) >= GPIO_PIN_INTR_LOLEVEL)

static int gpio_cb_ref[GPIO_PIN_NUM];

// This task is scheduled by the ISR and is used
// to initiate the Lua-land gpio.trig() callback function
// It also re-enables the pin interrupt, so that we get another callback queued
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
    //
    if (!INTERRUPT_TYPE_IS_LEVEL(pin_int_type[pin])) {
      // Edge triggered -- re-enable the interrupt
      platform_gpio_intr_init(pin, pin_int_type[pin]);
    }

    // Do the actual callback
    lua_rawgeti(L, LUA_REGISTRYINDEX, gpio_cb_ref[pin]);
    lua_pushinteger(L, level);
    lua_call(L, 1, 0);

    if (INTERRUPT_TYPE_IS_LEVEL(pin_int_type[pin])) {
      // Level triggered -- re-enable the callback
      platform_gpio_intr_init(pin, pin_int_type[pin]);
    }
  }
}

// Lua: trig( pin, type, function )
static int lgpio_trig( lua_State* L )
{
  unsigned pin = luaL_checkinteger( L, 1 );
  static const char * const opts[] = {"none", "up", "down", "both", "low", "high", NULL};
  static const int opts_type[] = {
    GPIO_PIN_INTR_DISABLE, GPIO_PIN_INTR_POSEDGE, GPIO_PIN_INTR_NEGEDGE,
    GPIO_PIN_INTR_ANYEDGE, GPIO_PIN_INTR_LOLEVEL, GPIO_PIN_INTR_HILEVEL
    };
  luaL_argcheck(L, platform_gpio_exists(pin) && pin>0, 1, "Invalid interrupt pin");

  int old_pin_ref = gpio_cb_ref[pin];
  int type = opts_type[luaL_checkoption(L, 2, "none", opts)];

  if (type == GPIO_PIN_INTR_DISABLE) {
    // "none" clears the callback
    gpio_cb_ref[pin] = LUA_NOREF;

  } else if (lua_gettop(L)==2 && old_pin_ref != LUA_NOREF) {
    // keep the old one if no callback 
    old_pin_ref = LUA_NOREF;

  } else if (lua_type(L, 3) == LUA_TFUNCTION || lua_type(L, 3) == LUA_TLIGHTFUNCTION) {
    // set up the new callback if present
    lua_pushvalue(L, 3);
    gpio_cb_ref[pin] = luaL_ref(L, LUA_REGISTRYINDEX);

  } else {
     // invalid combination, so clear down any old callback and throw an error
    if(old_pin_ref != LUA_NOREF) luaL_unref(L, LUA_REGISTRYINDEX, old_pin_ref);
    luaL_argcheck(L,  0, 3, "invalid callback type");
  }

  // unreference any overwritten callback
  if(old_pin_ref != LUA_NOREF) luaL_unref(L, LUA_REGISTRYINDEX, old_pin_ref);

  NODE_DBG("Pin data: %d %d %08x, %d %d %d, %08x\n",
          pin, type, pin_mux[pin], pin_num[pin], pin_func[pin], pin_int_type[pin], gpio_cb_ref[pin]);
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

  luaL_argcheck(L, platform_gpio_exists(pin) && (mode!=INTERRUPT || pin>0), 1, "Invalid pin");
  luaL_argcheck(L, mode==OUTPUT || mode==OPENDRAIN || mode==INPUT
 #ifdef GPIO_INTERRUPT_ENABLE
                                               || mode==INTERRUPT
 #endif
                                                                  , 2,  "wrong arg type" );
  if(pullup!=FLOAT) pullup = PULLUP;

NODE_DBG("pin,mode,pullup= %d %d %d\n",pin,mode,pullup);
NODE_DBG("Pin data at mode: %d %08x, %d %d %d, %08x\n",
          pin, pin_mux[pin], pin_num[pin], pin_func[pin], pin_int_type[pin], gpio_cb_ref[pin]);

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
#define delayMicroseconds os_delay_us
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
  unsigned clocks_per_us = system_get_cpu_freq();
  unsigned pin = luaL_checkinteger( L, 1 );
  unsigned level = luaL_checkinteger( L, 2 );
  unsigned repeats = luaL_optint( L, 4, 1 );
  unsigned table_len, i, j;

  luaL_argcheck(L, platform_gpio_exists(pin), 1, "Invalid pin");
  luaL_argcheck(L, level==HIGH || level==LOW, 2, "Wrong arg type" );
  luaL_argcheck(L, lua_istable( L, 3 ) &&
                   ((table_len = lua_objlen( L, 3 )<DELAY_TABLE_MAX_LEN)), 3, "Invalid table" );
  luaL_argcheck(L, repeats<256, 4, "repeats >= 256" );

  uint32 *delay_table = luaM_newvector(L, table_len*repeats, uint32);
  for( i = 1; i <= table_len; i++ )  {
    lua_rawgeti( L, 3, i + 1 );
    unsigned delay = (unsigned) luaL_checkinteger( L, -1 );
    if (delay > 1000000) return luaL_error( L, "delay %u must be < 1,000,000 us", i );
    delay_table[i-1] = delay;
    lua_pop( L, 1 );
  }

  for( i = 0; i <= repeats; i++ )  {
    if (!i)    // skip the first loop (presumably this is some form of icache priming??).
      continue;

    for( j = 0;j < table_len; j++ ){
      /* Direct Write is a ROM function which already disables interrupts for the atomic bit */
      GPIO_OUTPUT_SET(GPIO_ID_PIN(pin_num[pin]), level);
      delayMicroseconds(delay_table[j]);
      level = level==LOW ? HIGH : LOW;
    }
  }

  luaM_freearray(L, delay_table, table_len, uint32);
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
  { LSTRKEY( "OUTPUT" ),    LNUMVAL( OUTPUT ) },
  { LSTRKEY( "OPENDRAIN" ), LNUMVAL( OPENDRAIN ) },
  { LSTRKEY( "INPUT" ),     LNUMVAL( INPUT ) },
  { LSTRKEY( "HIGH" ),      LNUMVAL( HIGH ) },
  { LSTRKEY( "LOW" ),       LNUMVAL( LOW ) },
  { LSTRKEY( "FLOAT" ),     LNUMVAL( FLOAT ) },
  { LSTRKEY( "PULLUP" ),    LNUMVAL( PULLUP ) },
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
