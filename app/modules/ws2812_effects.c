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
#include "pm/swtimer.h"

#include "ws2812.h"
#include "color_utils.h"

#define CANARY_VALUE 0x32372132

#define DEFAULT_MODE 0
#define DEFAULT_COLOR 0xFF0000

#define UINT32_MAX 4294967295U

#define SPEED_MIN 0
#define SPEED_MAX 255
#define SPEED_DEFAULT 150
#define DELAY_DEFAULT 100

#define BRIGHTNESS_MIN 0
#define BRIGHTNESS_MAX 255
#define BRIGHTNESS_DEFAULT 100

#define EFFECT_PARAM_INVALID -10000

#define LIBRARY_NOT_INITIALIZED_ERROR_MSG "please call init() first"


#define min(a,b) ((a) < (b) ? (a) : (b))
#define max(a,b) ((a) > (b) ? (a) : (b))
#define abs(a) ((a) > 0 ? (a) : (0-a))
#define min3(a,b, c) min((a), min((b), (c)))
#define max3(a,b, c) max((a), max((b), (c)))



typedef struct {
  ws2812_buffer *buffer;
  int buffer_ref;
  uint32_t mode_delay;
  uint32_t counter_mode_call;
  uint32_t counter_mode_step;
  uint8_t mode_color_index;
  uint8_t speed;
  uint8_t brightness;
  os_timer_t os_t;
  uint8_t running;
  uint8_t effect_type;
  uint8_t color[4];
  int effect_int_param1;
} ws2812_effects;



enum ws2812_effects_type {
  WS2812_EFFECT_STATIC,
  WS2812_EFFECT_BLINK,
  WS2812_EFFECT_GRADIENT,
  WS2812_EFFECT_GRADIENT_RGB,
  WS2812_EFFECT_RANDOM_COLOR,
  WS2812_EFFECT_RAINBOW,
  WS2812_EFFECT_RAINBOW_CYCLE,
  WS2812_EFFECT_FLICKER,
  WS2812_EFFECT_FIRE_FLICKER,
  WS2812_EFFECT_FIRE_FLICKER_SOFT,
  WS2812_EFFECT_FIRE_FLICKER_INTENSE,
  WS2812_EFFECT_HALLOWEEN,
  WS2812_EFFECT_CIRCUS_COMBUSTUS,
  WS2812_EFFECT_LARSON_SCANNER,
  WS2812_EFFECT_CYCLE,
  WS2812_EFFECT_COLOR_WIPE,
  WS2812_EFFECT_RANDOM_DOT
};


static ws2812_effects *state;


//-----------------
// UTILITY METHODS
//-----------------


static int ws2812_write(ws2812_buffer* buffer) {
  size_t length1, length2;
  const char *buffer1, *buffer2;

  buffer1 = 0;
  length1 = 0;

  buffer1 = buffer->values;
  length1 = buffer->colorsPerLed*buffer->size;

  buffer2 = 0;
  length2 = 0;

  // Send the buffers
  ws2812_write_data(buffer1, length1, buffer2, length2);

  return 0;
}


static int ws2812_set_pixel(int pixel, uint32_t color) {
  ws2812_buffer * buffer = state->buffer;
  uint8_t g = ((color & 0x00FF0000) >> 16);
  uint8_t r = ((color & 0x0000FF00) >> 8);
  uint8_t b = (color & 0x000000FF);
  uint8_t w = buffer->colorsPerLed  == 4 ? ((color & 0xFF000000) >> 24) : 0;

  int offset = pixel * buffer->colorsPerLed;
  buffer->values[offset] = g;
  buffer->values[offset+1] = r;
  buffer->values[offset+2] = b;
  if (buffer->colorsPerLed == 4) {
    buffer->values[offset+3] = w;
  }

  return 0;
}


/*
* Returns a new, random color wheel index with a minimum distance of 42 from pos.
*/
static uint8_t get_random_wheel_index(uint8_t pos)
{
  uint8_t r = 0;
  uint8_t x = 0;
  uint8_t y = 0;
  uint8_t d = 0;

  while(d < 42) {
    r = rand() % 360;
    x = abs(pos - r);
    y = 360 - x;
    d = min(x, y);
  }

  return r;
}

//-----------------
// EFFECTS LIBRARY
//-----------------

