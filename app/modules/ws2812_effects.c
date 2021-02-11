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
#include "pm/swtimer.h"

#include "pixbuf.h"
#include "color_utils.h"

#ifdef LUA_USE_MODULES_WS2812_EFFECTS
#ifndef LUA_USE_MODULES_PIXBUF
#  error module pixbuf is required for ws2812_effects
#endif
#ifndef LUA_USE_MODULES_COLOR_UTILS
#  error module color_utilsf is required for ws2812_effects
#endif
#endif

#define CANARY_VALUE 0x32372132

#define DEFAULT_MODE 0
#define DEFAULT_COLOR 0xFF0000

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

#define IDX_R 1
#define IDX_G 0
#define IDX_B 2
#define IDX_W 3

typedef struct {
  pixbuf *buffer;
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

  struct pixbuf_shift_params shift;
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

// XXX Not exported because this module is its sole non-Lua consumer and we
// should be going away soon!  Deprecated, 'n all that.
extern void ICACHE_RAM_ATTR ws2812_write_data(
  const uint8_t *pixels, uint32_t length,
  const uint8_t *pixels2, uint32_t length2);

static int ws2812_effects_write(pixbuf* buffer) {
  ws2812_write_data(buffer->values, pixbuf_size(buffer), 0, 0);
  return 0;
}

// :opens_boxes -1
static void ws2812_set_pixel(int pixel, uint32_t color) {
  pixbuf * buffer = state->buffer;

  uint8_t g = ((color & 0x00FF0000) >> 16);
  uint8_t r = ((color & 0x0000FF00) >> 8);
  uint8_t b = (color & 0x000000FF);
  uint8_t w = pixbuf_channels(buffer) == 4 ? ((color & 0xFF000000) >> 24) : 0;

  int offset = pixel * pixbuf_channels(buffer);
  buffer->values[offset+IDX_R] = r;
  buffer->values[offset+IDX_G] = g;
  buffer->values[offset+IDX_B] = b;
  if (pixbuf_channels(buffer) == 4) {
    buffer->values[offset+IDX_W] = w;
  }
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

  platform_print_deprecation_note("ws2812_effects",
    "soon; please see https://github.com/nodemcu/nodemcu-firmware/issues/3122");

  pixbuf * buffer = pixbuf_from_lua_arg(L, 1);

  // get rid of old state
  if (state != NULL) {
    luaL_unref(L, LUA_REGISTRYINDEX, state->buffer_ref);
    free((void *) state);
  }

  // Allocate memory and set all to zero
  state = (ws2812_effects *) calloc(1,sizeof(ws2812_effects));
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
  lua_pushinteger(L, state->speed);
  return 1;
}

static int ws2812_effects_set_speed(lua_State* L) {
  int speed = luaL_checkinteger(L, 1);
  luaL_argcheck(L, state != NULL, 1, LIBRARY_NOT_INITIALIZED_ERROR_MSG);
  luaL_argcheck(L, speed >= SPEED_MIN && speed <= SPEED_MAX, 1, "should be 0-255");
  state->speed = (uint8_t)speed;
  state->mode_delay = 10;
  return 0;
}

static int ws2812_effects_get_delay(lua_State* L) {
  luaL_argcheck(L, state != NULL, 1, LIBRARY_NOT_INITIALIZED_ERROR_MSG);
  lua_pushinteger(L, state->mode_delay);
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
  int brightness = luaL_checkint(L, 1);
  luaL_argcheck(L, state != NULL, 1, LIBRARY_NOT_INITIALIZED_ERROR_MSG);
  luaL_argcheck(L, brightness >= BRIGHTNESS_MIN && brightness <= BRIGHTNESS_MAX, 1, "should be 0-255");
  state->brightness = (uint8_t) brightness;
  return 0;
}

// :opens_boxes -1
static void ws2812_effects_fill_buffer(uint8_t r, uint8_t g, uint8_t b, uint8_t w) {

  pixbuf * buffer = state->buffer;

  uint8_t bright_g = g * state->brightness / BRIGHTNESS_MAX;
  uint8_t bright_r = r * state->brightness / BRIGHTNESS_MAX;
  uint8_t bright_b = b * state->brightness / BRIGHTNESS_MAX;
  uint8_t bright_w = w * state->brightness / BRIGHTNESS_MAX;

  // Fill buffer
  int i;
  uint8_t * p = &buffer->values[0];
  for(i = 0; i < buffer->npix; i++) {
    *p++ = bright_g;
    *p++ = bright_r;
    *p++ = bright_b;
    if (pixbuf_channels(buffer) == 4) {
      *p++ = bright_w;
    }
  }
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

  ws2812_effects_fill_buffer(r, g, b, w);

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
    pixbuf * buffer = state->buffer;
    memset(&buffer->values[0], 0, pixbuf_size(buffer));
  }
  return 0;
}



