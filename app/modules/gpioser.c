// Module for interfacing with GPIOs

// Usage:
// gpioser.read(PIN_CLK, PIN_OUT, num_bits, extra_ticks, delay_us, initial_val, ready_state)
// gpioser.read(5, 6, 24, 1, 1, 0, 0)
// -- read 24 bits, from pin 6, pin 5 is CLOCK, hold it for 1us, initial value
// -- written to pin 5 is LOW (0), and ready state is when the pin 6 changes to LOW (0)


#include "lualib.h"
#include "lauxlib.h"
#include "platform.h"
#include "module.h"
#include "c_types.h"
#include "c_string.h"

#define PULLUP PLATFORM_GPIO_PULLUP
#define FLOAT PLATFORM_GPIO_FLOAT
#define OUTPUT PLATFORM_GPIO_OUTPUT
#define INPUT PLATFORM_GPIO_INPUT
#define INTERRUPT PLATFORM_GPIO_INT
#define HIGH PLATFORM_GPIO_HIGH
#define LOW PLATFORM_GPIO_LOW


unsigned int PIN_CLK;
unsigned int PIN_DATA;


long read(int pin_clk, int pin_data, int num_bits, int after_ticks,
        int delay_us, int initial_clk_write, int ready_state) {
    unsigned long Count;
    unsigned char i;
    unsigned int maxWait = 1000;
    Count=0;

    platform_gpio_write(pin_clk, initial_clk_write);
    
    while (platform_gpio_read(pin_data) != ready_state) {
      maxWait--;
      if (maxWait == 0) return -1;
    }

    for (i=0;i<num_bits;i++){
        platform_gpio_write(pin_clk, HIGH );
        Count=Count<<1;

        os_delay_us(delay_us);

        if (platform_gpio_read(pin_data) == HIGH) Count++;
        platform_gpio_write(pin_clk, LOW );

        os_delay_us(delay_us);
    }
    for (i = 1; i <= after_ticks; i++) {
        platform_gpio_write(pin_clk, HIGH);
        os_delay_us(delay_us);
        platform_gpio_write(pin_clk, LOW);
        os_delay_us(delay_us);
    }
    return(Count);
}

// debugging function (to return the current values of the module)
static int gpioser_info( lua_State* L )
{
    char buf[255];
    c_sprintf(buf, "pin clock:%d, pin output:%d", PIN_CLK, PIN_DATA);
    lua_pushfstring(L, buf);
    return 1;
}

static int gpioser_read( lua_State* L )
{
    int pin_clk;
    int pin_data;
    int num_bits;
    int after_ticks;
    int delay_us;
    int initial_clk_write;
    int ready_state;

    pin_clk = (int) luaL_checkinteger( L, 1 );
    pin_data = (int) luaL_checkinteger( L, 2 );
    num_bits = (int) luaL_checkinteger( L, 3 );
    after_ticks = (int)luaL_checkinteger( L, 4 );
    delay_us = (int) luaL_checkinteger( L, 5 );
    initial_clk_write = (int) luaL_checkinteger( L, 6 );
    ready_state = (int) luaL_checkinteger( L, 7 );

    long int result;
    result = read(pin_clk, pin_data, num_bits, after_ticks,
                delay_us, initial_clk_write, ready_state);

    lua_pushnumber(L, (lua_Number)result);
    return 1;
}

// Module function map, this is how we tell Lua what API our module has
const LUA_REG_TYPE gpioser_map[] =
{
  { LSTRKEY( "read" ), LFUNCVAL( gpioser_read ) },
  { LSTRKEY( "info" ), LFUNCVAL( gpioser_info ) },
  { LNILKEY, LNILVAL }
};

NODEMCU_MODULE(GPIOSER, "gpioser", gpioser_map, NULL);