/**
* initialized ws2812_effects with the buffer to use
*/
static int ws2812_effects_init(lua_State *L) {
  ws2812_buffer * buffer = (ws2812_buffer*)luaL_checkudata(L, 1, "ws2812.buffer");
  luaL_argcheck(L, buffer != NULL, 1, "no valid buffer provided");
  // get rid of old state
  if (state != NULL) {
    luaL_unref(L, LUA_REGISTRYINDEX, state->buffer_ref);
    os_free((void *) state);
  }
  // Allocate memory and set all to zero
  size_t size = sizeof(ws2812_effects) + buffer->colorsPerLed*sizeof(uint8_t);
  state = (ws2812_effects *) os_zalloc(size);
  // initialize
  state->speed = SPEED_DEFAULT;
  state->mode_delay = DELAY_DEFAULT;
  state->brightness = BRIGHTNESS_DEFAULT;
  state->buffer = buffer;

  state->buffer_ref = luaL_ref(L, LUA_REGISTRYINDEX);

  return 0;
}



/*
* set color for single color effects
*/
static int ws2812_effects_set_color(lua_State* L) {
  luaL_argcheck(L, state != NULL, 1, LIBRARY_NOT_INITIALIZED_ERROR_MSG);

  uint8_t g = luaL_checkinteger(L, 1);
  uint8_t r = luaL_checkinteger(L, 2);
  uint8_t b = luaL_checkinteger(L, 3);
  uint8_t w = luaL_optinteger(L, 4, 0 );

  state->color[0] = g;
  state->color[1] = r;
  state->color[2] = b;
  state->color[3] = w;
  return 0;
}


static int ws2812_effects_get_speed(lua_State* L) {
  luaL_argcheck(L, state != NULL, 1, LIBRARY_NOT_INITIALIZED_ERROR_MSG);
  lua_pushnumber(L, state->speed);
  return 1;
}

static int ws2812_effects_set_speed(lua_State* L) {
  uint8_t speed = luaL_checkinteger(L, 1);
  luaL_argcheck(L, state != NULL, 1, LIBRARY_NOT_INITIALIZED_ERROR_MSG);
  luaL_argcheck(L, speed >= 0 && speed <= 255, 1, "should be a 0-255");
  state->speed = speed;
  state->mode_delay = 10;
  return 0;
}

static int ws2812_effects_get_delay(lua_State* L) {
  luaL_argcheck(L, state != NULL, 1, LIBRARY_NOT_INITIALIZED_ERROR_MSG);
  lua_pushnumber(L, state->mode_delay);
  return 1;
}

static int ws2812_effects_set_delay(lua_State* L) {
  luaL_argcheck(L, state != NULL, 1, LIBRARY_NOT_INITIALIZED_ERROR_MSG);
  const int delay = luaL_checkinteger(L, 1);
  luaL_argcheck(L, delay >= 10, 1, "must be equal / larger than 10");

  state->mode_delay = delay;
  state->speed = 0;
  return 1;
}



static int ws2812_effects_set_brightness(lua_State* L) {
  uint8_t brightness = luaL_checkint(L, 1);
  luaL_argcheck(L, state != NULL, 1, LIBRARY_NOT_INITIALIZED_ERROR_MSG);
  luaL_argcheck(L, brightness >= 0 && brightness < 256, 1, "should be a 0-255");
  state->brightness = brightness;
  return 0;
}



static int ws2812_effects_fill_buffer(uint32_t color) {

  ws2812_buffer * buffer = state->buffer;

  uint8_t g = ((color & 0x00FF0000) >> 16);
  uint8_t r = ((color & 0x0000FF00) >> 8);
  uint8_t b = (color & 0x000000FF);
  uint8_t w = buffer->colorsPerLed  == 4 ? ((color & 0xFF000000) >> 24) : 0;

  // Fill buffer
  int i;
  uint8_t * p = &buffer->values[0];
  for(i = 0; i < buffer->size; i++) {
    *p++ = g * state->brightness / 255;
    *p++ = r * state->brightness / 255;
    *p++ = b * state->brightness / 255;
    if (buffer->colorsPerLed == 4) {
      *p++ = w * state->brightness / 255;
    }
  }

  return 0;
}


//------------------
// basic methods
//------------------


