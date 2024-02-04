
#include "module.h"
#include "lauxlib.h"
#include "lmem.h"
#include "platform.h"

#include <string.h>

#define FADE_IN  1
#define FADE_OUT 0
#define SHIFT_LOGICAL  0
#define SHIFT_CIRCULAR 1

// The default bit H & L durations in multiples of 100ns.
#define WS2812_DURATION_T0H 4
#define WS2812_DURATION_T0L 7
#define WS2812_DURATION_T1H 8
#define WS2812_DURATION_T1L 6
// The default reset duration in multiples of 100ns.
#define WS2812_DURATION_RESET 512


typedef struct {
  int size;
  uint8_t colorsPerLed;
  uint8_t values[0];
} ws2812_buffer;

static void ws2812_cleanup( lua_State *L, int pop )
{
  if (pop)
    lua_pop( L, pop );
  platform_ws2812_release();
}

// Lua: ws2812.write("string")
// Byte triples in the string are interpreted as G R B values.
//
// ws2812.write({pin = 4, data = string.char(0, 255, 0)}) sets the first LED red.
// ws2812.write({pin = 4, data = string.char(0, 0, 255):rep(10)}) sets ten LEDs blue.
// ws2812.write({pin = 4, data = string.char(255, 0, 0, 255, 255, 255)}) first LED green, second LED white.
static int ws2812_write( lua_State* L )
{
  int type;
  int top = lua_gettop( L );

  for (int stack = 1; stack <= top; stack++) {
    if (lua_type( L, stack ) == LUA_TNIL)
      continue;

    if (lua_type( L, stack ) != LUA_TTABLE) {
      ws2812_cleanup( L, 0 );
      luaL_checktype( L, stack, LUA_TTABLE ); // trigger error
      return 0;
    }

    //
    // retrieve pin
    //
    lua_getfield( L, stack, "pin" );
    if (!lua_isnumber( L, -1 )) {
      ws2812_cleanup( L, 1 );
      return luaL_argerror( L, stack, "invalid pin" );
    }
    int gpio_num = luaL_checkint( L, -1 );
    lua_pop( L, 1 );

    //
    // retrieve reset
    // This is an optional parameter which defaults to WS2812_DURATION_RESET.
    //
    int reset = WS2812_DURATION_RESET;
    type = lua_getfield( L, stack, "reset" );
    if (type!=LUA_TNIL )
    {
      if (!lua_isnumber( L, -1 )) {
        ws2812_cleanup( L, 1 );
        return luaL_argerror( L, stack, "invalid reset" );
      }
      reset = luaL_checkint( L, -1 );
      if ((reset<0) || (reset>0xfffe)) {
        ws2812_cleanup( L, 1 );
        return luaL_argerror( L, stack, "reset must be 0<=reset<=65534" );
      }
    }
    lua_pop( L, 1 );

    //
    // retrieve t0h
    // This is an optional parameter which defaults to WS2812_DURATION_T0H.
    //
    int t0h = WS2812_DURATION_T0H;
    type = lua_getfield( L, stack, "t0h" );
    if (type!=LUA_TNIL )
    {
      if (!lua_isnumber( L, -1 )) {
        ws2812_cleanup( L, 1 );
        return luaL_argerror( L, stack, "invalid t0h" );
      }
      t0h = luaL_checkint( L, -1 );
      if ((t0h<1) || (t0h>0x7fff)) {
        ws2812_cleanup( L, 1 );
        return luaL_argerror( L, stack, "t0h must be 1<=t0h<=32767" );
      }
    }
    lua_pop( L, 1 );

    //
    // retrieve t0l
    // This is an optional parameter which defaults to WS2812_DURATION_T0L.
    //
    int t0l = WS2812_DURATION_T0L;
    type = lua_getfield( L, stack, "t0l" );
    if (type!=LUA_TNIL )
    {
      if (!lua_isnumber( L, -1 )) {
        ws2812_cleanup( L, 1 );
        return luaL_argerror( L, stack, "invalid t0l" );
      }
      t0l = luaL_checkint( L, -1 );
      if ((t0l<1) || (t0l>0x7fff)) {
        ws2812_cleanup( L, 1 );
        return luaL_argerror( L, stack, "t0l must be 1<=t0l<=32767" );
      }
    }
    lua_pop( L, 1 );

    //
    // retrieve t1h
    // This is an optional parameter which defaults to WS2812_DURATION_T1H.
    //
    int t1h = WS2812_DURATION_T1H;
    type = lua_getfield( L, stack, "t1h" );
    if (type!=LUA_TNIL )
    {
      if (!lua_isnumber( L, -1 )) {
        ws2812_cleanup( L, 1 );
        return luaL_argerror( L, stack, "invalid t1h" );
      }
      t1h = luaL_checkint( L, -1 );
      if ((t1h<1) || (t1h>0x7fff)) {
        ws2812_cleanup( L, 1 );
        return luaL_argerror( L, stack, "t1h must be 1<=t1h<=32767" );
      }
    }
    lua_pop( L, 1 );

    //
    // retrieve t1l
    // This is an optional parameter which defaults to WS2812_DURATION_T1L.
    //
    int t1l = WS2812_DURATION_T1L;
    type = lua_getfield( L, stack, "t1l" );
    if (type!=LUA_TNIL )
    {
      if (!lua_isnumber( L, -1 )) {
        ws2812_cleanup( L, 1 );
        return luaL_argerror( L, stack, "invalid t1l" );
      }
      t1l = luaL_checkint( L, -1 );
      if ((t1l<1) || (t1l>0x7fff)) {
        ws2812_cleanup( L, 1 );
        return luaL_argerror( L, stack, "t1l must be 1<=t1l<=32767" );
      }
    }
    lua_pop( L, 1 );

    //
    // retrieve data
    //
    lua_getfield( L, stack, "data" );

    const char *data;
    size_t length;
    int type = lua_type( L, -1 );
    if (type == LUA_TSTRING)
    {
      data = lua_tolstring( L, -1, &length );
    }
    else if (type == LUA_TUSERDATA)
    {
      ws2812_buffer *buffer = (ws2812_buffer*)luaL_checkudata( L, -1, "ws2812.buffer" );

      data = (const char *)buffer->values;
      length = buffer->colorsPerLed*buffer->size;
    }
    else
    {
      ws2812_cleanup( L, 1 );
      return luaL_argerror(L, stack, "ws2812.buffer or string expected");
    }
    lua_pop( L, 1 );

    // prepare channel
    if (platform_ws2812_setup( gpio_num, reset, t0h, t0l, t1h, t1l, (const uint8_t *)data, length ) != PLATFORM_OK) {
      ws2812_cleanup( L, 0 );
      return luaL_argerror( L, stack, "can't set up chain" );
    }
  }

  //
  // send all channels at once
  //
  if (platform_ws2812_send() != PLATFORM_OK) {
    ws2812_cleanup( L, 0 );
    return luaL_error( L, "sending failed" );
  }

  ws2812_cleanup( L, 0 );

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

  memset(buffer->values, 0, colorsPerLed * leds);

  return 1;
}

