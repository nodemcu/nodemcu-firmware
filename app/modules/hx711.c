// Module for HX711 load cell amplifier
// https://learn.sparkfun.com/tutorials/load-cell-amplifier-hx711-breakout-hookup-guide

#include "module.h"
#include "lauxlib.h"
#include "lmem.h"
#include "platform.h"
#include "c_stdlib.h"
#include "c_string.h"
#include "user_interface.h"
static uint8_t data_pin;
static uint8_t clk_pin;
static task_handle_t tasknumber;

typedef struct {
  char *buf[2];
  uint16_t buflen;
  uint16_t used;
  uint8_t active;
  uint8_t mode;
  int cb_ref;
} CONTROL;

static CONTROL *control;

/*Lua: hx711.init(clk_pin,data_pin)*/
static int hx711_init(lua_State* L) {
  clk_pin = luaL_checkinteger(L,1);
  data_pin = luaL_checkinteger(L,2);
  MOD_CHECK_ID( gpio, clk_pin );
  MOD_CHECK_ID( gpio, data_pin );

  platform_gpio_mode(clk_pin, PLATFORM_GPIO_OUTPUT, PLATFORM_GPIO_FLOAT);
  platform_gpio_mode(data_pin, PLATFORM_GPIO_INPUT, PLATFORM_GPIO_FLOAT);
  platform_gpio_write(clk_pin,1);//put chip to sleep.
  return 0;
}

static int32_t read_sample(char mode) {
  int i;
  int32_t data = 0;

  for (i = 0; i < 24 ; i++){  //clock in the 24 bits
    platform_gpio_write(clk_pin, 1);
    platform_gpio_write(clk_pin, 0);
    data = data << 1;
    if (platform_gpio_read(data_pin) == 1) {
      data = i == 0 ? -1 : data | 1; //signextend the first bit
    }
  }
  //add 25th-27th clock pulse to prevent protocol error 
  for (i = 0; i <= mode; i++) {
    platform_gpio_write(clk_pin, 1);
    platform_gpio_write(clk_pin, 0);
  }
}

#define HX711_MAX_WAIT 1000000
/*will only read chA@128gain*/
/*Lua: result = hx711.read()*/
static int hx711_read(lua_State* L) {
  uint32_t i;
  //TODO: double check init has happened first.

  //wakeup hx711
  platform_gpio_write(clk_pin,0);

  //wait for data ready.  or time out.
  //TODO: set pin inturrupt and come back to it.  This may take up to 1/10 sec
  //        or maybe just make an async version too and have both available.
	system_soft_wdt_feed(); //clear WDT... this may take a while.
  for (i = 0; i<HX711_MAX_WAIT && platform_gpio_read(data_pin)==1;i++){
    asm ("nop");
  }

  //Handle timeout error
  if (i>=HX711_MAX_WAIT) {
    return luaL_error( L, "ADC timeout!", ( unsigned )0 );
  }

  int32_t data = read_sample(0);

  //sleep -- unfortunately, this resets the mode to 0
  platform_gpio_write(clk_pin,1);
  lua_pushinteger( L, data );
  return 1;
}

#ifdef GPIO_INTERRUPT_ENABLE
static void ICACHE_RAM_ATTR hx711_data_available() {
  if (!control) {
    return;
  }
  if (platform_gpio_read(data_pin) == 0) {
    return;   // not ready
  }

  // Read a sample
  int32_t data = read_sample(control->mode);

  // insert into the active buffer
  char *dest = control->buf[control->active] + control->used;
  *dest++ = data;
  *dest++ = data >> 8;
  *dest++ = data >> 16;

  control->used += 3;
  if (control->used == control->buflen) {
    control->used = 0;
    // post task
    task_post_medium(tasknumber, control->active);

    // flip to other buffer
    control->active = 1 - control->active;
  }
}

static uint32_t ICACHE_RAM_ATTR hx711_interrupt(uint32_t ret_gpio_status)
{
  // This function really is running at interrupt level with everything
  // else masked off. It should take as little time as necessary.
  //
  //

  // This gets the set of pins which have changed status
  uint32 gpio_status = GPIO_REG_READ(GPIO_STATUS_ADDRESS);

  int pin_mask = 1 << data_pin;
  int i;

  if (gpio_status & pin_mask) {
    uint32_t bits = GPIO_REG_READ(GPIO_IN_ADDRESS);
    if (!(bits & pin_mask)) {
      // is now ready to read
      hx711_data_available();
    }
    GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, gpio_status & pin_mask);
  }

  return gpio_status & ~pin_mask;
}

// Lua: hx711.start( mode, samples, callback ) 
static int hx711_start( lua_State* L )
{
  uint32_t mode = luaL_checkinteger( L, 1 );
  uint32_t samples = luaL_checkinteger( L, 2 );

  if (mode > 2) {
    return luaL_error( L, "Mode value out of range" );
  }

  if (control) {
    return luaL_error( L, "Already running" );
  }

  int buflen = 3 * samples;

  control = (CONTROL *) luaM_malloc(L, sizeof(CONTROL) + 2 * buflen);
  if (!control) {
    return luaL_error( L, "Failed to allocate memory" );
  }

  int cb_ref;

  if (lua_type(L, 3) == LUA_TFUNCTION || lua_type(L, 3) == LUA_TLIGHTFUNCTION) {
    lua_pushvalue(L, 3);  // copy argument (func) to the top of stack
    cb_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  } else {
    luaM_free(L, control);
    control = NULL;
    return luaL_error( L, "No callback function specified" );
  }

  return -1;
  memset(control, 0, sizeof(*control));
  control->buf[0] = (char *) (control + 1);
  control->buflen = buflen;
  control->buf[1] = control->buf[0] + buflen;
  control->mode = mode;
  control->cb_ref = cb_ref;

  // configure data_pin as interrupt input
  platform_gpio_register_intr_hook(1 << data_pin, hx711_interrupt);
  platform_gpio_mode(data_pin, PLATFORM_GPIO_INT, PLATFORM_GPIO_FLOAT);

  // Wake up chip
  platform_gpio_write(clk_pin, 0);

  return 0;
}

// Lua: hx711.stop( ) 
static int hx711_stop( lua_State* L )
{
  if (control) {
    platform_gpio_mode(data_pin, PLATFORM_GPIO_INPUT, PLATFORM_GPIO_FLOAT);
    CONTROL *to_free = control;
    control = NULL;
    luaL_unref(L, LUA_REGISTRYINDEX, to_free->cb_ref);
    luaM_free(L, to_free);
  }

  return 0;
}

static void hx711_task(os_param_t param, uint8_t prio)
{
  (void) prio;
  if (!control) {
    return;
  }

  lua_State *L = lua_getstate();

  if (control->cb_ref != LUA_NOREF) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, control->cb_ref);

    lua_pushlstring(L, control->buf[param], control->buflen);

    lua_call(L, 1, 0);
  }
}
#endif

// Module function map
static const LUA_REG_TYPE hx711_map[] = {
  { LSTRKEY( "init" ), LFUNCVAL( hx711_init )},
  { LSTRKEY( "read" ), LFUNCVAL( hx711_read )},
#ifdef GPIO_INTERRUPT_ENABLE
  { LSTRKEY( "start" ), LFUNCVAL( hx711_start )},
  { LSTRKEY( "stop" ), LFUNCVAL( hx711_stop )},
#endif
  { LNILKEY, LNILVAL}
};

int luaopen_hx711(lua_State *L) {
  tasknumber = task_get_id(hx711_task);
  return 0;
}

NODEMCU_MODULE(HX711, "hx711", hx711_map, luaopen_hx711);