/*
* Cycles all LEDs at once through a rainbow.
*/
static int ws2812_effects_fill_color() {

  uint8_t g = state->color[0];
  uint8_t r = state->color[1];
  uint8_t b = state->color[2];
  uint8_t w = state->color[3];

  uint32_t color = (w << 24) | (g << 16) | (r << 8) | b;

  ws2812_effects_fill_buffer(color);

  return 0;
}



//-----------------
// EFFECFTS
//-----------------



/*
* blink with set color
*/
static int ws2812_effects_mode_blink() {
  if(state->counter_mode_call % 2 == 1) {
    // on
    ws2812_effects_fill_color();
  }
  else {
    // off
    ws2812_buffer * buffer = state->buffer;
    c_memset(&buffer->values[0], 0, buffer->size * buffer->colorsPerLed);
  }
  return 0;
}



static int ws2812_effects_gradient(const char *gradient_spec, size_t length1) {

  ws2812_buffer * buffer = state->buffer;

  int segments = (length1 / buffer->colorsPerLed) - 1;
  int segmentSize = buffer->size / segments;

  uint8_t g1, r1, b1, g2, r2, b2;
  int i,j,k;

  g2 = *gradient_spec++;
  r2 = *gradient_spec++;
  b2 = *gradient_spec++;
  // skip non-rgb components
  for (j = 3; j < buffer->colorsPerLed; j++)
  {
    *gradient_spec++;
  }

  // reference to buffer memory
  uint8_t * p = &buffer->values[0];

  uint16_t h1,h2;
  uint8_t s,v,s1,v1,s2,v2;

  for (k = 0; k < segments; k++) {
    g1 = g2;
    r1 = r2;
    b1 = b2;
    uint32_t hsv1 = grb2hsv(g1, r1, b1);
    h1 = (hsv1 & 0xFFFF0000) >> 16;
    s1 = (hsv1 & 0x0000FF00) >> 8;
    v1 = (hsv1 & 0x000000FF);

    g2 = *gradient_spec++;
    r2 = *gradient_spec++;
    b2 = *gradient_spec++;
    for (j = 3; j < buffer->colorsPerLed; j++)
    {
      *gradient_spec++;
    }

    uint32_t hsv2 = grb2hsv(g2, r2, b2);
    h2 = (hsv2 & 0xFFFF0000) >> 16;
    s2 = (hsv1 & 0x0000FF00) >> 8;
    v2 = (hsv1 & 0x000000FF);

    // get distance and direction to use
    int maxCCW = h1 > h2 ? h1 - h2 : 360 + h1 - h2;
    int maxCW = h1 > h2 ? 360 + h2 - h1 : h2 - h1;

    // Fill buffer
    int numPixels = segmentSize;
    // make sure we fill the strip correctly in case of rounding errors
    if (k == segments - 1) {
      numPixels = buffer->size - (segmentSize * (segments - 1));
    }

    int steps = numPixels - 1;

    for(i = 0; i < numPixels; i++) {
      // calculate HSV values
      //h = h1 + ((h2-h1) * i / fillSize);
      int h = maxCCW > maxCW ? h1 + ((maxCW * i / steps) % 360) : h1 - (maxCCW * i / steps);
      if (h < 0) h = h + 360;
      if (h > 359) h = h - 360;
      s = s1 + ((s2-s1) * i / steps);
      v = v1 + ((v2-v1) * i / steps);
      // convert to RGB
      uint32_t grb = hsv2grb(h, s, v);

      *p++ = ((grb & 0x00FF0000) >> 16) * state->brightness / 255;
      *p++ = ((grb & 0x0000FF00) >> 8)  * state->brightness / 255;
      *p++ = (grb & 0x000000FF) * state->brightness / 255;

      for (j = 3; j < buffer->colorsPerLed; j++) {
        *p++ = 0;
      }
    }
  }

  return 0;
}



