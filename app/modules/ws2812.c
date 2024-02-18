#include "module.h"
#include "lauxlib.h"
#include "lmem.h"
#include "platform.h"
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "user_interface.h"
#include "driver/uart.h"
#include "osapi.h"
#include "cpu_esp8266_irq.h"

#include "pixbuf.h"

#define MODE_SINGLE  0
#define MODE_DUAL    1

// Init UART1 to be able to stream WS2812 data to GPIO2 pin
// If DUAL mode is selected, init UART0 to stream to TXD0 as well
// You HAVE to redirect LUA's output somewhere else
static int ws2812_init(lua_State* L) {
  const int mode = luaL_optinteger(L, 1, MODE_SINGLE);
  luaL_argcheck(L, mode == MODE_SINGLE || mode == MODE_DUAL, 1, "ws2812.SINGLE or ws2812.DUAL expected");

  // Configure UART1
  // Set baudrate of UART1 to 3200000
  WRITE_PERI_REG(UART_CLKDIV(1), UART_CLK_FREQ / 3200000);
  // Set UART Configuration No parity / 6 DataBits / 1 StopBits / Invert TX
  WRITE_PERI_REG(UART_CONF0(1), UART_TXD_INV | (1 << UART_STOP_BIT_NUM_S) | (1 << UART_BIT_NUM_S));

  if (mode == MODE_DUAL) {
    // Configure UART0
    // Set baudrate of UART0 to 3200000
    WRITE_PERI_REG(UART_CLKDIV(0), UART_CLK_FREQ / 3200000);
    // Set UART Configuration No parity / 6 DataBits / 1 StopBits / Invert TX
    WRITE_PERI_REG(UART_CONF0(0), UART_TXD_INV | (1 << UART_STOP_BIT_NUM_S) | (1 << UART_BIT_NUM_S));
  }

  // Pull GPIO2 down
  platform_gpio_mode(4, PLATFORM_GPIO_OUTPUT, PLATFORM_GPIO_FLOAT);
  platform_gpio_write(4, 0);

  // Waits 10us to simulate a reset
  os_delay_us(10);

  // Redirect UART1 to GPIO2
  // Disable GPIO2
  GPIO_REG_WRITE(GPIO_ENABLE_W1TC_ADDRESS, BIT2);
  // Enable Function 2 for GPIO2 (U1TXD)
  PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO2_U, FUNC_U1TXD_BK);

  return 0;
}

static bool
ws2812_can_write(int uart)
{
  // If something to send for first buffer and enough room
  // in FIFO buffer (we wants to write 4 bytes, so less than
  // 124 in the buffer)
  return (((READ_PERI_REG(UART_STATUS(uart)) >> UART_TXFIFO_CNT_S) & UART_TXFIFO_CNT) <= 124);
}

static void
ws2812_write_byte(int uart, uint8_t value)
{
  // Data are sent LSB first, with a start bit at 0, an end bit at 1 and all inverted
  // 0b00110111 => 110111 => [0]111011[1] => 10001000 => 00
  // 0b00000111 => 000111 => [0]111000[1] => 10001110 => 01
  // 0b00110100 => 110100 => [0]001011[1] => 11101000 => 10
  // 0b00000100 => 000100 => [0]001000[1] => 11101110 => 11
  // Array declared as static const to avoid runtime generation
  // But declared in ".data" section to avoid read penalty from FLASH
  static const __attribute__((section(".data._uartData"))) uint8_t _uartData[4] = { 0b00110111, 0b00000111, 0b00110100, 0b00000100 };

  WRITE_PERI_REG(UART_FIFO(uart), _uartData[(value >> 6) & 3]);
  WRITE_PERI_REG(UART_FIFO(uart), _uartData[(value >> 4) & 3]);
  WRITE_PERI_REG(UART_FIFO(uart), _uartData[(value >> 2) & 3]);
  WRITE_PERI_REG(UART_FIFO(uart), _uartData[(value >> 0) & 3]);
}