static int ws2812_effects_gradient(const char *gradient_spec, size_t length1) {

  pixbuf * buffer = state->buffer;

  int segments = (length1 / pixbuf_channels(buffer)) - 1;
  int segmentSize = buffer->npix / segments;

  uint8_t g1, r1, b1, g2, r2, b2;
  int i,j,k;

  g2 = *gradient_spec++;
  r2 = *gradient_spec++;
  b2 = *gradient_spec++;
  // skip non-rgb components
  for (j = 3; j < pixbuf_channels(buffer); j++)
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
    for (j = 3; j < pixbuf_channels(buffer); j++)
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
      numPixels = buffer->npix - (segmentSize * (segments - 1));
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

      *p++ = ((grb & 0x00FF0000) >> 16) * state->brightness / BRIGHTNESS_MAX;
      *p++ = ((grb & 0x0000FF00) >> 8)  * state->brightness / BRIGHTNESS_MAX;
      *p++ = (grb & 0x000000FF) * state->brightness / BRIGHTNESS_MAX;

      for (j = 3; j < pixbuf_channels(buffer); j++) {
        *p++ = 0;
      }
    }
  }

  return 0;
}



static int ws2812_effects_gradient_rgb(const char *buffer1, size_t length1) {

  pixbuf * buffer = state->buffer;

  int segments = (length1 / pixbuf_channels(buffer)) - 1;
  int segmentSize = buffer->npix / segments;

  uint8_t g1, r1, b1, g2, r2, b2;
  int i,j,k;

  g2 = *buffer1++;
  r2 = *buffer1++;
  b2 = *buffer1++;
  // skip non-rgb components
  for (j = 3; j < pixbuf_channels(buffer); j++)
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

    for (j = 3; j < pixbuf_channels(buffer); j++) {
      *buffer1++;
    }

    // Fill buffer
    int numPixels = segmentSize;
    // make sure we fill the strip correctly in case of rounding errors
    if (k == segments - 1) {
      numPixels = buffer->npix - (segmentSize * (segments - 1));
    }

    int steps = numPixels - 1;

    for(i = 0; i < numPixels; i++) {
      *p++ = (g1 + ((g2-g1) * i / steps)) * state->brightness / BRIGHTNESS_MAX;
      *p++ = (r1 + ((r2-r1) * i / steps)) * state->brightness / BRIGHTNESS_MAX;
      *p++ = (b1 + ((b2-b1) * i / steps)) * state->brightness / BRIGHTNESS_MAX;
      for (j = 3; j < pixbuf_channels(buffer); j++)
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
  pixbuf * buffer = state->buffer;

  uint32_t color = color_wheel(state->mode_color_index);
  uint8_t r = ((color & 0x00FF0000) >> 16) * state->brightness / BRIGHTNESS_MAX;
  uint8_t g = ((color & 0x0000FF00) >>  8) * state->brightness / BRIGHTNESS_MAX;
  uint8_t b = ((color & 0x000000FF) >>  0) * state->brightness / BRIGHTNESS_MAX;

  // Fill buffer
  int i,j;
  uint8_t * p = &buffer->values[0];
  for(i = 0; i < buffer->npix; i++) {
    *p++ = g;
    *p++ = r;
    *p++ = b;
    for (j = 3; j < pixbuf_channels(buffer); j++)
    {
      *p++ = 0;
    }
  }
}


/*
* Cycles all LEDs at once through a rainbow.
*/
static int ws2812_effects_mode_rainbow() {

  pixbuf * buffer = state->buffer;

  uint32_t color = color_wheel(state->counter_mode_step);
  uint8_t r = (color & 0x00FF0000) >> 16;
  uint8_t g = (color & 0x0000FF00) >>  8;
  uint8_t b = (color & 0x000000FF) >>  0;

  // Fill buffer
  int i,j;
  uint8_t * p = &buffer->values[0];
  for(i = 0; i < buffer->npix; i++) {
    *p++ = g * state->brightness / BRIGHTNESS_MAX;
    *p++ = r * state->brightness / BRIGHTNESS_MAX;
    *p++ = b * state->brightness / BRIGHTNESS_MAX;
    for (j = 3; j < pixbuf_channels(buffer); j++)
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

  pixbuf * buffer = state->buffer;

  int i,j;
  uint8_t * p = &buffer->values[0];
  for(i = 0; i < buffer->npix; i++) {
    uint16_t wheel_index = (i * 360 / buffer->npix * repeat_count) % 360;
    uint32_t color = color_wheel(wheel_index);
    uint8_t r = ((color & 0x00FF0000) >> 16) * state->brightness / BRIGHTNESS_MAX;
    uint8_t g = ((color & 0x0000FF00) >>  8) * state->brightness / BRIGHTNESS_MAX;
    uint8_t b = ((color & 0x000000FF) >>  0) * state->brightness / BRIGHTNESS_MAX;
    *p++ = g;
    *p++ = r;
    *p++ = b;
    for (j = 3; j < pixbuf_channels(buffer); j++)
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

  pixbuf * buffer = state->buffer;

  uint8_t p_g = state->color[0];
  uint8_t p_r = state->color[1];
  uint8_t p_b = state->color[2];

  // Fill buffer
  int i,j;
  uint8_t * p = &buffer->values[0];
  for(i = 0; i < buffer->npix; i++) {
    int flicker = rand() % (max_flicker > 0 ? max_flicker : 1);
    int r1 = p_r-flicker;
    int g1 = p_g-flicker;
    int b1 = p_b-flicker;
    if(g1<0) g1=0;
    if(r1<0) r1=0;
    if(b1<0) b1=0;
    *p++ = g1 * state->brightness / BRIGHTNESS_MAX;
    *p++ = r1 * state->brightness / BRIGHTNESS_MAX;
    *p++ = b1 * state->brightness / BRIGHTNESS_MAX;
    for (j = 3; j < pixbuf_channels(buffer); j++) {
      *p++ = 0;
    }
  }

  return 0;
}

/**
* Halloween effect
*/
static int ws2812_effects_mode_halloween() {
  pixbuf * buffer = state->buffer;

  int g1 = 50 * state->brightness / BRIGHTNESS_MAX;
  int r1 = 255 * state->brightness / BRIGHTNESS_MAX;
  int b1 = 0 * state->brightness / BRIGHTNESS_MAX;

  int g2 = 0 * state->brightness / BRIGHTNESS_MAX;
  int r2 = 255 * state->brightness / BRIGHTNESS_MAX;
  int b2 = 130 * state->brightness / BRIGHTNESS_MAX;


  // Fill buffer
  int i,j;
  uint8_t * p = &buffer->values[0];
  for(i = 0; i < buffer->npix; i++) {
    *p++ = (i % 4 < 2) ? g1 : g2;
    *p++ = (i % 4 < 2) ? r1 : r2;
    *p++ = (i % 4 < 2) ? b1 : b2;
    for (j = 3; j < pixbuf_channels(buffer); j++)
    {
      *p++ = 0;
    }
  }

  return 0;
}



static int ws2812_effects_mode_circus_combustus() {
  pixbuf * buffer = state->buffer;

  int g1 = 0 * state->brightness / BRIGHTNESS_MAX;
  int r1 = 255 * state->brightness / BRIGHTNESS_MAX;
  int b1 = 0 * state->brightness / BRIGHTNESS_MAX;

  int g2 = 255 * state->brightness / BRIGHTNESS_MAX;
  int r2 = 255 * state->brightness / BRIGHTNESS_MAX;
  int b2 = 255 * state->brightness / BRIGHTNESS_MAX;

  // Fill buffer
  int i,j;
  uint8_t * p = &buffer->values[0];
  for(i = 0; i < buffer->npix; i++) {
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
    for (j = 3; j < pixbuf_channels(buffer); j++)
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

  pixbuf * buffer = state->buffer;
  int led_index = 0;

  for(int i=0; i < pixbuf_size(buffer); i++) {
    buffer->values[i] = buffer->values[i] >> 2;
  }

  uint16_t pos = 0;

  if(state->counter_mode_step < buffer->npix) {
    pos = state->counter_mode_step;
  } else {
    pos = (buffer->npix * 2) - state->counter_mode_step - 2;
  }
  pos = pos * pixbuf_channels(buffer);
  buffer->values[pos + 1] = state->color[1];
  buffer->values[pos] = state->color[0];
  buffer->values[pos + 2] = state->color[2];

  state->counter_mode_step = (state->counter_mode_step + 1) % ((buffer->npix * 2) - 2);
}



static int ws2812_effects_mode_color_wipe() {

  pixbuf * buffer = state->buffer;

  int led_index = (state->counter_mode_step % buffer->npix) * pixbuf_channels(buffer);

  if (state->counter_mode_step >= buffer->npix)
  {
    buffer->values[led_index] = 0;
    buffer->values[led_index + 1] = 0;
    buffer->values[led_index + 2] = 0;
  }
  else
  {
    uint8_t px_r = state->color[1] * state->brightness / BRIGHTNESS_MAX;
    uint8_t px_g = state->color[0] * state->brightness / BRIGHTNESS_MAX;
    uint8_t px_b = state->color[2] * state->brightness / BRIGHTNESS_MAX;
    buffer->values[led_index] = px_g;
    buffer->values[led_index + 1] = px_r;
    buffer->values[led_index + 2] = px_b;
  }
  state->counter_mode_step = (state->counter_mode_step + 1) % (buffer->npix * 2);
}

static int ws2812_effects_mode_random_dot(uint8_t dots) {

  pixbuf * buffer = state->buffer;

  // fade out
  for(int i=0; i < pixbuf_size(buffer); i++) {
    buffer->values[i] = buffer->values[i] >> 1;
  }

  for(int i=0; i < dots; i++) {
    // pick random pixel
    int led_index  = rand() % buffer->npix;

    uint32_t color = (state->color[0] << 16) | (state->color[1] << 8) | state->color[2];
    if (pixbuf_channels(buffer) == 4) {
      color = color | (state->color[3] << 24);
    }
    ws2812_set_pixel(led_index, color);
  }

  state->counter_mode_step = (state->counter_mode_step + 1) % ((buffer->npix * 2) - 2);
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

static void ws2812_effects_do_shift(void)
{
  pixbuf_shift(state->buffer, &state->shift);
  ws2812_effects_write(state->buffer);
}

/**
* run loop for the effects.
*/
static void ws2812_effects_loop(void* p)
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
    ws2812_effects_do_shift();
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
    ws2812_effects_do_shift();
  }
  else if (state->effect_type == WS2812_EFFECT_CIRCUS_COMBUSTUS)
  {
    ws2812_effects_do_shift();
  }
  else if (state->effect_type == WS2812_EFFECT_LARSON_SCANNER)
  {
    ws2812_effects_mode_larson_scanner();
  }
  else if (state->effect_type == WS2812_EFFECT_CYCLE)
  {
    ws2812_effects_do_shift();
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
  ws2812_effects_write(state->buffer);
  // set the timer
  if (state->running == 1 && state->mode_delay >= 10)
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

        if ((length1 / pixbuf_channels(state->buffer) < 2) || (length1 % pixbuf_channels(state->buffer) != 0))
        {
          luaL_argerror(L, 2, "must be at least two colors and same size as buffer colors");
        }

        ws2812_effects_gradient(buffer1, length1);
        ws2812_effects_write(state->buffer);
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

        if ((length1 / pixbuf_channels(state->buffer) < 2) || (length1 % pixbuf_channels(state->buffer) != 0))
        {
          luaL_argerror(L, 2, "must be at least two colors and same size as buffer colors");
        }

        ws2812_effects_gradient_rgb(buffer1, length1);
        ws2812_effects_write(state->buffer);
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
      pixbuf_prepare_shift(state->buffer, &state->shift, 1, PIXBUF_SHIFT_CIRCULAR, 1, -1);
      break;
    case WS2812_EFFECT_FLICKER:
      state->effect_int_param1 = effect_param;
      break;
    case WS2812_EFFECT_FIRE_FLICKER:
    case WS2812_EFFECT_FIRE_FLICKER_SOFT:
    case WS2812_EFFECT_FIRE_FLICKER_INTENSE:
      state->color[0] = 255-40;
      state->color[1] = 255;
      state->color[2] = 40;
      state->color[3] = 0;
      break;
    case WS2812_EFFECT_HALLOWEEN:
      ws2812_effects_mode_halloween();
      pixbuf_prepare_shift(state->buffer, &state->shift, 1, PIXBUF_SHIFT_CIRCULAR, 1, -1);
      break;
    case WS2812_EFFECT_CIRCUS_COMBUSTUS:
      ws2812_effects_mode_circus_combustus();
      pixbuf_prepare_shift(state->buffer, &state->shift, 1, PIXBUF_SHIFT_CIRCULAR, 1, -1);
      break;
    case WS2812_EFFECT_LARSON_SCANNER:
      ws2812_effects_mode_larson_scanner();
      break;
    case WS2812_EFFECT_CYCLE:
      if (effect_param != EFFECT_PARAM_INVALID) {
        state->effect_int_param1 = effect_param;
      }
      pixbuf_prepare_shift(state->buffer, &state->shift, state->effect_int_param1, PIXBUF_SHIFT_CIRCULAR, 1, -1);
      break;
    case WS2812_EFFECT_COLOR_WIPE:
      // fill buffer with black. r,g,b,w = 0
      ws2812_effects_fill_buffer(0, 0, 0, 0);
      ws2812_effects_mode_color_wipe();
      break;
    case WS2812_EFFECT_RANDOM_DOT:
      // check if more than 1 dot shall be set
      state->effect_int_param1 = effect_param;
      // fill buffer with black. r,g,b,w = 0
      ws2812_effects_fill_buffer(0, 0, 0, 0);
      break;
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

LROT_BEGIN(ws2812_effects_map, NULL, 0)
  LROT_FUNCENTRY( __tostring, ws2812_effects_tostring )
  LROT_FUNCENTRY( init, ws2812_effects_init )
  LROT_FUNCENTRY( set_brightness, ws2812_effects_set_brightness )
  LROT_FUNCENTRY( set_color, ws2812_effects_set_color )
  LROT_FUNCENTRY( set_speed, ws2812_effects_set_speed )
  LROT_FUNCENTRY( set_delay, ws2812_effects_set_delay )
  LROT_FUNCENTRY( set_mode, ws2812_effects_set_mode )
  LROT_FUNCENTRY( start, ws2812_effects_start )
  LROT_FUNCENTRY( stop, ws2812_effects_stop )
  LROT_FUNCENTRY( get_delay, ws2812_effects_get_delay )
  LROT_FUNCENTRY( get_speed, ws2812_effects_get_speed )
LROT_END(ws2812_effects_map, NULL, 0)



NODEMCU_MODULE(WS2812_EFFECTS, "ws2812_effects", ws2812_effects_map, NULL);