static int ws2812_effects_gradient_rgb(const char *buffer1, size_t length1) {

  ws2812_buffer * buffer = state->buffer;

  int segments = (length1 / buffer->colorsPerLed) - 1;
  int segmentSize = buffer->size / segments;

  uint8_t g1, r1, b1, g2, r2, b2;
  int i,j,k;

  g2 = *buffer1++;
  r2 = *buffer1++;
  b2 = *buffer1++;
  // skip non-rgb components
  for (j = 3; j < buffer->colorsPerLed; j++)
  {
    *buffer1++;
  }

  // reference to buffer memory
  uint8_t * p = &buffer->values[0];
  for (k = 0; k < segments; k++) {
    g1 = g2;
    r1 = r2;
    b1 = b2;

    g2 = *buffer1++;
    r2 = *buffer1++;
    b2 = *buffer1++;

    for (j = 3; j < buffer->colorsPerLed; j++) {
      *buffer1++;
    }

    // Fill buffer
    int numPixels = segmentSize;
    // make sure we fill the strip correctly in case of rounding errors
    if (k == segments - 1) {
      numPixels = buffer->size - (segmentSize * (segments - 1));
    }

    int steps = numPixels - 1;

    for(i = 0; i < numPixels; i++) {
      *p++ = (g1 + ((g2-g1) * i / steps)) * state->brightness / 255;
      *p++ = (r1 + ((r2-r1) * i / steps)) * state->brightness / 255;
      *p++ = (b1 + ((b2-b1) * i / steps)) * state->brightness / 255;
      for (j = 3; j < buffer->colorsPerLed; j++)
      {
        *p++ = 0;
      }
    }
  }

  return 0;
}


/*
* Lights all LEDs in one random color up. Then switches them
* to the next random color.
*/
static int ws2812_effects_mode_random_color() {
  state->mode_color_index = get_random_wheel_index(state->mode_color_index);
  ws2812_buffer * buffer = state->buffer;

  uint32_t color = color_wheel(state->mode_color_index);
  uint8_t r = ((color & 0x00FF0000) >> 16) * state->brightness / 255;
  uint8_t g = ((color & 0x0000FF00) >>  8) * state->brightness / 255;
  uint8_t b = ((color & 0x000000FF) >>  0) * state->brightness / 255;

  // Fill buffer
  int i,j;
  uint8_t * p = &buffer->values[0];
  for(i = 0; i < buffer->size; i++) {
    *p++ = g;
    *p++ = r;
    *p++ = b;
    for (j = 3; j < buffer->colorsPerLed; j++)
    {
      *p++ = 0;
    }
  }
}


/*
* Cycles all LEDs at once through a rainbow.
*/
static int ws2812_effects_mode_rainbow() {

  ws2812_buffer * buffer = state->buffer;

  uint32_t color = color_wheel(state->counter_mode_step);
  uint8_t r = (color & 0x00FF0000) >> 16;
  uint8_t g = (color & 0x0000FF00) >>  8;
  uint8_t b = (color & 0x000000FF) >>  0;

  // Fill buffer
  int i,j;
  uint8_t * p = &buffer->values[0];
  for(i = 0; i < buffer->size; i++) {
    *p++ = g * state->brightness / 255;
    *p++ = r * state->brightness / 255;
    *p++ = b * state->brightness / 255;
    for (j = 3; j < buffer->colorsPerLed; j++)
    {
      *p++ = 0;
    }
  }

  state->counter_mode_step = (state->counter_mode_step + 1) % 360;
  return 0;
}


/*
* Cycles a rainbow over the entire string of LEDs.
*/
static int ws2812_effects_mode_rainbow_cycle(int repeat_count) {

  ws2812_buffer * buffer = state->buffer;

  int i,j;
  uint8_t * p = &buffer->values[0];
  for(i = 0; i < buffer->size; i++) {
    uint16_t wheel_index = (i * 360 / buffer->size * repeat_count) % 360;
    uint32_t color = color_wheel(wheel_index);
    uint8_t r = ((color & 0x00FF0000) >> 16) * state->brightness / 255;
    uint8_t g = ((color & 0x0000FF00) >>  8) * state->brightness / 255;
    uint8_t b = ((color & 0x000000FF) >>  0) * state->brightness / 255;
    *p++ = g;
    *p++ = r;
    *p++ = b;
    for (j = 3; j < buffer->colorsPerLed; j++)
    {
      *p++ = 0;
    }
  }

  return 0;
}



/*
* Random flickering.
*/
static int ws2812_effects_mode_flicker_int(uint8_t max_flicker) {

  ws2812_buffer * buffer = state->buffer;

  uint8_t p_g = state->color[0];
  uint8_t p_r = state->color[1];
  uint8_t p_b = state->color[2];

  // Fill buffer
  int i,j;
  uint8_t * p = &buffer->values[0];
  for(i = 0; i < buffer->size; i++) {
    int flicker = rand() % (max_flicker > 0 ? max_flicker : 1);
    int r1 = p_r-flicker;
    int g1 = p_g-flicker;
    int b1 = p_b-flicker;
    if(g1<0) g1=0;
    if(r1<0) r1=0;
    if(b1<0) b1=0;
    *p++ = g1 * state->brightness / 255;
    *p++ = r1 * state->brightness / 255;
    *p++ = b1 * state->brightness / 255;
    for (j = 3; j < buffer->colorsPerLed; j++) {
      *p++ = 0;
    }
  }

  return 0;
}

