#include "module.h"
#include "lauxlib.h"
#include "platform.h"
#include "c_stdlib.h"
#include "c_string.h"

/**
 * Code is based on https://github.com/CHERTS/esp8266-devkit/blob/master/Espressif/examples/EspLightNode/user/ws2801.c
 * and provides a similar api as the ws2812 module.
 * The current implementation allows the caller to use
 * any combination of GPIO0, GPIO2, GPIO4, GPIO5 as clock and data.
 */

#define PIN_CLK_DEFAULT         0
#define PIN_DATA_DEFAULT        2

static uint32_t ws2801_bit_clk;
static uint32_t ws2801_bit_data;

static void ws2801_byte(uint8_t n) {
    uint8_t bitmask;
    for (bitmask = 0x80; bitmask !=0 ; bitmask >>= 1) {
        if (n & bitmask) {
            GPIO_REG_WRITE(GPIO_OUT_W1TS_ADDRESS, ws2801_bit_data);
        } else {
            GPIO_REG_WRITE(GPIO_OUT_W1TC_ADDRESS, ws2801_bit_data);
        }
        GPIO_REG_WRITE(GPIO_OUT_W1TS_ADDRESS, ws2801_bit_clk);
        GPIO_REG_WRITE(GPIO_OUT_W1TC_ADDRESS, ws2801_bit_clk);
    }
}

static void ws2801_color(uint8_t r, uint8_t g, uint8_t b) {
    ws2801_byte(r);
    ws2801_byte(g);
    ws2801_byte(b);
}

static void ws2801_strip(uint8_t const * data, uint16_t len) {
    while (len--) {
        ws2801_byte(*(data++));
    }
    GPIO_REG_WRITE(GPIO_OUT_W1TC_ADDRESS, ws2801_bit_data);
}

static void enable_pin_mux(int pin) {
    // The API only supports setting PERIPHS_IO_MUX on GPIO 0, 2, 4, 5
    switch (pin) {
    case 0:
        PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO0_U, FUNC_GPIO0);
        break;
    case 2:
        PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO2_U, FUNC_GPIO2);
        break;
    case 4:
        PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO4_U, FUNC_GPIO4);
        break;
    case 5:
        PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO5_U, FUNC_GPIO5);
        break;
    default:
        break;
    }
}

/* Lua: ws2801.init(pin_clk, pin_data)
 * Sets up the GPIO pins
 * 
 * ws2801.init(0, 2) uses GPIO0 as clock and GPIO2 as data.
 * This is the default behavior.
 */
static int ICACHE_FLASH_ATTR ws2801_init_lua(lua_State* L) {
    uint32_t pin_clk;
    uint32_t pin_data;
    uint32_t func_gpio_clk;
    uint32_t func_gpio_data;

    if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2)) {
        // Use default pins if the input is omitted
        pin_clk = PIN_CLK_DEFAULT;
        pin_data = PIN_DATA_DEFAULT;
    } else {
        pin_clk = luaL_checkinteger(L, 1);
        pin_data = luaL_checkinteger(L, 2);
    }

    ws2801_bit_clk = 1 << pin_clk;
    ws2801_bit_data = 1 << pin_data;

    os_delay_us(10);

    //Set GPIO pins to output mode
    enable_pin_mux(pin_clk);
    enable_pin_mux(pin_data);

    //Set both GPIOs low low
    gpio_output_set(0, ws2801_bit_clk | ws2801_bit_data, ws2801_bit_clk | ws2801_bit_data, 0);

    os_delay_us(10);
}

/* Lua: ws2801.write(pin, "string")
 * Byte triples in the string are interpreted as R G B values.
 * This function does not corrupt your buffer.
 *
 * ws2801.write(string.char(255, 0, 0)) sets the first LED red.
 * ws2801.write(string.char(0, 0, 255):rep(10)) sets ten LEDs blue.
 * ws2801.write(string.char(0, 255, 0, 255, 255, 255)) first LED green, second LED white.
 */
static int ICACHE_FLASH_ATTR ws2801_writergb(lua_State* L) {
    size_t length;
    const char *buffer = luaL_checklstring(L, 1, &length);

    os_delay_us(10);

    ets_intr_lock();

    ws2801_strip(buffer, length);

    ets_intr_unlock();

    return 0;
}

static const LUA_REG_TYPE ws2801_map[] =
{
    { LSTRKEY( "write" ), LFUNCVAL( ws2801_writergb )},
    { LSTRKEY( "init" ), LFUNCVAL( ws2801_init_lua )},
    { LNILKEY, LNILVAL}
};

NODEMCU_MODULE(WS2801, "ws2801", ws2801_map, NULL);
