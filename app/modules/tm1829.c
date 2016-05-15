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
static void ICACHE_RAM_ATTR tm1829_write(uint8_t pin, uint8_t *pixels, uint32_t length) {
  uint8_t *p, *end;

  p   =  pixels;
  end =  p + length;

  const uint32_t t0l  = (1000 * system_get_cpu_freq()) / 3333;  // 0.390us (spec=0.35 +- 0.15)
  const uint32_t t1l  = (1000 * system_get_cpu_freq()) / 1250;  // 0.800us (spec=0.70 +- 0.15)

  const uint32_t ttot = (1000 * system_get_cpu_freq()) / 450;   // 2.2us (MUST be >= 1.25)

  const uint32_t tbreak = (1000 * system_get_cpu_freq()) / 160; // 6us

  while (p != end) {
    register int i;

    register uint8_t pixel = *p++;

    for (i = 7; i >= 0; i--) {
       register uint32_t pin_mask = 1 << pin;

       // Select low time
       register uint32_t tl = ((pixel >> i) & 1) ? t1l : t0l;
       register uint32_t t1, t2;

       register uint32_t t = _getCycleCount();

       t1 = t + tl;
       t2 = t + ttot;

       GPIO_REG_WRITE(GPIO_OUT_W1TC_ADDRESS, pin_mask); // Set pin low
       while (_getCycleCount() < t1);

       GPIO_REG_WRITE(GPIO_OUT_W1TS_ADDRESS, pin_mask); // Set pin high
       while (_getCycleCount() < t2);
    }

    {
      register uint32_t t = _getCycleCount() + tbreak;
      while (_getCycleCount() < t);
    }
  }
}

// Lua: tm1829.write(pin, "string")
// Byte triples in the string are interpreted as R G B values and sent to the hardware as G R B.
// WARNING: this function scrambles the input buffer :
//    a = string.char(255,0,128)
//    tm1829.write(3,a)
//    =a.byte()
//    (0,255,128)

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

  // Rearrange R G B values to B R G order needed by tm1829 LEDs:
  size_t i;
  for (i = 0; i < length; i += 3) {
    char r = buffer[i];
    char g = buffer[i + 1];
    char b = buffer[i + 2];

    // First 8 bit must not be 1s (constant current command,
    // instead of PWM duty command)
    if (b == 0xff)
        b = 0xfe;

    buffer[i] = b;
    buffer[i + 1] = r;
    buffer[i + 2] = g;
  }

  // Initialize the output pin and wait a bit
  platform_gpio_mode(pin, PLATFORM_GPIO_OUTPUT, PLATFORM_GPIO_FLOAT);
  platform_gpio_write(pin, 1);

  // Send the buffer
  ets_intr_lock();
  tm1829_write(pin_num[pin], (uint8_t*) buffer, length);
  ets_intr_unlock();

  os_delay_us(500); // reset time

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