/**
* Halloween effect
*/
static int ws2812_effects_mode_halloween() {
  ws2812_buffer * buffer = state->buffer;

  int g1 = 50 * state->brightness / 255;
  int r1 = 255 * state->brightness / 255;
  int b1 = 0 * state->brightness / 255;

  int g2 = 0 * state->brightness / 255;
  int r2 = 255 * state->brightness / 255;
  int b2 = 130 * state->brightness / 255;


  // Fill buffer
  int i,j;
  uint8_t * p = &buffer->values[0];
  for(i = 0; i < buffer->size; i++) {
    *p++ = (i % 4 < 2) ? g1 : g2;
    *p++ = (i % 4 < 2) ? r1 : r2;
    *p++ = (i % 4 < 2) ? b1 : b2;
    for (j = 3; j < buffer->colorsPerLed; j++)
    {
      *p++ = 0;
    }
  }

  return 0;
}



static int ws2812_effects_mode_circus_combustus() {
  ws2812_buffer * buffer = state->buffer;

  int g1 = 0 * state->brightness / 255;
  int r1 = 255 * state->brightness / 255;
  int b1 = 0 * state->brightness / 255;

  int g2 = 255 * state->brightness / 255;
  int r2 = 255 * state->brightness / 255;
  int b2 = 255 * state->brightness / 255;

  // Fill buffer
  int i,j;
  uint8_t * p = &buffer->values[0];
  for(i = 0; i < buffer->size; i++) {
    if (i % 6 < 2) {
      *p++ = g1;
      *p++ = r1;
      *p++ = b1;
    }
    else if (i % 6 < 4) {
      *p++ = g2;
      *p++ = r2;
      *p++ = b2;
    }
    else {
      *p++ = 0;
      *p++ = 0;
      *p++ = 0;
    }
    for (j = 3; j < buffer->colorsPerLed; j++)
    {
      *p++ = 0;
    }
  }

  return 0;
}




/*
* K.I.T.T.
*/
static int ws2812_effects_mode_larson_scanner() {

  ws2812_buffer * buffer = state->buffer;
  int led_index = 0;

  for(int i=0; i < buffer->size * buffer->colorsPerLed; i++) {
    buffer->values[i] = buffer->values[i] >> 1;
  }

  uint16_t pos = 0;

  if(state->counter_mode_step < buffer->size) {
    pos = state->counter_mode_step;
  } else {
    pos = (buffer->size * 2) - state->counter_mode_step - 2;
  }
  pos = pos * buffer->colorsPerLed;
  buffer->values[pos + 1] = state->color[1];
  buffer->values[pos] = state->color[0];
  buffer->values[pos + 2] = state->color[2];

  state->counter_mode_step = (state->counter_mode_step + 1) % ((buffer->size * 2) - 2);
}



static int ws2812_effects_mode_color_wipe() {

  ws2812_buffer * buffer = state->buffer;

  int led_index = (state->counter_mode_step % buffer->size) * buffer->colorsPerLed;

  if (state->counter_mode_step >= buffer->size)
  {
    buffer->values[led_index] = 0;
    buffer->values[led_index + 1] = 0;
    buffer->values[led_index + 2] = 0;
  }
  else
  {
    uint8_t px_r = state->color[1] * state->brightness / 255;
    uint8_t px_g = state->color[0] * state->brightness / 255;
    uint8_t px_b = state->color[2] * state->brightness / 255;
    buffer->values[led_index] = px_g;
    buffer->values[led_index + 1] = px_r;
    buffer->values[led_index + 2] = px_b;
  }
  state->counter_mode_step = (state->counter_mode_step + 1) % (buffer->size * 2);
}