static int ws2812_buffer_fill(lua_State* L) {
  ws2812_buffer * buffer = (ws2812_buffer*)luaL_checkudata(L, 1, "ws2812.buffer");

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


static int ws2812_buffer_shift(lua_State* L) {
  ws2812_buffer * buffer = (ws2812_buffer*)luaL_checkudata(L, 1, "ws2812.buffer");
  const int shiftValue = luaL_checkinteger(L, 2);
  const unsigned shift_type = luaL_optinteger( L, 3, SHIFT_LOGICAL );

  ptrdiff_t start = posrelat(luaL_optinteger(L, 4, 1), buffer->size);
  ptrdiff_t end = posrelat(luaL_optinteger(L, 5, -1), buffer->size);
  if (start < 1) start = 1;
  if (end > (ptrdiff_t)buffer->size) end = (ptrdiff_t)buffer->size;

  start--;
  int size = end - start;
  size_t offset = start * buffer->colorsPerLed;

  luaL_argcheck(L, shiftValue > 0-size && shiftValue < size, 2, "shifting more elements than buffer size");

  int shift = shiftValue >= 0 ? shiftValue : -shiftValue;

  // check if we want to shift at all
  if (shift == 0 || size <= 0)
  {
    return 0;
  }

  uint8_t * tmp_pixels = luaM_malloc(L, buffer->colorsPerLed * sizeof(uint8_t) * shift);
  size_t shift_len, remaining_len;
  // calculate length of shift section and remaining section
  shift_len = shift*buffer->colorsPerLed;
  remaining_len = (size-shift)*buffer->colorsPerLed;

  if (shiftValue > 0)
  {
    // Store the values which are moved out of the array (last n pixels)
    memcpy(tmp_pixels, &buffer->values[offset + (size-shift)*buffer->colorsPerLed], shift_len);
    // Move pixels to end
    memmove(&buffer->values[offset + shift*buffer->colorsPerLed], &buffer->values[offset], remaining_len);
    // Fill beginning with temp data
    if (shift_type == SHIFT_LOGICAL)
    {
      memset(&buffer->values[offset], 0, shift_len);
    }
    else
    {
      memcpy(&buffer->values[offset], tmp_pixels, shift_len);
    }
  }
  else
  {
    // Store the values which are moved out of the array (last n pixels)
    memcpy(tmp_pixels, &buffer->values[offset], shift_len);
    // Move pixels to end
    memmove(&buffer->values[offset], &buffer->values[offset + shift*buffer->colorsPerLed], remaining_len);
    // Fill beginning with temp data
    if (shift_type == SHIFT_LOGICAL)
    {
      memset(&buffer->values[offset + (size-shift)*buffer->colorsPerLed], 0, shift_len);
    }
    else
    {
      memcpy(&buffer->values[offset + (size-shift)*buffer->colorsPerLed], tmp_pixels, shift_len);
    }
  }
  // Free memory
  luaM_free(L, tmp_pixels);

  return 0;
}

static int ws2812_buffer_dump(lua_State* L) {
  ws2812_buffer * buffer = (ws2812_buffer*)luaL_checkudata(L, 1, "ws2812.buffer");

  lua_pushlstring(L, (char *)buffer->values, buffer->size * buffer->colorsPerLed);

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

  memcpy(buffer->values + (start - 1) * buffer->colorsPerLed, src, srcLen * buffer->colorsPerLed);

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
    int val = 0;
    for (src = 0; src < n_sources; src++) {
      val += ((int)(source[src].values[i] * source[src].factor) >> 8);
    }

    if (val < 0) {
      val = 0;
    } else if (val > 255) {
      val = 255;
    }
    buffer->values[i] = val;
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

    memcpy(&buffer->values[buffer->colorsPerLed*led], buf, len);
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
    memcpy(result->values, lhs->values + lhs->colorsPerLed * (start - 1), lhs->colorsPerLed * (end - start + 1));
  } else {
    ws2812_buffer *result = allocate_buffer(L, 0, lhs->colorsPerLed);
    (void)result;
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

  memcpy(buffer->values, lhs->values, lhs->colorsPerLed * lhs->size);
  memcpy(buffer->values + lhs->colorsPerLed * lhs->size, rhs->values, rhs->colorsPerLed * rhs->size);

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
      sprintf(numbuf, "%d", buffer->values[p]);
      luaL_addstring(&result, numbuf);
    }
    luaL_addchar(&result, ')');
  }

  luaL_addchar(&result, ']');
  luaL_pushresult(&result);

  return 1;
}

