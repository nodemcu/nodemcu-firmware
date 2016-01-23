#include "module.h"
#include "lauxlib.h"
#include "platform.h"
#include "c_stdlib.h"
#include "c_string.h"
#include "user_interface.h"
#include "driver/uart.h"

// Stream data using UART1 routed to GPIO2
// NODE_DEBUG should not be activated because it also uses UART1
static void ICACHE_RAM_ATTR ws2812_write(uint8_t pin, uint8_t *pixels, uint32_t length) {

  // Data are sent LSB first, with a start bit at 0, an end bit at 1 and all inverted
  // 0b00110111 => 110111 => [0]111011[1] => 10001000 => 00
  // 0b00000111 => 000111 => [0]111000[1] => 10001110 => 01
  // 0b00110100 => 110100 => [0]001011[1] => 11101000 => 10
  // 0b00000100 => 000100 => [0]001000[1] => 11101110 => 11
  uint8_t _uartData[4] = { 0b00110111, 0b00000111, 0b00110100, 0b00000100 };

  // Configure UART1
  // Set baudrate of UART1 to 3200000
  WRITE_PERI_REG(UART_CLKDIV(1), UART_CLK_FREQ / 3200000);
  // Set UART Configuration No parity / 6 DataBits / 1 StopBits / Invert TX
  WRITE_PERI_REG(UART_CONF0(1), UART_TXD_INV | (1 << UART_STOP_BIT_NUM_S) | (1 << UART_BIT_NUM_S));

  // Redirect UART1 to GPIO2
  // Disable GPIO2
  GPIO_REG_WRITE(GPIO_ENABLE_W1TC_ADDRESS, BIT2);
  // Enable Function 2 for GPIO2 (U1TXD)
  PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO2_U, FUNC_U1TXD_BK);

  uint8_t *end = pixels + length;
  do {
    uint8_t value = *pixels++;

    // Wait enough space in the FIFO buffer
    // (Less than 124 bytes in the buffer)
    while (((READ_PERI_REG(UART_STATUS(1)) >> UART_TXFIFO_CNT_S) & UART_TXFIFO_CNT) > 124);

    // Fill the buffer
    WRITE_PERI_REG(UART_FIFO(1), _uartData[(value >> 6) & 3]);
    WRITE_PERI_REG(UART_FIFO(1), _uartData[(value >> 4) & 3]);
    WRITE_PERI_REG(UART_FIFO(1), _uartData[(value >> 2) & 3]);
    WRITE_PERI_REG(UART_FIFO(1), _uartData[(value >> 0) & 3]);

  } while(pixels < end);
}

// Lua: ws2812.writergb(pin, "string")
// Byte triples in the string are interpreted as R G B values and sent to the hardware as G R B.
// WARNING: this function scrambles the input buffer :
//    a = string.char(255,0,128)
//    ws212.writergb(3,a)
//    =a.byte()
//    (0,255,128)

// ws2812.writergb(4, string.char(255, 0, 0)) uses GPIO2 and sets the first LED red.
// ws2812.writergb(3, string.char(0, 0, 255):rep(10)) uses GPIO0 and sets ten LEDs blue.
// ws2812.writergb(4, string.char(0, 255, 0, 255, 255, 255)) first LED green, second LED white.
static int ICACHE_FLASH_ATTR ws2812_writergb(lua_State* L)
{
  const uint8_t pin = luaL_checkinteger(L, 1);
  size_t length;
  const char *rgb = luaL_checklstring(L, 2, &length);

  // dont modify lua-internal lstring - make a copy instead
  char *buffer = (char *)c_malloc(length);
  c_memcpy(buffer, rgb, length);

  // Ignore incomplete Byte triples at the end of buffer:
  length -= length % 3;

  // Rearrange R G B values to G R B order needed by WS2812 LEDs:
  size_t i;
  for (i = 0; i < length; i += 3) {
    const char r = buffer[i];
    const char g = buffer[i + 1];
    buffer[i] = g;
    buffer[i + 1] = r;
  }

  // Send the buffer
  ws2812_write(pin_num[pin], (uint8_t*) buffer, length);

  c_free(buffer);

  return 0;
}

// Lua: ws2812.write(pin, "string")
// Byte triples in the string are interpreted as G R B values.
// This function does not corrupt your buffer.
//
// ws2812.write(4, string.char(0, 255, 0)) uses GPIO2 and sets the first LED red.
// ws2812.write(3, string.char(0, 0, 255):rep(10)) uses GPIO0 and sets ten LEDs blue.
// ws2812.write(4, string.char(255, 0, 0, 255, 255, 255)) first LED green, second LED white.
static int ICACHE_FLASH_ATTR ws2812_writegrb(lua_State* L) {
  const uint8_t pin = luaL_checkinteger(L, 1);
  size_t length;
  const char *buffer = luaL_checklstring(L, 2, &length);

  // Send the buffer
  ws2812_write(pin_num[pin], (uint8_t*) buffer, length);

  return 0;
}

static const LUA_REG_TYPE ws2812_map[] =
{
  { LSTRKEY( "writergb" ), LFUNCVAL( ws2812_writergb )},
  { LSTRKEY( "write" ), LFUNCVAL( ws2812_writegrb )},
  { LNILKEY, LNILVAL}
};

int luaopen_ws2812(lua_State *L) {
  // TODO: Make sure that the GPIO system is initialized
  return 0;
}

NODEMCU_MODULE(WS2812, "ws2812", ws2812_map, luaopen_ws2812);
