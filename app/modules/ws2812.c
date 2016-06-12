#include "module.h"
#include "lauxlib.h"
#include "lmem.h"
#include "platform.h"
#include "c_stdlib.h"
#include "c_string.h"
#include "user_interface.h"
#include "driver/uart.h"
#include "osapi.h"
#include "sinus.h"

#define CANARY_VALUE 0x32383132

#define FADE_IN  1
#define FADE_OUT 0
#define SHIFT_LOGICAL  0
#define SHIFT_CIRCULAR 1

typedef struct {
  int canary;
  int size;
  uint8_t colorsPerLed;
  uint8_t values[0];
} ws2812_buffer;

void hsi2grbw(int H, int S, int I, int* grbw, int use_rgbw, int swap_gr) {
  int idx2, idx1, idx0;
      
  if(H < 120) {
    idx0 = 1;
    idx1 = 0;
    idx2 = 2;
  } else if(H < 240) {
    H = H - 120;
    idx0 = 0;
    idx1 = 2;
    idx2 = 1;
  } else {
    H = H - 240;
    idx0 = 2;
    idx1 = 1;
    idx2 = 0;
  }
/*
   if(swap_gr){
	idx1 ^= idx2;
	idx2 ^= idx1;
	idx1 ^= idx2;	
   }
*/

    if(!use_rgbw)
    {
      grbw[idx0] = (I+SIcosH_cos_H(H,S,I));
      grbw[idx1] = (I+(((S*I)>>10)-SIcosH_cos_H(H,S,I)));
      grbw[idx2] = (I-((I*S)>>10));
    }
    else
    {
      grbw[idx0] = (((S*I)>>10)+SIcosH_cos_H(H,S,I))/12;
      grbw[idx1] = (((S*I)>>10)+(((S*I)>>10)-SIcosH_cos_H(H,S,I)))/12;
      grbw[idx2] = 0;
      grbw[3] = ((1023-S)*I)>>12;
    }
}

// Init UART1 to be able to stream WS2812 data
// We use GPIO2 as output pin
static void ws2812_init() {
  // Configure UART1
  // Set baudrate of UART1 to 3200000
  WRITE_PERI_REG(UART_CLKDIV(1), UART_CLK_FREQ / 3200000);
  // Set UART Configuration No parity / 6 DataBits / 1 StopBits / Invert TX
  WRITE_PERI_REG(UART_CONF0(1), UART_TXD_INV | (1 << UART_STOP_BIT_NUM_S) | (1 << UART_BIT_NUM_S));

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
}


