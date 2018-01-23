#include "module.h"
#include "lauxlib.h"
#include "lmem.h"
#include "platform.h"
#include "c_stdlib.h"
#include "c_math.h"
#include "c_string.h"
#include "user_interface.h"
#include "driver/uart.h"
#include "osapi.h"

#include "ws2812.h"

#define CANARY_VALUE 0x32383132
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

// Stream data using UART1 routed to GPIO2
// ws2812.init() should be called first
//
// NODE_DEBUG should not be activated because it also uses UART1
void ICACHE_RAM_ATTR ws2812_write_data(const uint8_t *pixels, uint32_t length, const uint8_t *pixels2, uint32_t length2) {

  // Data are sent LSB first, with a start bit at 0, an end bit at 1 and all inverted
  // 0b00110111 => 110111 => [0]111011[1] => 10001000 => 00
  // 0b00000111 => 000111 => [0]111000[1] => 10001110 => 01
  // 0b00110100 => 110100 => [0]001011[1] => 11101000 => 10
  // 0b00000100 => 000100 => [0]001000[1] => 11101110 => 11
  // Array declared as static const to avoid runtime generation
  // But declared in ".data" section to avoid read penalty from FLASH
  static const __attribute__((section(".data._uartData"))) uint8_t _uartData[4] = { 0b00110111, 0b00000111, 0b00110100, 0b00000100 };

  const uint8_t *end  = pixels + length;
  const uint8_t *end2 = pixels2 + length2;

  do {
    // If something to send for first buffer and enough room
    // in FIFO buffer (we wants to write 4 bytes, so less than
    // 124 in the buffer)
    if (pixels < end && (((READ_PERI_REG(UART_STATUS(1)) >> UART_TXFIFO_CNT_S) & UART_TXFIFO_CNT) <= 124)) {
      uint8_t value = *pixels++;

      // Fill the buffer
      WRITE_PERI_REG(UART_FIFO(1), _uartData[(value >> 6) & 3]);
      WRITE_PERI_REG(UART_FIFO(1), _uartData[(value >> 4) & 3]);
      WRITE_PERI_REG(UART_FIFO(1), _uartData[(value >> 2) & 3]);
      WRITE_PERI_REG(UART_FIFO(1), _uartData[(value >> 0) & 3]);
    }
    // Same for the second buffer
    if (pixels2 < end2 && (((READ_PERI_REG(UART_STATUS(0)) >> UART_TXFIFO_CNT_S) & UART_TXFIFO_CNT) <= 124)) {
      uint8_t value = *pixels2++;

      // Fill the buffer
      WRITE_PERI_REG(UART_FIFO(0), _uartData[(value >> 6) & 3]);
      WRITE_PERI_REG(UART_FIFO(0), _uartData[(value >> 4) & 3]);
      WRITE_PERI_REG(UART_FIFO(0), _uartData[(value >> 2) & 3]);
      WRITE_PERI_REG(UART_FIFO(0), _uartData[(value >> 0) & 3]);
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
  else if (type == LUA_TUSERDATA)
  {
    ws2812_buffer * buffer = (ws2812_buffer*)luaL_checkudata(L, 1, "ws2812.buffer");

    buffer1 = buffer->values;
    length1 = buffer->colorsPerLed*buffer->size;
  }
  else
  {
    luaL_argerror(L, 1, "ws2812.buffer or string expected");
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
  else if (type == LUA_TUSERDATA)
  {
    ws2812_buffer * buffer = (ws2812_buffer*)luaL_checkudata(L, 2, "ws2812.buffer");

    buffer2 = buffer->values;
    length2 = buffer->colorsPerLed*buffer->size;
  }
  else
  {
    luaL_argerror(L, 2, "ws2812.buffer or string expected");
  }

  // Send the buffers
  ws2812_write_data(buffer1, length1, buffer2, length2);

  return 0;
}

static ptrdiff_t posrelat (ptrdiff_t pos, size_t len) {
  /* relative string position: negative means back from end */
  if (pos < 0) pos += (ptrdiff_t)len + 1;
  return (pos >= 0) ? pos : 0;
}

static ws2812_buffer *allocate_buffer(lua_State *L, int leds, int colorsPerLed) {
  // Allocate memory
  size_t size = sizeof(ws2812_buffer) + colorsPerLed*leds*sizeof(uint8_t);
  ws2812_buffer * buffer = (ws2812_buffer*)lua_newuserdata(L, size);

  // Associate its metatable
  luaL_getmetatable(L, "ws2812.buffer");
  lua_setmetatable(L, -2);

  // Save led strip size
  buffer->size = leds;
  buffer->colorsPerLed = colorsPerLed;

  return buffer;
}


// Handle a buffer where we can store led values
static int ws2812_new_buffer(lua_State *L) {
  const int leds = luaL_checkint(L, 1);
  const int colorsPerLed = luaL_checkint(L, 2);

  luaL_argcheck(L, leds > 0, 1, "should be a positive integer");
  luaL_argcheck(L, colorsPerLed > 0, 2, "should be a positive integer");

  ws2812_buffer * buffer = allocate_buffer(L, leds, colorsPerLed);

  c_memset(buffer->values, 0, colorsPerLed * leds);

  return 1;
}


int ws2812_buffer_fill(ws2812_buffer * buffer, int * colors) {

  // Grab colors
  int i, j;

  // Fill buffer
  uint8_t * p = &buffer->values[0];
  for(i = 0; i < buffer->size; i++)
  {
    for (j = 0; j < buffer->colorsPerLed; j++)
    {
      *p++ = colors[j];
    }
  }

  return 0;
}

static int ws2812_buffer_fill_lua(lua_State* L) {
  ws2812_buffer * buffer = (ws2812_buffer*)luaL_checkudata(L, 1, "ws2812.buffer");

  // Grab colors
  int i;
  int * colors = luaM_malloc(L, buffer->colorsPerLed * sizeof(int));

  for (i = 0; i < buffer->colorsPerLed; i++)
  {
    colors[i] = luaL_checkinteger(L, 2+i);
  }

  ws2812_buffer_fill(buffer, colors);

  // Free memory
  luaM_free(L, colors);

  return 0;
}

static int ws2812_buffer_fade(lua_State* L) {
  ws2812_buffer * buffer = (ws2812_buffer*)luaL_checkudata(L, 1, "ws2812.buffer");
  const int fade = luaL_checkinteger(L, 2);
  unsigned direction = luaL_optinteger( L, 3, FADE_OUT );

  luaL_argcheck(L, fade > 0, 2, "fade value should be a strict positive number");

  uint8_t * p = &buffer->values[0];
  int val = 0;
  int i;
  for(i = 0; i < buffer->size * buffer->colorsPerLed; i++)
  {
    if (direction == FADE_OUT)
    {
      *p++ /= fade;
    }
    else
    {
      // as fade in can result in value overflow, an int is used to perform the check afterwards
      val = *p * fade;
      if (val > 255) val = 255;
      *p++ = val;
    }
  }

  return 0;
}


int ws2812_buffer_shift(ws2812_buffer * buffer, int shiftValue, unsigned shift_type, int pos_start, int pos_end) {

  ptrdiff_t start = posrelat(pos_start, buffer->size);
  ptrdiff_t end = posrelat(pos_end, buffer->size);
  if (start < 1) start = 1;
  if (end > (ptrdiff_t)buffer->size) end = (ptrdiff_t)buffer->size;

  start--;
  int size = end - start;
  size_t offset = start * buffer->colorsPerLed;

  //luaL_argcheck(L, shiftValue > 0-size && shiftValue < size, 2, "shifting more elements than buffer size");

  int shift = shiftValue >= 0 ? shiftValue : -shiftValue;

  // check if we want to shift at all
  if (shift == 0 || size <= 0)
  {
    return 0;
  }

  uint8_t * tmp_pixels = c_malloc(buffer->colorsPerLed * sizeof(uint8_t) * shift);
  int i,j;
  size_t shift_len, remaining_len;
  // calculate length of shift section and remaining section
  shift_len = shift*buffer->colorsPerLed;
  remaining_len = (size-shift)*buffer->colorsPerLed;

  if (shiftValue > 0)
  {
    // Store the values which are moved out of the array (last n pixels)
    c_memcpy(tmp_pixels, &buffer->values[offset + (size-shift)*buffer->colorsPerLed], shift_len);
    // Move pixels to end
    os_memmove(&buffer->values[offset + shift*buffer->colorsPerLed], &buffer->values[offset], remaining_len);
    // Fill beginning with temp data
    if (shift_type == SHIFT_LOGICAL)
    {
      c_memset(&buffer->values[offset], 0, shift_len);
    }
    else
    {
      c_memcpy(&buffer->values[offset], tmp_pixels, shift_len);
    }
  }
  else
  {
    // Store the values which are moved out of the array (last n pixels)
    c_memcpy(tmp_pixels, &buffer->values[offset], shift_len);
    // Move pixels to end
    os_memmove(&buffer->values[offset], &buffer->values[offset + shift*buffer->colorsPerLed], remaining_len);
    // Fill beginning with temp data
    if (shift_type == SHIFT_LOGICAL)
    {
      c_memset(&buffer->values[offset + (size-shift)*buffer->colorsPerLed], 0, shift_len);
    }
    else
    {
      c_memcpy(&buffer->values[offset + (size-shift)*buffer->colorsPerLed], tmp_pixels, shift_len);
    }
  }
  // Free memory
  c_free(tmp_pixels);

  return 0;
}


static int ws2812_buffer_shift_lua(lua_State* L) {

  ws2812_buffer * buffer = (ws2812_buffer*)luaL_checkudata(L, 1, "ws2812.buffer");
  const int shiftValue = luaL_checkinteger(L, 2);
  const unsigned shift_type = luaL_optinteger( L, 3, SHIFT_LOGICAL );

  const int pos_start = luaL_optinteger(L, 4, 1);
  const int pos_end = luaL_optinteger(L, 5, -1);


  ws2812_buffer_shift(buffer, shiftValue, shift_type, pos_start, pos_end);
  return 0;
}


static int ws2812_buffer_dump(lua_State* L) {
  ws2812_buffer * buffer = (ws2812_buffer*)luaL_checkudata(L, 1, "ws2812.buffer");

  lua_pushlstring(L, buffer->values, buffer->size * buffer->colorsPerLed);

  return 1;
}

static int ws2812_buffer_replace(lua_State* L) {
  ws2812_buffer * buffer = (ws2812_buffer*)luaL_checkudata(L, 1, "ws2812.buffer");
  size_t l = buffer->size;
  ptrdiff_t start = posrelat(luaL_optinteger(L, 3, 1), l);

  uint8_t *src;
  size_t srcLen;

  if (lua_type(L, 2) == LUA_TSTRING) {
    size_t length;

    src = (uint8_t *) lua_tolstring(L, 2, &length);
    srcLen = length / buffer->colorsPerLed;
  } else {
    ws2812_buffer * rhs = (ws2812_buffer*)luaL_checkudata(L, 2, "ws2812.buffer");
    src = rhs->values;
    srcLen = rhs->size;
    luaL_argcheck(L, rhs->colorsPerLed == buffer->colorsPerLed, 2, "Buffers have different colors");
  }

  luaL_argcheck(L, srcLen + start - 1 <= buffer->size, 2, "Does not fit into destination");

  c_memcpy(buffer->values + (start - 1) * buffer->colorsPerLed, src, srcLen * buffer->colorsPerLed);

  return 0;
}

// buffer:mix(factor1, buffer1, ..)
// factor is 256 for 100%
// uses saturating arithmetic (one buffer at a time)
static int ws2812_buffer_mix(lua_State* L) {
  ws2812_buffer * buffer = (ws2812_buffer*)luaL_checkudata(L, 1, "ws2812.buffer");

  int pos = 2;
  size_t cells = buffer->size * buffer->colorsPerLed;

  int n_sources = (lua_gettop(L) - 1) / 2;

  struct {
    int factor;
    const uint8_t *values;
  } source[n_sources];

  int src;
  for (src = 0; src < n_sources; src++, pos += 2) {
    int factor = luaL_checkinteger(L, pos);
    ws2812_buffer *src_buffer = (ws2812_buffer*) luaL_checkudata(L, pos + 1, "ws2812.buffer");

    luaL_argcheck(L, src_buffer->size == buffer->size && src_buffer->colorsPerLed == buffer->colorsPerLed, pos + 1, "Buffer not same shape");

    source[src].factor = factor;
    source[src].values = src_buffer->values;
  }

  size_t i;
  for (i = 0; i < cells; i++) {
    int32_t val = 0;
    for (src = 0; src < n_sources; src++) {
      val += (int32_t)(source[src].values[i] * source[src].factor);
    }

    val >>= 8;

    if (val < 0) {
      val = 0;
    } else if (val > 255) {
      val = 255;
    }
    buffer->values[i] = (uint8_t)val;
  }

  return 0;
}

// Returns the total of all channels
static int ws2812_buffer_power(lua_State* L) {
  ws2812_buffer * buffer = (ws2812_buffer*)luaL_checkudata(L, 1, "ws2812.buffer");

  size_t cells = buffer->size * buffer->colorsPerLed;

  size_t i;
  int total = 0;
  for (i = 0; i < cells; i++) {
    total += buffer->values[i];
  }

  lua_pushnumber(L, total);

  return 1;
}

static int ws2812_buffer_get(lua_State* L) {
  ws2812_buffer * buffer = (ws2812_buffer*)luaL_checkudata(L, 1, "ws2812.buffer");
  const int led = luaL_checkinteger(L, 2) - 1;

  luaL_argcheck(L, led >= 0 && led < buffer->size, 2, "index out of range");

  int i;
  for (i = 0; i < buffer->colorsPerLed; i++)
  {
    lua_pushnumber(L, buffer->values[buffer->colorsPerLed*led+i]);
  }

  return buffer->colorsPerLed;
}

static int ws2812_buffer_set(lua_State* L) {
  ws2812_buffer * buffer = (ws2812_buffer*)luaL_checkudata(L, 1, "ws2812.buffer");
  const int led = luaL_checkinteger(L, 2) - 1;

  luaL_argcheck(L, led >= 0 && led < buffer->size, 2, "index out of range");

  int type = lua_type(L, 3);
  if(type == LUA_TTABLE)
  {
    int i;
    for (i = 0; i < buffer->colorsPerLed; i++)
    {
      // Get value and push it on stack
      lua_rawgeti(L, 3, i+1);

      // Convert it as int and store them in buffer
      buffer->values[buffer->colorsPerLed*led+i] = lua_tonumber(L, -1);
    }

    // Clean up the stack
    lua_pop(L, buffer->colorsPerLed);
  }
  else if(type == LUA_TSTRING)
  {
    size_t len;
    const char * buf = lua_tolstring(L, 3, &len);

    // Overflow check
    if( buffer->colorsPerLed*led + len > buffer->colorsPerLed*buffer->size )
    {
	return luaL_error(L, "string size will exceed strip length");
    }

    c_memcpy(&buffer->values[buffer->colorsPerLed*led], buf, len);
  }
  else
  {
    int i;
    for (i = 0; i < buffer->colorsPerLed; i++)
    {
      buffer->values[buffer->colorsPerLed*led+i] = luaL_checkinteger(L, 3+i);
    }
  }

  return 0;
}

static int ws2812_buffer_size(lua_State* L) {
  ws2812_buffer * buffer = (ws2812_buffer*)luaL_checkudata(L, 1, "ws2812.buffer");

  lua_pushnumber(L, buffer->size);

  return 1;
}

static int ws2812_buffer_sub(lua_State* L) {
  ws2812_buffer * lhs = (ws2812_buffer*)luaL_checkudata(L, 1, "ws2812.buffer");
  size_t l = lhs->size;
  ptrdiff_t start = posrelat(luaL_checkinteger(L, 2), l);
  ptrdiff_t end = posrelat(luaL_optinteger(L, 3, -1), l);
  if (start < 1) start = 1;
  if (end > (ptrdiff_t)l) end = (ptrdiff_t)l;
  if (start <= end) {
    ws2812_buffer *result = allocate_buffer(L, end - start + 1, lhs->colorsPerLed);
    c_memcpy(result->values, lhs->values + lhs->colorsPerLed * (start - 1), lhs->colorsPerLed * (end - start + 1));
  } else {
    ws2812_buffer *result = allocate_buffer(L, 0, lhs->colorsPerLed);
  }
  return 1;
}

static int ws2812_buffer_concat(lua_State* L) {
  ws2812_buffer * lhs = (ws2812_buffer*)luaL_checkudata(L, 1, "ws2812.buffer");
  ws2812_buffer * rhs = (ws2812_buffer*)luaL_checkudata(L, 2, "ws2812.buffer");

  luaL_argcheck(L, lhs->colorsPerLed == rhs->colorsPerLed, 1, "Can only concatenate buffers with same colors");

  int colorsPerLed = lhs->colorsPerLed;
  int leds = lhs->size + rhs->size;

  ws2812_buffer * buffer = allocate_buffer(L, leds, colorsPerLed);

  c_memcpy(buffer->values, lhs->values, lhs->colorsPerLed * lhs->size);
  c_memcpy(buffer->values + lhs->colorsPerLed * lhs->size, rhs->values, rhs->colorsPerLed * rhs->size);

  return 1;
}

static int ws2812_buffer_tostring(lua_State* L) {
  ws2812_buffer * buffer = (ws2812_buffer*)luaL_checkudata(L, 1, "ws2812.buffer");

  luaL_Buffer result;
  luaL_buffinit(L, &result);

  luaL_addchar(&result, '[');
  int i;
  int p = 0;
  for (i = 0; i < buffer->size; i++) {
    int j;
    if (i > 0) {
      luaL_addchar(&result, ',');
    }
    luaL_addchar(&result, '(');
    for (j = 0; j < buffer->colorsPerLed; j++, p++) {
      if (j > 0) {
        luaL_addchar(&result, ',');
      }
      char numbuf[5];
      c_sprintf(numbuf, "%d", buffer->values[p]);
      luaL_addstring(&result, numbuf);
    }
    luaL_addchar(&result, ')');
  }

  luaL_addchar(&result, ']');
  luaL_pushresult(&result);

  return 1;
}


static const LUA_REG_TYPE ws2812_buffer_map[] =
{
  { LSTRKEY( "dump" ),    LFUNCVAL( ws2812_buffer_dump )},
  { LSTRKEY( "fade" ),    LFUNCVAL( ws2812_buffer_fade )},
  { LSTRKEY( "fill" ),    LFUNCVAL( ws2812_buffer_fill_lua )},
  { LSTRKEY( "get" ),     LFUNCVAL( ws2812_buffer_get )},
  { LSTRKEY( "replace" ), LFUNCVAL( ws2812_buffer_replace )},
  { LSTRKEY( "mix" ),     LFUNCVAL( ws2812_buffer_mix )},
  { LSTRKEY( "power" ),   LFUNCVAL( ws2812_buffer_power )},
  { LSTRKEY( "set" ),     LFUNCVAL( ws2812_buffer_set )},
  { LSTRKEY( "shift" ),   LFUNCVAL( ws2812_buffer_shift_lua )},
  { LSTRKEY( "size" ),    LFUNCVAL( ws2812_buffer_size )},
  { LSTRKEY( "sub" ),     LFUNCVAL( ws2812_buffer_sub )},
  { LSTRKEY( "__concat" ),LFUNCVAL( ws2812_buffer_concat )},
  { LSTRKEY( "__index" ), LROVAL( ws2812_buffer_map )},
  { LSTRKEY( "__tostring" ), LFUNCVAL( ws2812_buffer_tostring )},
  { LNILKEY, LNILVAL}
};


static const LUA_REG_TYPE ws2812_map[] =
{
  { LSTRKEY( "init" ),           LFUNCVAL( ws2812_init )},
  { LSTRKEY( "newBuffer" ),      LFUNCVAL( ws2812_new_buffer )},
  { LSTRKEY( "write" ),          LFUNCVAL( ws2812_write )},
  { LSTRKEY( "FADE_IN" ),        LNUMVAL( FADE_IN ) },
  { LSTRKEY( "FADE_OUT" ),       LNUMVAL( FADE_OUT ) },
  { LSTRKEY( "MODE_SINGLE" ),    LNUMVAL( MODE_SINGLE ) },
  { LSTRKEY( "MODE_DUAL" ),      LNUMVAL( MODE_DUAL ) },
  { LSTRKEY( "SHIFT_LOGICAL" ),  LNUMVAL( SHIFT_LOGICAL ) },
  { LSTRKEY( "SHIFT_CIRCULAR" ), LNUMVAL( SHIFT_CIRCULAR ) },
  { LNILKEY, LNILVAL}
};

int luaopen_ws2812(lua_State *L) {
  // TODO: Make sure that the GPIO system is initialized
  luaL_rometatable(L, "ws2812.buffer", (void *)ws2812_buffer_map);  // create metatable for ws2812.buffer
  return 0;
}

NODEMCU_MODULE(WS2812, "ws2812", ws2812_map, luaopen_ws2812);