static int ws2812_effects_mode_random_dot(uint8_t dots) {

  ws2812_buffer * buffer = state->buffer;

  // fade out
  for(int i=0; i < buffer->size * buffer->colorsPerLed; i++) {
    buffer->values[i] = buffer->values[i] >> 1;
  }

  for(int i=0; i < dots; i++) {
    // pick random pixel
    int led_index  = rand() % buffer->size;

    uint32_t color = (state->color[0] << 16) | (state->color[1] << 8) | state->color[2];
    if (buffer->colorsPerLed == 4) {
      color = color | (state->color[3] << 24);
    }
    ws2812_set_pixel(led_index, color);
  }

  state->counter_mode_step = (state->counter_mode_step + 1) % ((buffer->size * 2) - 2);
}



static uint32_t ws2812_effects_mode_delay()
{
  // check if delay has been set explicitly
  if (state->speed == 0 && state->mode_delay > 0)
  {
    return state->mode_delay;
  }

  uint32_t delay = 10;
  switch (state->effect_type) {
    case WS2812_EFFECT_BLINK:
    case WS2812_EFFECT_RAINBOW:
    case WS2812_EFFECT_RAINBOW_CYCLE:
      delay = 10 + ((1000 * (uint32_t)(SPEED_MAX - state->speed)) / SPEED_MAX);
    break;
    case WS2812_EFFECT_FLICKER:
    case WS2812_EFFECT_FIRE_FLICKER:
    case WS2812_EFFECT_FIRE_FLICKER_SOFT:
    case WS2812_EFFECT_FIRE_FLICKER_INTENSE:
      delay = 30 + (rand() % 100) + (200 * (SPEED_MAX - state->speed) / SPEED_MAX);
    break;

    case WS2812_EFFECT_RANDOM_COLOR:
    case WS2812_EFFECT_HALLOWEEN:
    case WS2812_EFFECT_CIRCUS_COMBUSTUS:
    case WS2812_EFFECT_LARSON_SCANNER:
    case WS2812_EFFECT_CYCLE:
    case WS2812_EFFECT_COLOR_WIPE:
    case WS2812_EFFECT_RANDOM_DOT:
      delay = 10 + ((1000 * (uint32_t)(SPEED_MAX - state->speed)) / SPEED_MAX);
    break;

  }
  return delay;
}


/**
* run loop for the effects.
*/
static void ws2812_effects_loop(void *p)
{

  if (state->effect_type == WS2812_EFFECT_BLINK)
  {
    ws2812_effects_mode_blink();
  }
  else if (state->effect_type == WS2812_EFFECT_RAINBOW)
  {
    ws2812_effects_mode_rainbow();
  }
  else if (state->effect_type == WS2812_EFFECT_RAINBOW_CYCLE)
  {
    // the rainbow cycle effect can be achieved by shifting the buffer
    ws2812_buffer_shift(state->buffer, 1, SHIFT_CIRCULAR, 1, -1);
  }
  else if (state->effect_type == WS2812_EFFECT_FLICKER)
  {
    int flicker_value = state->effect_int_param1 != EFFECT_PARAM_INVALID ? state->effect_int_param1 : 100;
    if (flicker_value == 0) {
      flicker_value = 50;
    }
    ws2812_effects_mode_flicker_int(flicker_value);
    state->counter_mode_step = (state->counter_mode_step + 1) % 256;
  }
  else if (state->effect_type == WS2812_EFFECT_FIRE_FLICKER)
  {
    ws2812_effects_mode_flicker_int(110);
    state->counter_mode_step = (state->counter_mode_step + 1) % 256;
  }
  else if (state->effect_type == WS2812_EFFECT_FIRE_FLICKER_SOFT)
  {
    ws2812_effects_mode_flicker_int(70);
    state->counter_mode_step = (state->counter_mode_step + 1) % 256;
  }
  else if (state->effect_type == WS2812_EFFECT_FIRE_FLICKER_INTENSE)
  {
    ws2812_effects_mode_flicker_int(170);
    state->counter_mode_step = (state->counter_mode_step + 1) % 256;
  }
  else if (state->effect_type == WS2812_EFFECT_RANDOM_COLOR)
  {
    ws2812_effects_mode_random_color();
  }
  else if (state->effect_type == WS2812_EFFECT_HALLOWEEN)
  {
    ws2812_buffer_shift(state->buffer, 1, SHIFT_CIRCULAR, 1, -1);
  }
  else if (state->effect_type == WS2812_EFFECT_CIRCUS_COMBUSTUS)
  {
    ws2812_buffer_shift(state->buffer, 1, SHIFT_CIRCULAR, 1, -1);
  }
  else if (state->effect_type == WS2812_EFFECT_LARSON_SCANNER)
  {
    ws2812_effects_mode_larson_scanner();
  }
  else if (state->effect_type == WS2812_EFFECT_CYCLE)
  {
    ws2812_buffer_shift(state->buffer, state->effect_int_param1, SHIFT_CIRCULAR, 1, -1);
  }
  else if (state->effect_type == WS2812_EFFECT_COLOR_WIPE)
  {
    ws2812_effects_mode_color_wipe();
  }
  else if (state->effect_type == WS2812_EFFECT_RANDOM_DOT)
  {
    uint8_t dots = state->effect_int_param1 != EFFECT_PARAM_INVALID ? state->effect_int_param1 : 1;
    ws2812_effects_mode_random_dot(dots);
  }

  // set the new delay for this effect
  state->mode_delay = ws2812_effects_mode_delay();
  // call count
  state->counter_mode_call = (state->counter_mode_call + 1) % UINT32_MAX;
  // write the buffer
  ws2812_write(state->buffer);
  // set the timer
  if (state->running == 1 && state->mode_delay >= 10)
  {
    os_timer_disarm(&(state->os_t));
    os_timer_arm(&(state->os_t), state->mode_delay, FALSE);
  }
}


