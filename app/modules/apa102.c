#include "c_stdlib.h"
#include "c_string.h"
#include "lualib.h"
#include "lauxlib.h"
#include "lrotable.h"
#include "module.h"
#include "platform.h"
#include "user_interface.h"


#define NOP asm volatile(" nop \n\t")


static inline void apa102_send_byte(uint32_t data_pin, uint32_t clock_pin, uint8_t byte) {
  int i;
  for (i = 0; i < 8; i++) {
    if (byte & 0x80) {
      GPIO_OUTPUT_SET(data_pin, PLATFORM_GPIO_HIGH); // Set pin high
    } else {
      GPIO_OUTPUT_SET(data_pin, PLATFORM_GPIO_LOW); // Set pin low
    }
    GPIO_OUTPUT_SET(clock_pin, PLATFORM_GPIO_HIGH); // Set pin high
    byte <<= 1;
    NOP;
    NOP;
    GPIO_OUTPUT_SET(clock_pin, PLATFORM_GPIO_LOW); // Set pin low
    NOP;
    NOP;
  }
}


static void apa102_send_buffer(uint32_t data_pin, uint32_t clock_pin, uint32_t *buf, uint32_t nbr_frames) {
  int i;

  // Send 32-bit Start Frame that's all 0x00
  apa102_send_byte(data_pin, clock_pin, 0x00);
  apa102_send_byte(data_pin, clock_pin, 0x00);
  apa102_send_byte(data_pin, clock_pin, 0x00);
  apa102_send_byte(data_pin, clock_pin, 0x00);

  // Send 32-bit LED Frames
  for (i = 0; i < nbr_frames; i++) {
    uint8_t *byte = (uint8_t *) buf++;

    // Set the first 3 bits of that byte to 1.
    // This makes the lua interface easier to use since you
    // don't have to worry about creating invalid LED Frames.
    byte[0] |= 0xE0;
    apa102_send_byte(data_pin, clock_pin, byte[0]);
    apa102_send_byte(data_pin, clock_pin, byte[1]);
    apa102_send_byte(data_pin, clock_pin, byte[2]);
    apa102_send_byte(data_pin, clock_pin, byte[3]);
  }

  // Send 32-bit End Frames
  uint32_t required_postamble_frames = (nbr_frames + 1) / 2;
  for (i = 0; i < required_postamble_frames; i++) {
    apa102_send_byte(data_pin, clock_pin, 0xFF);
    apa102_send_byte(data_pin, clock_pin, 0xFF);
    apa102_send_byte(data_pin, clock_pin, 0xFF);
    apa102_send_byte(data_pin, clock_pin, 0xFF);
  }
}


// Lua: apa102.write(data_pin, clock_pin, "string")
// Byte quads in the string are interpreted as (brightness, B, G, R) values.
// Only the first 5 bits of the brightness value is actually used (0-31).
// This function does not corrupt your buffer.
//
// apa102.write(1, 3, string.char(31, 0, 255, 0)) uses GPIO5 for DATA and GPIO0 for CLOCK and sets the first LED green, with the brightness 31 (out of 0-32)
// apa102.write(5, 6, string.char(255, 0, 0, 255):rep(10)) uses GPIO14 for DATA and GPIO12 for CLOCK and sets ten LED to red, with the brightness 31 (out of 0-32).
//                                                         Brightness values are clamped to 0-31.
static int apa102_write(lua_State* L) {
  uint8_t data_pin = luaL_checkinteger(L, 1);
  MOD_CHECK_ID(gpio, data_pin);
  uint32_t alt_data_pin = pin_num[data_pin];

  uint8_t clock_pin = luaL_checkinteger(L, 2);
  MOD_CHECK_ID(gpio, clock_pin);
  uint32_t alt_clock_pin = pin_num[clock_pin];

  size_t buf_len;
  const char *buf = luaL_checklstring(L, 3, &buf_len);
  uint32_t nbr_frames = buf_len / 4;

  if (nbr_frames > 100000) {
    return luaL_error(L, "The supplied buffer is too long, and might cause the callback watchdog to bark.");
  }

  // Initialize the output pins
  platform_gpio_mode(data_pin, PLATFORM_GPIO_OUTPUT, PLATFORM_GPIO_FLOAT);
  GPIO_OUTPUT_SET(alt_data_pin, PLATFORM_GPIO_HIGH); // Set pin high
  platform_gpio_mode(clock_pin, PLATFORM_GPIO_OUTPUT, PLATFORM_GPIO_FLOAT);
  GPIO_OUTPUT_SET(alt_clock_pin, PLATFORM_GPIO_LOW); // Set pin low

  // Send the buffers
  apa102_send_buffer(alt_data_pin, alt_clock_pin, (uint32_t *) buf, (uint32_t) nbr_frames);
  return 0;
}


const LUA_REG_TYPE apa102_map[] =
{
  { LSTRKEY( "write" ), LFUNCVAL( apa102_write )},
  { LNILKEY, LNILVAL}
};

LUALIB_API int luaopen_apa102(lua_State *L) {
  LREGISTER(L, "apa102", apa102_map);
  return 0;
}

NODEMCU_MODULE(APA102, "apa102", apa102_map, luaopen_apa102);
