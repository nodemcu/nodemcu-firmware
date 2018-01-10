// Module for interfacing with GPIO
//#define NODE_DEBUG

#include "module.h"
#include "lauxlib.h"
#include "lmem.h"
#include "platform.h"
#include "user_interface.h"
#include "c_types.h"
#include "c_string.h"
#include "gpio.h"
#include "hw_timer.h"

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
  uint32_t then = (param >> 8) & 0xffffff;
  uint32_t pin = (param >> 1) & 127;
  uint32_t level = param & 1;
  UNUSED(priority);

  uint32_t now = system_get_time();

  // Now must be >= then . Add the missing bits
  if (then > (now & 0xffffff)) {
    // Now must have rolled over since the interrupt -- back it down
    now -= 0x1000000;
  }
  then = (then + (now & 0x7f000000)) & 0x7fffffff;

  NODE_DBG("pin:%d, level:%d \n", pin, level);
  if(gpio_cb_ref[pin] != LUA_NOREF) {
    // GPIO callbacks are run in L0 and include the level as a parameter
    lua_State *L = lua_getstate();
    NODE_DBG("Calling: %08x\n", gpio_cb_ref[pin]);

    bool needs_callback = 1;

    while (needs_callback)  {
      // Note that the interrupt level only modifies 'seen' and
      // the base level only modifies 'reported'. 

      // Do the actual callback
      lua_rawgeti(L, LUA_REGISTRYINDEX, gpio_cb_ref[pin]);
      lua_pushinteger(L, level);
      lua_pushinteger(L, then);
      uint16_t seen = pin_counter[pin].seen;
      lua_pushinteger(L, 0x7fff & (seen - pin_counter[pin].reported));
      pin_counter[pin].reported = seen & 0x7fff; // This will cause the next interrupt to trigger a callback
      uint16_t diff = (seen ^ pin_counter[pin].seen);
      // Needs another callback if seen changed but not if the top bit is set
      needs_callback = diff <= 0x7fff && diff > 0;
      if (needs_callback) {
        // Fake this for next time (this only happens if another interrupt happens since
        // we loaded the 'seen' variable.
        then = system_get_time() & 0x7fffffff;
      }

      lua_call(L, 3, 0);
    } 

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

  uint16_t seen;

  // Make sure that we clear out any queued interrupts
  do {
    seen = pin_counter[pin].seen;
    pin_counter[pin].reported = seen & 0x7fff;
  } while (seen != pin_counter[pin].seen);

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
          pin, pin_mux[pin], pin_num[pin], pin_func[pin], 
#ifdef GPIO_INTERRUPT_ENABLE
          pin_int_type[pin], gpio_cb_ref[pin]
#else
          0, 0
#endif
          );

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
// Lua: serout( pin, firstLevel, delay_table[, repeat_num[, callback]])
// gpio.mode(1,gpio.OUTPUT,gpio.PULLUP)
// gpio.serout(1,1,{30,30,60,60,30,30})  -- serial one byte, b10110010
// gpio.serout(1,1,{30,70},8)  -- serial 30% pwm 10k, lasts 8 cycles
// gpio.serout(1,1,{3,7},8)  -- serial 30% pwm 100k, lasts 8 cycles
// gpio.serout(1,1,{0,0},8)  -- serial 50% pwm as fast as possible, lasts 8 cycles
// gpio.mode(1,gpio.OUTPUT,gpio.PULLUP)
// gpio.serout(1,0,{20,10,10,20,10,10,10,100}) -- sim uart one byte 0x5A at about 100kbps
// gpio.serout(1,1,{8,18},8) -- serial 30% pwm 38k, lasts 8 cycles

typedef struct
{
  unsigned pin;
  unsigned level;
  uint32 index;
  uint32 repeats;
  uint32 *delay_table;
  uint32 tablelen;
  task_handle_t done_taskid;
  int lua_done_ref; // callback when transmission is done
} serout_t;
static serout_t serout;
static const os_param_t TIMER_OWNER = 0x6770696f; // "gpio"

static void seroutasync_done (task_param_t arg)
{
  lua_State *L = lua_getstate();
  if (serout.delay_table) {
    luaM_freearray(L, serout.delay_table, serout.tablelen, uint32);
    serout.delay_table = NULL;
  }
  if (serout.lua_done_ref != LUA_NOREF) {
    lua_rawgeti (L, LUA_REGISTRYINDEX, serout.lua_done_ref);
    luaL_unref (L, LUA_REGISTRYINDEX, serout.lua_done_ref);
    serout.lua_done_ref = LUA_NOREF;
    if (lua_pcall(L, 0, 0, 0)) {
      // Uncaught Error. Print instead of sudden reset
      luaL_error(L, "error: %s", lua_tostring(L, -1));
    }
  }
}