// Stream data using UART1 routed to GPIO2
// ws2812.init() should be called first
//
// NODE_DEBUG should not be activated because it also uses UART1
void ICACHE_RAM_ATTR ws2812_write_data(const uint8_t *pixels, uint32_t length, const uint8_t *pixels2, uint32_t length2) {
  const uint8_t *end  = pixels + length;
  const uint8_t *end2 = pixels2 + length2;

  /* Fill the UART fifos with IRQs disabled */
  uint32_t irq_state = esp8266_defer_irqs();
  while ((pixels < end) && ws2812_can_write(1)) {
    ws2812_write_byte(1, *pixels++);
  }
  while ((pixels2 < end2) && ws2812_can_write(0)) {
    ws2812_write_byte(0, *pixels2++);
  }
  esp8266_restore_irqs(irq_state);

  do {
    if (pixels < end && ws2812_can_write(1)) {
      ws2812_write_byte(1, *pixels++);
    }
    // Same for the second buffer
    if (pixels2 < end2 && ws2812_can_write(0)) {
      ws2812_write_byte(0, *pixels2++);
    }
  } while(pixels < end || pixels2 < end2); // Until there is still something to send
}

// Lua: ws2812.write("string")
// Byte triples in the string are interpreted as G R B values.
//
// ws2812.init() should be called first
//
// ws2812.write(string.char(0, 255, 0)) sets the first LED red.
// ws2812.write(string.char(0, 0, 255):rep(10)) sets ten LEDs blue.
// ws2812.write(string.char(255, 0, 0, 255, 255, 255)) first LED green, second LED white.
//
// In DUAL mode 'ws2812.init(ws2812.DUAL)', you may pass a second string as parameter
// It will be sent through TXD0 in parallel
static int ws2812_write(lua_State* L) {
  size_t length1, length2;
  const char *buffer1, *buffer2;

  // First mandatory parameter
  int type = lua_type(L, 1);
  if (type == LUA_TNIL)
  {
    buffer1 = 0;
    length1 = 0;
  }
  else if(type == LUA_TSTRING)
  {
    buffer1 = lua_tolstring(L, 1, &length1);
  }
#ifdef LUA_USE_MODULES_PIXBUF      
  else if (type == LUA_TUSERDATA)
  {
    pixbuf *buffer = pixbuf_from_lua_arg(L, 1);
    luaL_argcheck(L, pixbuf_channels(buffer) == 3, 1, "Bad pixbuf format");
    buffer1 = buffer->values;
    length1 = pixbuf_size(buffer);
  }
#endif
  else
  {
    luaL_argerror(L, 1, "pixbuf or string expected");
  }

  // Second optionnal parameter
  type = lua_type(L, 2);
  if (type == LUA_TNONE || type == LUA_TNIL)
  {
    buffer2 = 0;
    length2 = 0;
  }
  else if (type == LUA_TSTRING)
  {
    buffer2 = lua_tolstring(L, 2, &length2);
  }
#ifdef LUA_USE_MODULES_PIXBUF      
  else if (type == LUA_TUSERDATA)
  {
    pixbuf *buffer = pixbuf_from_lua_arg(L, 2);
    luaL_argcheck(L, pixbuf_channels(buffer) == 3, 2, "Bad pixbuf format");
    buffer2 = buffer->values;
    length2 = pixbuf_size(buffer);
  }
#endif
  else
  {
    luaL_argerror(L, 2, "pixbuf or string expected");
  }

  // Send the buffers
  ws2812_write_data(buffer1, length1, buffer2, length2);

  return 0;
}

LROT_BEGIN(ws2812, NULL, 0)
  LROT_FUNCENTRY( init, ws2812_init )
#ifdef LUA_USE_MODULES_PIXBUF      
  LROT_FUNCENTRY( newBuffer, pixbuf_new_lua ) // backwards compatibility
  LROT_NUMENTRY( FADE_IN, PIXBUF_FADE_IN )    // BC
  LROT_NUMENTRY( FADE_OUT, PIXBUF_FADE_OUT )  // BC
  LROT_NUMENTRY( SHIFT_LOGICAL, PIXBUF_SHIFT_LOGICAL ) // BC
  LROT_NUMENTRY( SHIFT_CIRCULAR, PIXBUF_SHIFT_CIRCULAR ) // BC
#endif
  LROT_FUNCENTRY( write, ws2812_write )
  LROT_NUMENTRY( MODE_SINGLE, MODE_SINGLE )
  LROT_NUMENTRY( MODE_DUAL, MODE_DUAL )
LROT_END(ws2812, NULL, 0)

static int luaopen_ws2812(lua_State *L) {
  // TODO: Make sure that the GPIO system is initialized
  return 0;
}

NODEMCU_MODULE(WS2812, "ws2812", ws2812, luaopen_ws2812);
