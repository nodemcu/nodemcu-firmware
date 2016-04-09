#include "module.h"
#include "lauxlib.h"
#include "platform.h"
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
// The values for t0l, t1l, ttot have been tweaked and it doesn't get faster than this.
// The datasheet is confusing and one might think that a shorter pulse time can be achieved.
// The period has to be at least 1.25us, even if the datasheet says:
//   T0H: 0.35 (+- 0.15) + T0L: 0.8 (+- 0.15), which is 0.85<->1.45 us.
//   T1H: 0.70 (+- 0.15) + T1L: 0.6 (+- 0.15), which is 1.00<->1.60 us.
// Anything lower than 1.25us will glitch in the long run.
static void ICACHE_RAM_ATTR tm1829_write(uint8_t pin, uint8_t *pixels, uint32_t length) {
  uint8_t *p, *end, pixel, mask;
  uint32_t t, t0l, t1l, ttot, c, start_time, pin_mask;

  pin_mask = 1 << pin;
  p =  pixels;
  end =  p + length;
  pixel = *p++;
  mask = 0x80;

  t0l  = (1000 * system_get_cpu_freq()) / 3022;  // 0.35us (spec=0.35 +- 0.15)
  t1l  = (1000 * system_get_cpu_freq()) / 1477;  // 0.70us (spec=0.70 +- 0.15)
  ttot = (1000 * system_get_cpu_freq()) /  800;  // 1.25us (MUST be >= 1.25)

  while (true) {
    if (pixel & mask)
        t = t1l;
    else
        t = t0l;

    start_time = _getCycleCount();                        // Save the start time
    GPIO_REG_WRITE(GPIO_OUT_W1TC_ADDRESS, pin_mask);      // Set pin low
    while (((c = _getCycleCount()) - start_time) < t);    // Wait for low time to finish

    GPIO_REG_WRITE(GPIO_OUT_W1TS_ADDRESS, pin_mask);      // Set pin high
    while (((c = _getCycleCount()) - start_time) < ttot); // Wait for the previous bit to finish

    if (!(mask >>= 1)) {                                  // Next bit/byte
      if (p >= end)
        break;

      pixel = *p++;
      mask  = 0x80;
    }
  }
}

// Lua: tm1829.writergb(pin, "string")
// Byte triples in the string are interpreted as R G B values and sent to the hardware as G R B.

// tm1829.write(4, string.char(255, 0, 0)) uses GPIO2 and sets the first LED red.
// tm1829.write(3, string.char(0, 0, 255):rep(10)) uses GPIO0 and sets ten LEDs blue.
// tm1829.write(4, string.char(0, 255, 0, 255, 255, 255)) first LED green, second LED white.
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

  // Rearrange R G B values to G R B order needed by tm1829 LEDs:
  size_t i;
  for (i = 0; i < length; i += 3) {
    const char r = buffer[i];
    const char g = buffer[i + 1];
    const char b = buffer[i + 2];

    buffer[i] = b;
    buffer[i + 1] = r;
    buffer[i + 2] = g;
  }

  // Initialize the output pin and wait a bit
  platform_gpio_mode(pin, PLATFORM_GPIO_OUTPUT, PLATFORM_GPIO_FLOAT);
  platform_gpio_write(pin, PLATFORM_GPIO_HIGH);

  // Make pin high for 500us to make sure chip is ready to receive
  // data
  os_delay_us(500);

  // Send the buffer
  ets_intr_lock();
  tm1829_write(pin_num[pin], (uint8_t*) buffer, length);
  ets_intr_unlock();

  c_free(buffer);

  return 0;
}

static const LUA_REG_TYPE tm1829_map[] =
{
  { LSTRKEY( "write" ), LFUNCVAL( tm1829_writergb )},
  { LNILKEY, LNILVAL}
};

int luaopen_tm1829(lua_State *L) {
  // TODO: Make sure that the GPIO system is initialized
  return 0;
}

NODEMCU_MODULE(TM1829, "tm1829", tm1829_map, luaopen_tm1829);