static void ICACHE_RAM_ATTR seroutasync_cb(os_param_t p) {
  (void) p;
  if (serout.index < serout.tablelen) {
    GPIO_OUTPUT_SET(GPIO_ID_PIN(pin_num[serout.pin]), serout.level);
    serout.level = serout.level==LOW ? HIGH : LOW;
    platform_hw_timer_arm_us(TIMER_OWNER, serout.delay_table[serout.index]);
    serout.index++;
    if (serout.repeats && serout.index>=serout.tablelen) {serout.index=0; serout.repeats--;}
  } else {
    platform_hw_timer_close(TIMER_OWNER);
    task_post_low (serout.done_taskid, (task_param_t)0);
  }
}

static int lgpio_serout( lua_State* L )
{
  serout.pin = luaL_checkinteger( L, 1 );
  serout.level = luaL_checkinteger( L, 2 );
  serout.repeats = luaL_optint( L, 4, 1 )-1;
  luaL_unref (L, LUA_REGISTRYINDEX, serout.lua_done_ref);
  uint8_t is_async = FALSE;
  if (!lua_isnoneornil(L, 5)) {
    if (lua_isnumber(L, 5)) {
      serout.lua_done_ref = LUA_NOREF;
    } else {
      lua_pushvalue(L, 5);
      serout.lua_done_ref = luaL_ref(L, LUA_REGISTRYINDEX);
    }
    is_async = TRUE;
  } else {
    serout.lua_done_ref = LUA_NOREF;
  }

  if (serout.delay_table) {
    luaM_freearray(L, serout.delay_table, serout.tablelen, uint32);
    serout.delay_table = NULL;
  }

  luaL_argcheck(L, platform_gpio_exists(serout.pin), 1, "Invalid pin");
  luaL_argcheck(L, serout.level==HIGH || serout.level==LOW, 2, "Wrong level type" );
  luaL_argcheck(L, lua_istable( L, 3 ) &&
                   ((serout.tablelen = lua_objlen( L, 3 )) < DELAY_TABLE_MAX_LEN), 3, "Invalid delay_times" );

  serout.delay_table = luaM_newvector(L, serout.tablelen, uint32);
  for(unsigned i = 0; i < serout.tablelen; i++ )  {
    lua_rawgeti( L, 3, i + 1 );
    unsigned delay = (unsigned) luaL_checkinteger( L, -1 );
    serout.delay_table[i] = delay;
    lua_pop( L, 1 );
  }

  if (is_async) { // async version for duration above 15 mSec
    if (!platform_hw_timer_init(TIMER_OWNER, FRC1_SOURCE, TRUE)) {
      // Failed to init the timer
      luaL_error(L, "Unable to initialize timer");
    }
    platform_hw_timer_set_func(TIMER_OWNER, seroutasync_cb, 0);
    serout.index = 0;
    seroutasync_cb(0);
  } else { // sync version for sub-50 µs resolution & total duration < 15 mSec
    do { 
      for( serout.index = 0;serout.index < serout.tablelen; serout.index++ ){
        NODE_DBG("%d\t%d\t%d\t%d\t%d\t%d\t%d\n", serout.repeats, serout.index, serout.level, serout.pin, serout.tablelen, serout.delay_table[serout.index], system_get_time()); // timings is delayed for short timings when debug output is enabled
        GPIO_OUTPUT_SET(GPIO_ID_PIN(pin_num[serout.pin]), serout.level);
        serout.level = serout.level==LOW ? HIGH : LOW;
        delayMicroseconds(serout.delay_table[serout.index]);
      }
    } while (serout.repeats--);
    luaM_freearray(L, serout.delay_table, serout.tablelen, uint32);
    serout.delay_table = NULL;
  }
  return 0;
}
#undef DELAY_TABLE_MAX_LEN

#ifdef LUA_USE_MODULES_GPIO_PULSE
extern const LUA_REG_TYPE gpio_pulse_map[];
extern int gpio_pulse_init(lua_State *);
#endif

// Module function map
static const LUA_REG_TYPE gpio_map[] = {
  { LSTRKEY( "mode" ),   LFUNCVAL( lgpio_mode ) },
  { LSTRKEY( "read" ),   LFUNCVAL( lgpio_read ) },
  { LSTRKEY( "write" ),  LFUNCVAL( lgpio_write ) },
  { LSTRKEY( "serout" ), LFUNCVAL( lgpio_serout ) },
#ifdef LUA_USE_MODULES_GPIO_PULSE
  { LSTRKEY( "pulse" ),  LROVAL( gpio_pulse_map ) }, //declared in gpio_pulse.c
#endif
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
#ifdef LUA_USE_MODULES_GPIO_PULSE
  gpio_pulse_init(L);
#endif
#ifdef GPIO_INTERRUPT_ENABLE
  int i;
  for(i=0;i<GPIO_PIN_NUM;i++){
    gpio_cb_ref[i] = LUA_NOREF;
  }
  platform_gpio_init(task_get_id(gpio_intr_callback_task));
#endif
  serout.done_taskid = task_get_id((task_callback_t) seroutasync_done);
  serout.lua_done_ref = LUA_NOREF;
  return 0;
}

NODEMCU_MODULE(GPIO, "gpio", gpio_map, luaopen_gpio);