// Stream data using UART1 routed to GPIO2
// ws2812.init() should be called first
//
// NODE_DEBUG should not be activated because it also uses UART1
static void ICACHE_RAM_ATTR ws2812_write(uint8_t *pixels, uint32_t length) {

  // Data are sent LSB first, with a start bit at 0, an end bit at 1 and all inverted
  // 0b00110111 => 110111 => [0]111011[1] => 10001000 => 00
  // 0b00000111 => 000111 => [0]111000[1] => 10001110 => 01
  // 0b00110100 => 110100 => [0]001011[1] => 11101000 => 10
  // 0b00000100 => 000100 => [0]001000[1] => 11101110 => 11
  // Array declared as static const to avoid runtime generation
  // But declared in ".data" section to avoid read penalty from FLASH
  static const __attribute__((section(".data._uartData"))) uint8_t _uartData[4] = { 0b00110111, 0b00000111, 0b00110100, 0b00000100 };

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

// Lua: ws2812.write("string")
// Byte triples in the string are interpreted as G R B values.
//
// ws2812.init() should be called first
//
// ws2812.write(string.char(0, 255, 0)) sets the first LED red.
// ws2812.write(string.char(0, 0, 255):rep(10)) sets ten LEDs blue.
// ws2812.write(string.char(255, 0, 0, 255, 255, 255)) first LED green, second LED white.
static int ws2812_writegrb(lua_State* L) {
  size_t length;
  const char *values = luaL_checklstring(L, 1, &length);

  // Send the buffer
  ws2812_write((uint8_t*) values, length);

  return 0;
}

// Handle a buffer where we can store led values
static int ws2812_new_buffer(lua_State *L) {
  const int leds = luaL_checkint(L, 1);
  const int colorsPerLed = luaL_checkint(L, 2);

  luaL_argcheck(L, leds > 0, 1, "should be a positive integer");
  luaL_argcheck(L, colorsPerLed > 0, 2, "should be a positive integer");

  // Allocate memory
  size_t size = sizeof(ws2812_buffer) + colorsPerLed*leds*sizeof(uint8_t);
  ws2812_buffer * buffer = (ws2812_buffer*)lua_newuserdata(L, size);

  // Associate its metatable
  luaL_getmetatable(L, "ws2812.buffer");
  lua_setmetatable(L, -2);

  // Save led strip size
  buffer->size = leds;
  buffer->colorsPerLed = colorsPerLed;

  // Store canary for future type checks
  buffer->canary = CANARY_VALUE;

  return 1;
}

static int ws2812_buffer_fill(lua_State* L) {
  ws2812_buffer * buffer = (ws2812_buffer*)lua_touserdata(L, 1);

  luaL_argcheck(L, buffer && buffer->canary == CANARY_VALUE, 1, "ws2812.buffer expected");

  // Grab colors
  int i, j;
  int * colors = luaM_malloc(L, buffer->colorsPerLed * sizeof(int));

  for (i = 0; i < buffer->colorsPerLed; i++)
  {
    colors[i] = luaL_checkinteger(L, 2+i);
  }

  // Fill buffer
  uint8_t * p = &buffer->values[0];
  for(i = 0; i < buffer->size; i++)
  {
    for (j = 0; j < buffer->colorsPerLed; j++)
    {
      *p++ = colors[j];
    }
  }

  // Free memory
  luaM_free(L, colors);

  return 0;
}

static int ws2812_buffer_fillHSI(lua_State* L) {
  ws2812_buffer * buffer = (ws2812_buffer*)lua_touserdata(L, 1);
  luaL_argcheck(L, buffer && buffer->canary == CANARY_VALUE, 1, "ws2812.buffer expected");
  luaL_checktype(L, 2, LUA_TTABLE);

  int i, j;
  int hsi_in[3];
  int grbw_out[4];
    
  for(j = 0; j < buffer->size; j++)
  {
    lua_pushinteger(L, j+1);
    lua_gettable(L, 2);      
    if (lua_istable(L, -1))
    {
      for(i = 1; i <= 3; i++)
      {
        lua_pushinteger(L, i);
        lua_gettable(L, -2);
        hsi_in[i-1] = luaL_checknumber(L, -1);
        lua_pop(L, 1);
      }       
      hsi2grbw(hsi_in[0], hsi_in[1], hsi_in[2], grbw_out, (buffer->colorsPerLed>3)?1:0, 1); 
    }
  
    if(buffer->colorsPerLed == 3)
    {
      buffer->values[3*j+0] = grbw_out[0];
      buffer->values[3*j+1] = grbw_out[1];
      buffer->values[3*j+2] = grbw_out[2];
    }
    else if(buffer->colorsPerLed == 4)    
    {
      buffer->values[4*j+0] = grbw_out[0];
      buffer->values[4*j+1] = grbw_out[1];
      buffer->values[4*j+2] = grbw_out[2];
      buffer->values[4*j+3] = grbw_out[3];
    }
    lua_pop(L, 1); 
  }    

  return 0;
}

static int ws2812_buffer_fade(lua_State* L) {
  ws2812_buffer * buffer = (ws2812_buffer*)lua_touserdata(L, 1);
  const int fade = luaL_checkinteger(L, 2);
  unsigned direction = luaL_optinteger( L, 3, FADE_OUT );

  luaL_argcheck(L, buffer && buffer->canary == CANARY_VALUE, 1, "ws2812.buffer expected");
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


static int ws2812_buffer_shift(lua_State* L) {
  ws2812_buffer * buffer = (ws2812_buffer*)lua_touserdata(L, 1);
  const int shiftValue = luaL_checkinteger(L, 2);
  const unsigned shift_type = luaL_optinteger( L, 3, SHIFT_LOGICAL );

  luaL_argcheck(L, buffer && buffer->canary == CANARY_VALUE, 1, "ws2812.buffer expected");
  luaL_argcheck(L, shiftValue > 0-buffer->size && shiftValue < buffer->size, 2, "shifting more elements than buffer size");

  int shift = shiftValue >= 0 ? shiftValue : -shiftValue;
    
  // check if we want to shift at all
  if (shift == 0)
  {
    return 0;
  }
  
  uint8_t * tmp_pixels = luaM_malloc(L, buffer->colorsPerLed * sizeof(uint8_t) * shift);
  int i,j;
  size_t shift_len, remaining_len;
  // calculate length of shift section and remaining section
  shift_len = shift*buffer->colorsPerLed;
  remaining_len = (buffer->size-shift)*buffer->colorsPerLed;
  
  if (shiftValue > 0) 
  {
    // Store the values which are moved out of the array (last n pixels)
    c_memcpy(tmp_pixels, &buffer->values[(buffer->size-shift)*buffer->colorsPerLed], shift_len);
    // Move pixels to end
    os_memmove(&buffer->values[shift*buffer->colorsPerLed], &buffer->values[0], remaining_len);
    // Fill beginning with temp data
    if (shift_type == SHIFT_LOGICAL)
    {
      c_memset(&buffer->values[0], 0, shift_len);
    }
    else
    {
      c_memcpy(&buffer->values[0], tmp_pixels, shift_len);
    }
  }
  else
  {
    // Store the values which are moved out of the array (last n pixels)
    c_memcpy(tmp_pixels, &buffer->values[0], shift_len);
    // Move pixels to end
    os_memmove(&buffer->values[0], &buffer->values[shift*buffer->colorsPerLed], remaining_len);
    // Fill beginning with temp data
    if (shift_type == SHIFT_LOGICAL)
    {
      c_memset(&buffer->values[(buffer->size-shift)*buffer->colorsPerLed], 0, shift_len);
    }
    else
    {
      c_memcpy(&buffer->values[(buffer->size-shift)*buffer->colorsPerLed], tmp_pixels, shift_len);
    }
  }
  // Free memory
  luaM_free(L, tmp_pixels);

  return 0;
}



static int ws2812_buffer_get(lua_State* L) {
  ws2812_buffer * buffer = (ws2812_buffer*)lua_touserdata(L, 1);
  const int led = luaL_checkinteger(L, 2) - 1;

  luaL_argcheck(L, buffer && buffer->canary == CANARY_VALUE, 1, "ws2812.buffer expected");
  luaL_argcheck(L, led >= 0 && led < buffer->size, 2, "index out of range");

  int i;
  for (i = 0; i < buffer->colorsPerLed; i++)
  {
    lua_pushnumber(L, buffer->values[buffer->colorsPerLed*led+i]);
  }

  return buffer->colorsPerLed;
}

static int ws2812_buffer_set(lua_State* L) {
  ws2812_buffer * buffer = (ws2812_buffer*)lua_touserdata(L, 1);
  const int led = luaL_checkinteger(L, 2) - 1;

  luaL_argcheck(L, buffer && buffer->canary == CANARY_VALUE, 1, "ws2812.buffer expected");
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
  ws2812_buffer * buffer = (ws2812_buffer*)lua_touserdata(L, 1);

  luaL_argcheck(L, buffer && buffer->canary == CANARY_VALUE, 1, "ws2812.buffer expected");

  lua_pushnumber(L, buffer->size);

  return 1;
}

static int ws2812_buffer_write(lua_State* L) {
  ws2812_buffer * buffer = (ws2812_buffer*)lua_touserdata(L, 1);

  luaL_argcheck(L, buffer && buffer->canary == CANARY_VALUE, 1, "ws2812.buffer expected");

  // Send the buffer
  ws2812_write(buffer->values, buffer->colorsPerLed*buffer->size);

  return 0;
}

static const LUA_REG_TYPE ws2812_buffer_map[] =
{
  { LSTRKEY( "fade" ),    LFUNCVAL( ws2812_buffer_fade )},
  { LSTRKEY( "shift" ),  LFUNCVAL( ws2812_buffer_shift )},
  { LSTRKEY( "fill" ),    LFUNCVAL( ws2812_buffer_fill )},
  { LSTRKEY( "fillHSI" ),  LFUNCVAL( ws2812_buffer_fillHSI )},
  { LSTRKEY( "get" ),     LFUNCVAL( ws2812_buffer_get )},
  { LSTRKEY( "set" ),     LFUNCVAL( ws2812_buffer_set )},
  { LSTRKEY( "size" ),    LFUNCVAL( ws2812_buffer_size )},
  { LSTRKEY( "write" ),   LFUNCVAL( ws2812_buffer_write )},
  { LSTRKEY( "__index" ), LROVAL ( ws2812_buffer_map )},
  { LNILKEY, LNILVAL}
};

static const LUA_REG_TYPE ws2812_map[] =
{
  { LSTRKEY( "write" ), LFUNCVAL( ws2812_writegrb )},
  { LSTRKEY( "newBuffer" ), LFUNCVAL( ws2812_new_buffer )},
  { LSTRKEY( "init" ),     LFUNCVAL( ws2812_init )},
  { LSTRKEY( "FADE_IN" ), LNUMVAL( FADE_IN ) },
  { LSTRKEY( "FADE_OUT" ),LNUMVAL( FADE_OUT ) },
  { LSTRKEY( "SHIFT_LOGICAL" ),LNUMVAL( SHIFT_LOGICAL ) },
  { LSTRKEY( "SHIFT_CIRCULAR" ),LNUMVAL( SHIFT_CIRCULAR ) },
  { LNILKEY, LNILVAL}
};

int luaopen_ws2812(lua_State *L) {
  // TODO: Make sure that the GPIO system is initialized
  luaL_rometatable(L, "ws2812.buffer", (void *)ws2812_buffer_map);  // create metatable for ws2812.buffer
  return 0;
}

NODEMCU_MODULE(WS2812, "ws2812", ws2812_map, luaopen_ws2812);
