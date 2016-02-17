
This short tutorial will show you how to enhance the NodeMCU firmware with your own module written in C/C++. Bear in mind that most of the times you don't need to do that, as the existing modules may already provide what you need. But if for whatever reason you want to add your C/C++ code, you can.

You will need to have a build chain, or you may want to use the docker image, see (https://github.com/nodemcu/nodemcu-firmware#building-the-firmware)


## C Module

First, we'll create our C module - I'll use here a real example, which actually does something useful. It is just one file (and does not define functions in the header). It is saved inside `app/modules/easygpio.c`. The module reads data from GPIO channels.


```c
// Module for interfacing with GPIOs

// Usage:
// easygpio.read(PIN_CLK, PIN_OUT, num_bits, extra_ticks, delay_us, initial_val, ready_state)
// easygpio.read(5, 6, 24, 1, 1, 0, 0)
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
unsigned int STATE_INDICATOR;


bool is_ready() {
    if (STATE_INDICATOR == 0)
        return platform_gpio_read(PIN_DATA) == LOW;
    return platform_gpio_read(PIN_DATA) == HIGH;
}

long read(int pin_clk, int pin_data, int num_bits, int after_ticks,
        int delay_us, int initial_clk_write, int ready_state) {
    unsigned long Count;
    unsigned char i;
    Count=0;

    platform_gpio_write(pin_clk, initial_clk_write);

    while (platform_gpio_read(pin_data) != ready_state);

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
static int easygpio_info( lua_State* L )
{
    char buf[255];
    c_sprintf(buf, "pin clock:%d, pin output:%d, state:%d", PIN_CLK, PIN_DATA, STATE_INDICATOR);
    lua_pushfstring(L, buf);
    return 1;
}

static int easygpio_read( lua_State* L )
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
const LUA_REG_TYPE easygpio_map[] =
{
  { LSTRKEY( "read" ), LFUNCVAL( easygpio_read ) },
  { LSTRKEY( "info" ), LFUNCVAL( easygpio_info ) },
  { LNILKEY, LNILVAL }
};

NODEMCU_MODULE(EASYGPIO, "easygpio", easygpio_map, NULL);
```

As you can notice, the bulk of the library deals with just reading data from the GPIO's - for that, we are calling ESP functions (eg: `platform_gpio_read`). You will have to do little bit of detective work, as the documentation on ESP SDK is not so great. I recommend looking at existing code.

The second important part of the module is the actual connection between the C code and the Lua. First, we have to let Lua know which methods are accessible. That is done through `const LUA_REG_TYPE easygpio_map[]`. We are exposing two functions, `easygpio_read` and `easygpio_info`. They will be registered when the module is imported - the `NODEMCU_MODULE(EASYGPIO, "easygpio", easygpio_map, NULL)`

The `L` is a Lua State and you can read about it in the [Extending your Application](http://www.lua.org/pil/25.html), [Calling C from Lua](http://www.lua.org/pil/26.html), and [Manual](http://www.lua.org/manual/5.1/manual.html#3).

The connection between the Lua and the C function is nicely visible in the `easygpio_read` function - there, we take get values one by one (Lua State is a stack) and transforme them to the appropriate type before calling the C method. And when we got results, we'll put it back into the stack (Lua State).

  

## Integrate the C Module into NodeMCU

 When compiled, our module will be part of the `libmodules.a`. But that does not mean it will be automatically included in the generated firmware. That depends on the `app/include/user_modules.h` header. The following declaration will accomplish that:
 
```
#define LUA_USE_MODULES_EASYGPIO
```

### Compilation

It is ready! We can now compile the code and if everything went well, flash the device. I'm showing here the Docker image method (you will have to update your own paths and parameters based on the version of ESP you have):

```
$ cd nodemcu-firmware
$ docker run --rm -ti -v `pwd`:/opt/nodemcu-firmware marcelstoer/nodemcu-build
$ sudo esptool.py --port $usb_port write_flash 0x00000 ./bin/nodemcu_float_master_20160206-0220.bin
```

## Lua Module

This is optional, but I figured it might be useful to show how to write a Lua module that uses the `easygpio.c` library. You will have to upload it to the device after flashing the firmware (using `nodemcu-uploader`, `ESPlorer` or some other method).


```lua
local moduleName = ... 
local M = {}
_G[moduleName] = M

-- variables that affect HX711 reading (we read data with easygpio module)
local DOUT = 6;
local SCLK = 5;
local BITS_READ = 24;
local GAIN = 1;
local DELAY_US = 1;
local READY_STATE = gpio.LOW;
local INITIAL_VAL = gpio.HIGH;
local OFFSET = 0;
local SCALE_UNIT = 1;


-- HX711 accepts gain of 128, 64, and 32; the default is 128 (1)
function M.set_gain(b)
    if b == 128 then
        GAIN = 1;
    elseif b == 64 then 
        GAIN = 2;
    elseif b == 32 then
        GAIN = 3;
    else then
        throw("Error, unknown gain value: " .. b);
    end
end

-- public function, returns averaged readings (minus scale offset)
function M.read(times)
    return read_average(times) - OFFSET;
end

-- offset is individual to the load-cell
function M.set_offset(off)
    OFFSET = off;
end

-- you can discover the units by measuring a known object (i.e. 1kg)
function M.set_scale_unit(u)
    SCALE_UNIT = u;
end

-- reset measured weight to zero
function M.tare(times)
    M.set_offset(read_average(times));
end

-- return weight in the units (instead of raw reading)
function read_units(times)
    return M.read(times) / SCALE_UNIT;
end


-- read raw data from the sensor (apply appropriate bitmasks)
local function read()
    val = easygpio.read(CLK, OUT, BITS_READ, GAIN, DELAY_US, INITIAL_VAL, READY_STATE);
    return bit.bxor(val, 0x80);
end

-- average value after reading 'num_try' times
local function read_average(num_try)
    s = 0;
    for i=1, num_try do
        s = s + read();
    end
    return s / num_try;
end

return M
```