#include "lualib.h"
#include "lauxlib.h"
#include "platform.h"
#include "auxmods.h"
#include "lrotable.h"
#include "c_stdlib.h"
#include "c_string.h"
#include "user_interface.h"

static inline uint32_t _getCycleCount(void) {
  uint32_t cycles;
  __asm__ __volatile__("rsr %0,ccount":"=a" (cycles));
  return cycles;
}

// This algorithm reads the cpu clock cycles to calculate the correct
// pulse widths. It works in both 80 and 160 MHz mode.
// The values for t0l, t1l, ttot are for low-speed mode (and not optimized).
// The TM1829 also has a high-speed mode. Timings are:
//   T0L: typical: 170 ns (50 - 250 ns)
//   T1L: typical: 450 ns (300 - 550 ns)
//   TTOT: min 600 ns
//   TresetL: typical: 500 us (min 140 us) (for both timings)
static void ICACHE_RAM_ATTR tm1829_write(uint8_t pin, uint8_t *pixels, uint32_t length) {
  uint8_t *p, *end, pixel, mask;
  uint32_t t, t0l, t1l, ttot, c, start_time, pin_mask;

  pin_mask = 1 << pin;
  p =  pixels;
  end =  p + length;
  pixel = *p++;
  mask = 0x80;
  start_time = 0;
  t0l  = (1000 * system_get_cpu_freq()) / 3333;  // 0.30us (spec=0.30 +- 0.15)
  t1l  = (1000 * system_get_cpu_freq()) / 1250;  // 0.80us (spec=0.80 +- 0.20)
  ttot = (1000 * system_get_cpu_freq()) /  833;  // 1.20us (spec=must be >= 1.20)

  while (true) {
    if (pixel & mask) {
        t = t1l;
    } else {
        t = t0l;
    }
    while (((c = _getCycleCount()) - start_time) < ttot); // Wait for the previous bit to finish
    GPIO_REG_WRITE(GPIO_OUT_W1TC_ADDRESS, pin_mask);      // Set pin low
    start_time = c;                                       // Save the start time
    while (((c = _getCycleCount()) - start_time) < t);    // Wait for low time to finish
    GPIO_REG_WRITE(GPIO_OUT_W1TS_ADDRESS, pin_mask);      // Set pin high
    if (!(mask >>= 1)) {                                  // Next bit/byte
      if (p >= end) {
        break;
      }
      pixel= *p++;
      mask = 0x80;
    }
  }
}

// Reset the TM1829
// restart with first pixel
static void ICACHE_RAM_ATTR tm1829_reset(uint8_t pin) {
  GPIO_REG_WRITE(GPIO_OUT_W1TC_ADDRESS, 1 << pin);      // Set pin low
  os_delay_us(250);                                     // wait reset time
  GPIO_REG_WRITE(GPIO_OUT_W1TS_ADDRESS, 1 << pin);      // Set pin high
}

// Lua: tm1829.writergb(pin, "string")
// Byte triples in the string are interpreted as R G B values and sent to the hardware as B R G.
// WARNING: this function scrambles the input buffer :
//    a = string.char(255,0,128)
//    tm1829.writergb(3,a)
//    =a.byte()
//    (0,255,128)

// tm1829.writergb(4, string.char(255, 0, 0)) uses GPIO2 and sets the first LED red.
// tm1829.writergb(3, string.char(0, 0, 255):rep(10)) uses GPIO0 and sets ten LEDs blue.
// tm1829.writergb(4, string.char(0, 255, 0, 255, 255, 255)) first LED green, second LED white.
static int ICACHE_FLASH_ATTR tm1829_writergb(lua_State* L)
{
  const uint8_t pin = luaL_checkinteger(L, 1);
  size_t length;
  const char *rgb = luaL_checklstring(L, 2, &length);

  // dont modify lua-internal lstring - make a copy instead
  char *buffer = (char *)c_malloc(length);
  c_memcpy(buffer, rgb, length);

  // Ignore incomplete Byte triples at the end of buffer:
  length -= length % 3;

  // Rearrange R G B values to B R G order needed by TM1829 LEDs:
  size_t i;
  for (i = 0; i < length; i += 3) {
    const char r = buffer[i];
    const char g = buffer[i + 1];
    const char b = buffer[i + 2];
    buffer[i] = b;
    buffer[i + 1] = r;
    buffer[i + 2] = g;
    // TM1829 does interpret blue == 255 as different packet type (set constant current)
    // but we need pwm packet type, so make sure blue can't be 255
    if (buffer[i] == 0xFF) buffer[i] = 0xFE;
  }

  // Initialize the output pin and wait a bit
  platform_gpio_mode(pin, PLATFORM_GPIO_OUTPUT, PLATFORM_GPIO_FLOAT);
  platform_gpio_write(pin, 1);  // TM1829: high level on default

  // Send the buffer
  os_intr_lock();
  tm1829_write(pin_num[pin], (uint8_t*) buffer, length);
  os_intr_unlock();

  c_free(buffer);

  return 0;
}

// Lua: tm1829.write(pin, "string")
// Byte triples in the string are interpreted as B R G values.
// This function does not corrupt your buffer.
// WARNING: Make sure the blue values are not 255. Otherwise TM1829 will interpret
//    the following 2 bytes as a different packet type (set constant current)
//
// tm1829.write(4, string.char(0, 255, 0)) uses GPIO2 and sets the first LED red.
// tm1829.write(3, string.char(255, 0, 0):rep(10)) uses GPIO0 and sets ten LEDs blue.
// tm1829.write(4, string.char(0, 0, 255, 255, 255, 255)) first LED green, second LED white.
static int ICACHE_FLASH_ATTR tm1829_writebrg(lua_State* L) {
  const uint8_t pin = luaL_checkinteger(L, 1);
  size_t length;
  const char *buffer = luaL_checklstring(L, 2, &length);

  // Initialize the output pin
  platform_gpio_mode(pin, PLATFORM_GPIO_OUTPUT, PLATFORM_GPIO_FLOAT);
  platform_gpio_write(pin, 1);  // TM1829: high level on default

  // Send the buffer
  os_intr_lock();
  tm1829_write(pin_num[pin], (uint8_t*) buffer, length);
  os_intr_unlock();

  return 0;
}

#define MIN_OPT_LEVEL 2
#include "lrodefs.h"
const LUA_REG_TYPE tm1829_map[] =
{
  { LSTRKEY( "writergb" ), LFUNCVAL( tm1829_writergb )},
  { LSTRKEY( "write" ), LFUNCVAL( tm1829_writebrg )},
  { LNILKEY, LNILVAL}
};

LUALIB_API int luaopen_tm1829(lua_State *L) {
  // TODO: Make sure that the GPIO system is initialized
  LREGISTER(L, "tm1829", tm1829_map);
  return 1;
}