/**
* Set the active effect mode
*/
static int ws2812_effects_set_mode(lua_State* L) {

  luaL_argcheck(L, state != NULL, 1, LIBRARY_NOT_INITIALIZED_ERROR_MSG);

  // opts must be same order as effect type enum
  static const char * const opts[] = {"static", "blink", "gradient", "gradient_rgb", "random_color", "rainbow",
    "rainbow_cycle", "flicker", "fire", "fire_soft", "fire_intense", "halloween", "circus_combustus",
    "larson_scanner", "cycle", "color_wipe", "random_dot", NULL};

  int type = luaL_checkoption(L, 1, NULL, opts);

  state->effect_type = type;
  int effect_param = EFFECT_PARAM_INVALID;
  // check additional int parameter
  // First mandatory parameter
  int arg_type = lua_type(L, 2);
  if (arg_type == LUA_TNONE || arg_type == LUA_TNIL)
  {
    // we don't have a second parameter
  }
  else if(arg_type == LUA_TNUMBER)
  {
    effect_param = luaL_optinteger( L, 2, EFFECT_PARAM_INVALID );
  }

  // initialize the effect
  state->counter_mode_step = 0;

  switch (state->effect_type) {
    case WS2812_EFFECT_STATIC:
    // fill with currently set color
    ws2812_effects_fill_color();
    state->mode_delay = 250;
    break;
    case WS2812_EFFECT_BLINK:
    ws2812_effects_mode_blink();
    break;
    case WS2812_EFFECT_GRADIENT:
    if(arg_type == LUA_TSTRING)
    {
      size_t length1;
      const char *buffer1 = lua_tolstring(L, 2, &length1);

      if ((length1 / state->buffer->colorsPerLed < 2) || (length1 % state->buffer->colorsPerLed != 0))
      {
        luaL_argerror(L, 2, "must be at least two colors and same size as buffer colors");
      }

      ws2812_effects_gradient(buffer1, length1);
      ws2812_write(state->buffer);
    }
    else
    {
      luaL_argerror(L, 2, "string expected");
    }

    break;
    case WS2812_EFFECT_GRADIENT_RGB:
    if(arg_type == LUA_TSTRING)
    {
      size_t length1;
      const char *buffer1 = lua_tolstring(L, 2, &length1);

      if ((length1 / state->buffer->colorsPerLed < 2) || (length1 % state->buffer->colorsPerLed != 0))
      {
        luaL_argerror(L, 2, "must be at least two colors and same size as buffer colors");
      }

      ws2812_effects_gradient_rgb(buffer1, length1);
      ws2812_write(state->buffer);
    }
    else
    {
      luaL_argerror(L, 2, "string expected");
    }

    break;
    case WS2812_EFFECT_RANDOM_COLOR:
    ws2812_effects_mode_random_color();
    break;
    case WS2812_EFFECT_RAINBOW:
    ws2812_effects_mode_rainbow();
    break;
    case WS2812_EFFECT_RAINBOW_CYCLE:
    ws2812_effects_mode_rainbow_cycle(effect_param != EFFECT_PARAM_INVALID ? effect_param : 1);
    break;
    // flicker
    case WS2812_EFFECT_FLICKER:
    state->effect_int_param1 = effect_param;
    break;
    case WS2812_EFFECT_FIRE_FLICKER:
    case WS2812_EFFECT_FIRE_FLICKER_SOFT:
    case WS2812_EFFECT_FIRE_FLICKER_INTENSE:
    {
      state->color[0] = 255-40;
      state->color[1] = 255;
      state->color[2] = 40;
      state->color[3] = 0;
    }
    break;
    case WS2812_EFFECT_HALLOWEEN:
    ws2812_effects_mode_halloween();
    break;
    case WS2812_EFFECT_CIRCUS_COMBUSTUS:
    ws2812_effects_mode_circus_combustus();
    break;
    case WS2812_EFFECT_LARSON_SCANNER:
    ws2812_effects_mode_larson_scanner();
    break;
    case WS2812_EFFECT_CYCLE:
    if (effect_param != EFFECT_PARAM_INVALID) {
      state->effect_int_param1 = effect_param;
    }
    break;
    case WS2812_EFFECT_COLOR_WIPE:
    {
      uint32_t black = 0;
      ws2812_effects_fill_buffer(black);
      ws2812_effects_mode_color_wipe();
      break;
    }
    case WS2812_EFFECT_RANDOM_DOT:
    {
      // check if more than 1 dot shall be set
      state->effect_int_param1 = effect_param;
      uint32_t black = 0;
      ws2812_effects_fill_buffer(black);
      break;
    }
  }

}