LROT_BEGIN(ws2812_buffer, NULL, LROT_MASK_INDEX)
  LROT_FUNCENTRY( __concat,   ws2812_buffer_concat )
  LROT_TABENTRY ( __index,    ws2812_buffer )
  LROT_FUNCENTRY( __tostring, ws2812_buffer_tostring )
  LROT_FUNCENTRY( dump,       ws2812_buffer_dump )
  LROT_FUNCENTRY( fade,       ws2812_buffer_fade )
  LROT_FUNCENTRY( fill,       ws2812_buffer_fill )
  LROT_FUNCENTRY( get,        ws2812_buffer_get )
  LROT_FUNCENTRY( replace,    ws2812_buffer_replace )
  LROT_FUNCENTRY( mix,        ws2812_buffer_mix )
  LROT_FUNCENTRY( power,      ws2812_buffer_power )
  LROT_FUNCENTRY( set,        ws2812_buffer_set )
  LROT_FUNCENTRY( shift,      ws2812_buffer_shift )
  LROT_FUNCENTRY( size,       ws2812_buffer_size )
  LROT_FUNCENTRY( sub,        ws2812_buffer_sub )
LROT_END(ws2812_buffer, NULL, LROT_MASK_INDEX)

LROT_BEGIN(ws2812, NULL, 0)
  LROT_FUNCENTRY( newBuffer,      ws2812_new_buffer )
  LROT_FUNCENTRY( write,          ws2812_write )
  LROT_NUMENTRY ( FADE_IN,        FADE_IN )
  LROT_NUMENTRY ( FADE_OUT,       FADE_OUT )
  LROT_NUMENTRY ( SHIFT_LOGICAL,  SHIFT_LOGICAL )
  LROT_NUMENTRY ( SHIFT_CIRCULAR, SHIFT_CIRCULAR )
LROT_END(ws2812, NULL, 0)

int luaopen_ws2812(lua_State *L) {
  // TODO: Make sure that the GPIO system is initialized
  luaL_rometatable(L, "ws2812.buffer", LROT_TABLEREF(ws2812_buffer));  // create metatable for ws2812.buffer
  return 0;
}

NODEMCU_MODULE(WS2812, "ws2812", ws2812, luaopen_ws2812);
