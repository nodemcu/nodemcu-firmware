// Module for HX711 load cell amplifier
// https://learn.sparkfun.com/tutorials/load-cell-amplifier-hx711-breakout-hookup-guide

#include "module.h"
#include "lauxlib.h"
#include "lmem.h"
#include "platform.h"
#include <stdlib.h>
#include <string.h>
#include "user_interface.h"
static uint8_t data_pin;
static uint8_t clk_pin;
// The fields below are after the pin_num conversion
static uint8_t pin_data_pin;
static uint8_t pin_clk_pin;

#ifdef GPIO_INTERRUPT_ENABLE
static platform_task_handle_t tasknumber;

// HX711_STATUS can be defined to enable the hx711.status() function to get debug info
#undef HX711_STATUS
#define BUFFERS 2

typedef struct {
  char *buf[BUFFERS];
  uint32_t dropped[BUFFERS];
  uint32_t timestamp[BUFFERS];
  uint32_t interrupts;
  uint32_t hx711_interrupts;
  uint16_t buflen;
  uint16_t used;
  uint32_t nobuffer;
  uint8_t active;  // slot of the active buffer
  uint8_t freed; // slot of the most recently freed buffer
  uint8_t mode;
  uint8_t dropping;  // is non zero when there is no space
  int cb_ref;
} CONTROL;

static CONTROL *control;
#endif

/*Lua: hx711.init(clk_pin,data_pin)*/
static int hx711_init(lua_State* L) {
  clk_pin = luaL_checkint(L,1);
  data_pin = luaL_checkint(L,2);
  MOD_CHECK_ID( gpio, clk_pin );
  MOD_CHECK_ID( gpio, data_pin );

  platform_gpio_mode(clk_pin, PLATFORM_GPIO_OUTPUT, PLATFORM_GPIO_FLOAT);
  platform_gpio_mode(data_pin, PLATFORM_GPIO_INPUT, PLATFORM_GPIO_FLOAT);
  platform_gpio_write(clk_pin,1);//put chip to sleep.

  pin_data_pin = pin_num[data_pin];
  pin_clk_pin = pin_num[clk_pin];
  return 0;
}

static int32_t ICACHE_RAM_ATTR read_sample(char mode) {
  int i;
  int32_t data = 0;

  for (i = 0; i < 24 ; i++){  //clock in the 24 bits
    GPIO_REG_WRITE(GPIO_OUT_W1TS_ADDRESS, 1 << pin_clk_pin);
    GPIO_REG_WRITE(GPIO_OUT_W1TC_ADDRESS, 1 << pin_clk_pin);
    data = data << 1;
    if (GPIO_REG_READ(GPIO_IN_ADDRESS) & (1 << pin_data_pin)) {
      data = i == 0 ? -1 : data | 1; //signextend the first bit
    }
  }
  //add 25th-27th clock pulse to prevent protocol error 
  for (i = 0; i <= mode; i++) {
    GPIO_REG_WRITE(GPIO_OUT_W1TS_ADDRESS, 1 << pin_clk_pin);
    GPIO_REG_WRITE(GPIO_OUT_W1TC_ADDRESS, 1 << pin_clk_pin);
  }

  return data;
}