/*
* Start the effect execution
*/
static int ws2812_effects_start(lua_State* L) {

  //NODE_DBG("pin:%d, level:%d \n", pin, level);
  luaL_argcheck(L, state != NULL, 1, LIBRARY_NOT_INITIALIZED_ERROR_MSG);
  if (state != NULL) {
    os_timer_disarm(&(state->os_t));
    state->running = 1;
    state->counter_mode_call = 0;
    state->counter_mode_step = 0;
    // set the timer
    os_timer_setfn(&(state->os_t), ws2812_effects_loop, NULL);
    os_timer_arm(&(state->os_t), state->mode_delay, FALSE);
  }
  return 0;
}

/*
* Stop the effect execution
*/
static int ws2812_effects_stop(lua_State* L) {
  luaL_argcheck(L, state != NULL, 1, LIBRARY_NOT_INITIALIZED_ERROR_MSG);
  if (state != NULL) {
    os_timer_disarm(&(state->os_t));
    state->running = 0;
  }
  return 0;
}

static int ws2812_effects_tostring(lua_State* L) {

  luaL_Buffer result;
  luaL_buffinit(L, &result);

  luaL_addchar(&result, '[');
  luaL_addstring(&result, "effects");
  luaL_addchar(&result, ']');
  luaL_pushresult(&result);

  return 1;
}

static const LUA_REG_TYPE ws2812_effects_map[] =
{
  { LSTRKEY( "init" ),              LFUNCVAL( ws2812_effects_init )},
  { LSTRKEY( "set_brightness" ),    LFUNCVAL( ws2812_effects_set_brightness )},
  { LSTRKEY( "set_color" ),         LFUNCVAL( ws2812_effects_set_color )},
  { LSTRKEY( "set_speed" ),         LFUNCVAL( ws2812_effects_set_speed )},
  { LSTRKEY( "set_delay" ),         LFUNCVAL( ws2812_effects_set_delay )},
  { LSTRKEY( "set_mode" ),          LFUNCVAL( ws2812_effects_set_mode )},
  { LSTRKEY( "start" ),             LFUNCVAL( ws2812_effects_start )},
  { LSTRKEY( "stop" ),              LFUNCVAL( ws2812_effects_stop )},
  { LSTRKEY( "get_delay" ),         LFUNCVAL( ws2812_effects_get_delay )},
  { LSTRKEY( "get_speed" ),         LFUNCVAL( ws2812_effects_get_speed )},

  { LSTRKEY( "__index" ), LROVAL( ws2812_effects_map )},
  { LSTRKEY( "__tostring" ), LFUNCVAL( ws2812_effects_tostring )},
  { LNILKEY, LNILVAL}
};


NODEMCU_MODULE(WS2812_EFFECTS, "ws2812_effects", ws2812_effects_map, NULL);
