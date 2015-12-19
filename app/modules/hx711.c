// Module for HX711 load cell amplifier
// https://learn.sparkfun.com/tutorials/load-cell-amplifier-hx711-breakout-hookup-guide

#include "module.h"
#include "lauxlib.h"
#include "platform.h"
#include "c_stdlib.h"
#include "c_string.h"
#include "user_interface.h"
static uint8_t data_pin;
static uint8_t clk_pin;

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

#define HX711_MAX_WAIT 1000000
/*will only read chA@128gain*/
/*Lua: result = hx711.read()*/
static int ICACHE_FLASH_ATTR hx711_read(lua_State* L) {
  uint32_t i;
  int32_t data = 0;
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

  for (i = 0; i<24 ; i++){  //clock in the 24 bits
    platform_gpio_write(clk_pin,1);
    platform_gpio_write(clk_pin,0);
    data = data<<1;
    if (platform_gpio_read(data_pin)==1) {
      data = i==0 ? -1 : data|1; //signextend the first bit
    }
  }
  //add 25th clock pulse to prevent protocol error (probably not needed
  // since we'll go to sleep immediately after and reset on wakeup.)
  platform_gpio_write(clk_pin,1);
  platform_gpio_write(clk_pin,0);
  //sleep
  platform_gpio_write(clk_pin,1);
  lua_pushinteger( L, data );
  return 1;
}

// Module function map
static const LUA_REG_TYPE hx711_map[] = {
  { LSTRKEY( "init" ), LFUNCVAL( hx711_init )},
  { LSTRKEY( "read" ), LFUNCVAL( hx711_read )},
  { LNILKEY, LNILVAL}
};

int luaopen_hx711(lua_State *L) {
  // TODO: Make sure that the GPIO system is initialized
  return 0;
}

NODEMCU_MODULE(HX711, "hx711", hx711_map, luaopen_hx711);