#ifdef GPIO_INTERRUPT_ENABLE
static void ICACHE_RAM_ATTR hx711_data_available() {
  if (!control) {
    return;
  }
  uint32_t bits = GPIO_REG_READ(GPIO_IN_ADDRESS);
  if (bits & (1 << pin_data_pin)) {
    return;   // not ready
  }

  // Read a sample
  int32_t data = read_sample(control->mode);

  if (control->dropping) {
    if (control->active == control->freed) {
      // still can't advance
      control->nobuffer++;
      return;
    }
    // Advance
    control->active = (1 + control->active) % BUFFERS;
    control->dropping = 0;
  }

  // insert into the active buffer
  char *dest = control->buf[control->active] + control->used;
  *dest++ = data;
  *dest++ = data >> 8;
  *dest++ = data >> 16;

  control->used += 3;
  if (control->used == control->buflen) {
    control->used = 0;
    control->timestamp[control->active] = system_get_time();
    control->dropped[control->active] = control->nobuffer;
    control->nobuffer = 0;
    // post task
    platform_post_medium(tasknumber, control->active);

    uint8_t next_active = (1 + control->active) % BUFFERS;

    if (control->active == control->freed) {
      // We can't advance to the buffer
      control->dropping = 1;
    } else {
      // flip to other buffer
      control->active = next_active;
    }
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

  int pin_mask = 1 << pin_data_pin;
  int i;

  control->interrupts++;

  if (gpio_status & pin_mask) {
    uint32_t bits = GPIO_REG_READ(GPIO_IN_ADDRESS);
    control->hx711_interrupts++;
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
  uint32_t mode = luaL_checkint( L, 1 );
  uint32_t samples = luaL_checkint( L, 2 );

  if (mode > 2) {
    return luaL_argerror( L, 1, "Mode value out of range" );
  }

  if (!samples || samples > 400) {
    return luaL_argerror( L, 2, "Samples value out of range (1-400)" );
  }

  if (control) {
    return luaL_error( L, "Already running" );
  }

  int buflen = 3 * samples;

  control = (CONTROL *) luaM_malloc(L, sizeof(CONTROL) + BUFFERS * buflen);
  if (!control) {
    return luaL_error( L, "Failed to allocate memory" );
  }

  int cb_ref;

  if (lua_type(L, 3) == LUA_TFUNCTION) {
    lua_pushvalue(L, 3);  // copy argument (func) to the top of stack
    cb_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  } else {
    luaM_free(L, control);
    control = NULL;
    return luaL_argerror( L, 3, "Not a callback function" );
  }

  memset(control, 0, sizeof(*control));
  control->buf[0] = (char *) (control + 1);
  control->buflen = buflen;
  int i;

  for (i = 1; i < BUFFERS; i++) {
    control->buf[i] = control->buf[i - 1] + buflen;
  }
  control->mode = mode;
  control->cb_ref = cb_ref;
  control->freed = BUFFERS - 1;

  // configure data_pin as interrupt input
  platform_gpio_register_intr_hook(1 << pin_data_pin, hx711_interrupt);
  platform_gpio_mode(data_pin, PLATFORM_GPIO_INT, PLATFORM_GPIO_FLOAT);
  platform_gpio_intr_init(data_pin, GPIO_PIN_INTR_NEGEDGE);


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

static int hx711_status( lua_State* L )
{
  if (control) {
    lua_pushlstring(L, (char *) control, sizeof(*control));
    return 1;
  }

  return 0;
}

static void hx711_task(platform_task_param_t param, uint8_t prio)
{
  (void) prio;
  if (!control) {
    return;
  }

  lua_State *L = lua_getstate();

  if (control->cb_ref != LUA_NOREF) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, control->cb_ref);

    lua_pushlstring(L, control->buf[param], control->buflen);
    lua_pushinteger(L, control->timestamp[param]);
    lua_pushinteger(L, control->dropped[param]);

    control->freed = param;

    lua_call(L, 3, 0);
  }
}
#endif

#define HX711_MAX_WAIT 1000000
/*will only read chA@128gain*/
/*Lua: result = hx711.read()*/
static int hx711_read(lua_State* L) {
  int j;
  //TODO: double check init has happened first.
  //

  uint32_t mode = luaL_optinteger(L, 1, 0);

  if (mode > 2) {
    return luaL_argerror( L, 1, "Mode value out of range" );
  }

#ifdef GPIO_INTERRUPT_ENABLE
  if (control) {
    hx711_stop(L);
  }
#endif

  //wakeup hx711
  platform_gpio_write(clk_pin, 0);

  int32_t data;

  // read two samples if mode > 0. We discard the first read and return the 
  // second value.
  for (j = (mode ? 1 : 0); j >= 0; j--) {
    uint32_t i;

    //wait for data ready.  or time out.
    system_soft_wdt_feed(); //clear WDT... this may take a while.
    for (i = 0; i<HX711_MAX_WAIT && platform_gpio_read(data_pin)==1;i++){
      asm ("nop");
    }

    //Handle timeout error
    if (i >= HX711_MAX_WAIT) {
      return luaL_error( L, "ADC timeout!");
    }

    data = read_sample(mode);
  }

  //sleep -- unfortunately, this resets the mode to 0
  platform_gpio_write(clk_pin, 1);
  lua_pushinteger(L, data);
  return 1;
}

// Module function map
LROT_BEGIN(hx711, NULL, 0)
  LROT_FUNCENTRY( init, hx711_init )
  LROT_FUNCENTRY( read, hx711_read )
#ifdef GPIO_INTERRUPT_ENABLE
  LROT_FUNCENTRY( start,  hx711_start )
#ifdef HX711_STATUS
  LROT_FUNCENTRY( status, hx711_status )
#endif
  LROT_FUNCENTRY( stop,  hx711_stop )
#endif
LROT_END(hx711, NULL, 0)


int luaopen_hx711(lua_State *L) {
#ifdef GPIO_INTERRUPT_ENABLE
  tasknumber = platform_task_get_id(hx711_task);
#endif
  return 0;
}

NODEMCU_MODULE(HX711, "hx711", hx711, luaopen_hx711);
