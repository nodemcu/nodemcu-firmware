#include "module.h"
#include "lauxlib.h"

#include <string.h>

#include "pixbuf.h"
#define PIXBUF_METATABLE "pixbuf.buf"

#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif
#ifndef MAX
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#endif

pixbuf *pixbuf_from_lua_arg(lua_State *L, int arg) {
  return luaL_checkudata(L, arg, PIXBUF_METATABLE);
}

pixbuf *pixbuf_opt_from_lua_arg(lua_State *L, int arg) {
  return luaL_testudata(L, arg, PIXBUF_METATABLE);
}

static ssize_t posrelat(ssize_t pos, size_t len) {
  /* relative string position: negative means back from end */
  if (pos < 0)
    pos += (ssize_t)len + 1;
  return MIN(MAX(pos, 1), len);
}

const size_t pixbuf_channels(pixbuf *p) {
  return p->nchan;
}

const size_t pixbuf_size(pixbuf *p) {
  return p->npix * p->nchan;
}

/*
 * Construct a pixbuf newuserdata using C arguments.
 *
 * Allocates, so may throw!  Leaves new buffer at the top of the Lua stack
 * and returns a C pointer.
 */
static pixbuf *pixbuf_new(lua_State *L, size_t leds, size_t chans) {
  // Allocate memory

  // A crude hack of an overflow check, but unlikely to be reached in practice
  if ((leds > 8192) || (chans > 8)) {
    luaL_error(L, "pixbuf size limits exeeded");
    return NULL; // UNREACHED
  }

  size_t size = sizeof(pixbuf) + leds * chans;

  pixbuf *buffer = (pixbuf*)lua_newuserdata(L, size);

  // Associate its metatable
  luaL_getmetatable(L, PIXBUF_METATABLE);
  lua_setmetatable(L, -2);

  // Save led strip size
  *(size_t *)&buffer->npix = leds;
  *(size_t *)&buffer->nchan = chans;

  memset(buffer->values, 0, leds * chans);

  return buffer;
}

// Handle a buffer where we can store led values
int pixbuf_new_lua(lua_State *L) {
  const int leds  = luaL_checkint(L, 1);
  const int chans = luaL_checkint(L, 2);

  luaL_argcheck(L, leds > 0, 1, "should be a positive integer");
  luaL_argcheck(L, chans > 0, 2, "should be a positive integer");

  pixbuf_new(L, leds, chans);
  return 1;
}

static int pixbuf_concat_lua(lua_State *L) {
  pixbuf *lhs = pixbuf_from_lua_arg(L, 1);
  pixbuf *rhs = pixbuf_from_lua_arg(L, 2);

  luaL_argcheck(L, lhs->nchan == rhs->nchan, 1,
                "can only concatenate buffers with same channel count");

  size_t osize = lhs->npix + rhs->npix;
  if (lhs->npix > osize) {
    return luaL_error(L, "size sum overflow");
  }

  pixbuf *buffer = pixbuf_new(L, osize, lhs->nchan);

  memcpy(buffer->values, lhs->values, pixbuf_size(lhs));
  memcpy(buffer->values + pixbuf_size(lhs), rhs->values, pixbuf_size(rhs));

  return 1;
}

static int pixbuf_channels_lua(lua_State *L) {
  pixbuf *buffer = pixbuf_from_lua_arg(L, 1);
  lua_pushinteger(L, buffer->nchan);
  return 1;
}

static int pixbuf_dump_lua(lua_State *L) {
  pixbuf *buffer = pixbuf_from_lua_arg(L, 1);
  lua_pushlstring(L, (char*)buffer->values, pixbuf_size(buffer));
  return 1;
}

static int pixbuf_eq_lua(lua_State *L) {
  bool res;

  pixbuf *lhs = pixbuf_from_lua_arg(L, 1);
  pixbuf *rhs = pixbuf_from_lua_arg(L, 2);

  if (lhs->npix != rhs->npix) {
    res = false;
  } else if (lhs->nchan != rhs->nchan) {
    res = false;
  } else {
    res = true;
    for(size_t i = 0; i < pixbuf_size(lhs); i++) {
      if(lhs->values[i] != rhs->values[i]) {
        res = false;
        break;
      }
    }
  }

  lua_pushboolean(L, res);
  return 1;
}

static int pixbuf_fade_lua(lua_State *L) {
  pixbuf *buffer = pixbuf_from_lua_arg(L, 1);
  const int fade = luaL_checkinteger(L, 2);
  unsigned direction = luaL_optinteger( L, 3, PIXBUF_FADE_OUT );

  luaL_argcheck(L, fade > 0, 2, "fade value should be a strictly positive int");

  uint8_t *p = &buffer->values[0];
  for (size_t i = 0; i < pixbuf_size(buffer); i++)
  {
    if (direction == PIXBUF_FADE_OUT)
    {
      *p++ /= fade;
    }
    else
    {
      // as fade in can result in value overflow, an int is used to perform the check afterwards
      int val = *p * fade;
      *p++ = MIN(255, val);
    }
  }

  return 0;
}

/* Fade an Ixxx-type strip by just manipulating the I bytes */
static int pixbuf_fadeI_lua(lua_State *L) {
  pixbuf *buffer = pixbuf_from_lua_arg(L, 1);
  const int fade = luaL_checkinteger(L, 2);
  unsigned direction = luaL_optinteger( L, 3, PIXBUF_FADE_OUT );

  luaL_argcheck(L, fade > 0, 2, "fade value should be a strictly positive int");

  uint8_t *p = &buffer->values[0];
  for (size_t i = 0; i < buffer->npix; i++, p+=buffer->nchan) {
    if (direction == PIXBUF_FADE_OUT) {
      *p /= fade;
    } else {
      int val = *p * fade;
      *p++ = MIN(255, val);
    }
  }

  return 0;
}

static int pixbuf_fill_lua(lua_State *L) {
  pixbuf *buffer = pixbuf_from_lua_arg(L, 1);

  if (buffer->npix == 0) {
    goto out;
  }

  if (lua_gettop(L) != (1 + buffer->nchan)) {
    return luaL_argerror(L, 1, "need as many values as colors per pixel");
  }

  /* Fill the first pixel from the Lua stack */
  for (size_t i = 0; i < buffer->nchan; i++) {
    buffer->values[i] = luaL_checkinteger(L, 2+i);
  }

  /* Fill the rest of the pixels from the first */
  for (size_t i = 1; i < buffer->npix; i++) {
    memcpy(&buffer->values[i * buffer->nchan], buffer->values, buffer->nchan);
  }

out:
  lua_settop(L, 1);
  return 1;
}

static int pixbuf_get_lua(lua_State *L) {
  pixbuf *buffer = pixbuf_from_lua_arg(L, 1);
  const int led = luaL_checkinteger(L, 2) - 1;
  size_t channels = buffer->nchan;

  luaL_argcheck(L, led >= 0 && led < buffer->npix, 2, "index out of range");

  uint8_t tmp[channels];
  memcpy(tmp, &buffer->values[channels*led], channels);

  for (size_t i = 0; i < channels; i++)
  {
    lua_pushinteger(L, tmp[i]);
  }

  return channels;
}

/* :map(f, buf1, ilo, ihi, [buf2, ilo2]) */
static int pixbuf_map_lua(lua_State *L) {
  pixbuf *outbuf = pixbuf_from_lua_arg(L, 1);
  /* f at index 2 */

  pixbuf *buffer1 = pixbuf_opt_from_lua_arg(L, 3);
  if (!buffer1)
    buffer1 = outbuf;

  const int ilo = posrelat(luaL_optinteger(L, 4, 1), buffer1->npix) - 1;
  const int ihi = posrelat(luaL_optinteger(L, 5, buffer1->npix), buffer1->npix) - 1;

  luaL_argcheck(L, ihi > ilo, 3, "Buffer limits out of order");

  size_t npix = ihi - ilo + 1;

  luaL_argcheck(L, npix == outbuf->npix, 1, "Output buffer wrong size");

  pixbuf *buffer2 = pixbuf_opt_from_lua_arg(L, 6);
  const int ilo2 = buffer2 ? posrelat(luaL_optinteger(L, 7, 1), buffer2->npix) - 1 : 0;

  if (buffer2) {
    luaL_argcheck(L, ilo2 + npix <= buffer2->npix, 6, "Second buffer too short");
  }

  for (size_t p = 0; p < npix; p++) {
    lua_pushvalue(L, 2);
    for (size_t c = 0; c < buffer1->nchan; c++) {
      lua_pushinteger(L, buffer1->values[(ilo + p) * buffer1->nchan + c]);
    }
    if (buffer2) {
      for (size_t c = 0; c < buffer2->nchan; c++) {
        lua_pushinteger(L, buffer2->values[(ilo2 + p) * buffer2->nchan + c]);
      }
    }
    lua_call(L, buffer1->nchan + (buffer2 ? buffer2->nchan : 0), outbuf->nchan);
    for (size_t c = 0; c < outbuf->nchan; c++) {
      outbuf->values[(p + 1) * outbuf->nchan - c - 1] = luaL_checkinteger(L, -1);
      lua_pop(L, 1);
    }
  }

  lua_settop(L, 1);
  return 1;
}

struct mix_source {
  int factor;
  const uint8_t *values;
};

static uint32_t pixbuf_mix_clamp(int32_t v) {
  if (v <   0) { return   0; }
  if (v > 255) { return 255; }
  return v;
}

/* This one can sum straightforwardly, channel by channel */
static void pixbuf_mix_raw(pixbuf *out, size_t n_src, struct mix_source* src) {
  size_t cells = pixbuf_size(out);

  for (size_t c = 0; c < cells; c++) {
    int32_t val = 0;
    for (size_t s = 0; s < n_src; s++) {
      val += (int32_t)src[s].values[c] * src[s].factor;
    }

    val += 128; // rounding instead of floor
    val /= 256; // do not use implemetation dependant right shift

    out->values[c] = (uint8_t)pixbuf_mix_clamp(val);
  }
}

/* Mix intensity-mediated three-color pixbufs.
 *
 * XXX This is untested in real hardware; do they actually behave like this?
 */
static void pixbuf_mix_i3(pixbuf *out, size_t ibits, size_t n_src,
    struct mix_source* src) {
  for(size_t p = 0; p < out->npix; p++) {
    int32_t sums[3] = { 0, 0, 0 };

    for (size_t s = 0; s < n_src; s++) {
      for (size_t c = 0; c < 3; c++) {
        sums[c] += (int32_t)src[s].values[4*p+c+1] // color channel
                   * src[s].values[4*p]            // global intensity
                   * src[s].factor;                // user factor

      }
    }

    uint32_t pmaxc = 0;
    for (size_t c = 0; c < 3; c++) {
      pmaxc = sums[c] > pmaxc ? sums[c] : pmaxc;
    }

    size_t maxgi;
    if (pmaxc == 0) {
      /* Zero value */
      memset(&out->values[4*p], 0, 4);
      return;
    } else if (pmaxc <= (1 << 16)) {
      /* Minimum global factor */
      maxgi = 1;
    } else if (pmaxc >= ((1 << ibits) - 1) << 16) {
      /* Maximum global factor */
      maxgi = (1 << ibits) - 1;
    } else {
      maxgi = (pmaxc >> 16) + 1;
    }

    // printf("mixi3: %x %x %x -> %x, %zx\n", sums[0], sums[1], sums[2], pmaxc, maxgi);

    out->values[4*p] = maxgi;
    for (size_t c = 0; c < 3; c++) {
      out->values[4*p+c+1] = pixbuf_mix_clamp((sums[c] + 256 * maxgi - 127) / (256 * maxgi));
    }
  }
}

// buffer:mix(factor1, buffer1, ..)
// factor is 256 for 100%
// uses saturating arithmetic (one buffer at a time)
static int pixbuf_mix_core(lua_State *L, size_t ibits) {
  pixbuf *buffer = pixbuf_from_lua_arg(L, 1);
  pixbuf *src_buffer;

  int pos = 2;
  size_t n_sources = (lua_gettop(L) - 1) / 2;
  struct mix_source sources[n_sources];

  if (n_sources == 0) {
    lua_settop(L, 1);
    return 1;
  }

  for (size_t src = 0; src < n_sources; src++, pos += 2) {
    int factor = luaL_checkinteger(L, pos);
    src_buffer = pixbuf_from_lua_arg(L, pos + 1);

    luaL_argcheck(L, src_buffer->npix == buffer->npix &&
                     src_buffer->nchan == buffer->nchan,
                     pos + 1, "buffer not same size or shape");

    sources[src].factor = factor;
    sources[src].values = src_buffer->values;
  }

  if (ibits != 0) {
    luaL_argcheck(L, src_buffer->nchan == 4, 2, "Requires 4 channel pixbuf");
    pixbuf_mix_i3(buffer, ibits, n_sources, sources);
  } else {
    pixbuf_mix_raw(buffer, n_sources, sources);
  }

  lua_settop(L, 1);
  return 1;
}

static int pixbuf_mix_lua(lua_State *L) {
  return pixbuf_mix_core(L, 0);
}

static int pixbuf_mix4I5_lua(lua_State *L) {
  return pixbuf_mix_core(L, 5);
}


// Returns the total of all channels
static int pixbuf_power_lua(lua_State *L) {
  pixbuf *buffer = pixbuf_from_lua_arg(L, 1);

  int total = 0;
  size_t p = 0;
  for (size_t i = 0; i < buffer->npix; i++) {
    for (size_t j = 0; j < buffer->nchan; j++, p++) {
      total += buffer->values[p];
    }
  }

  lua_pushinteger(L, total);
  return 1;
}

// Returns the total of all channels, intensity-style
static int pixbuf_powerI_lua(lua_State *L) {
  pixbuf *buffer = pixbuf_from_lua_arg(L, 1);

  int total = 0;
  size_t p = 0;
  for (size_t i = 0; i < buffer->npix; i++) {
    int inten = buffer->values[p++];
    for (size_t j = 0; j < buffer->nchan - 1; j++, p++) {
      total += inten * buffer->values[p];
    }
  }

  lua_pushinteger(L, total);
  return 1;
}

static int pixbuf_replace_lua(lua_State *L) {
  pixbuf *buffer = pixbuf_from_lua_arg(L, 1);
  ptrdiff_t start = posrelat(luaL_optinteger(L, 3, 1), buffer->npix);
  size_t channels = buffer->nchan;

  uint8_t *src;
  size_t srcLen;

  if (lua_type(L, 2) == LUA_TSTRING) {
    size_t length;
    src = (uint8_t *) lua_tolstring(L, 2, &length);
    srcLen = length / channels;
  } else {
    pixbuf *rhs = pixbuf_from_lua_arg(L, 2);
    luaL_argcheck(L, rhs->nchan == buffer->nchan, 2, "buffers have different channels");
    src = rhs->values;
    srcLen = rhs->npix;
  }

  luaL_argcheck(L, srcLen + start - 1 <= buffer->npix, 2, "does not fit into destination");

  memcpy(buffer->values + (start - 1) * channels, src, srcLen * channels);

  return 0;
}

static int pixbuf_set_lua(lua_State *L) {

  pixbuf *buffer = pixbuf_from_lua_arg(L, 1);
  const int led = luaL_checkinteger(L, 2) - 1;
  const size_t channels = buffer->nchan;

  luaL_argcheck(L, led >= 0 && led < buffer->npix, 2, "index out of range");

  int type = lua_type(L, 3);
  if(type == LUA_TTABLE)
  {
    for (size_t i = 0; i < channels; i++)
    {
      lua_rawgeti(L, 3, i+1);
      buffer->values[channels*led+i] = lua_tointeger(L, -1);
      lua_pop(L, 1);
    }
  }
  else if(type == LUA_TSTRING)
  {
    size_t len;
    const char *buf = lua_tolstring(L, 3, &len);

    // Overflow check
    if( channels*led + len > channels*buffer->npix ) {
      return luaL_error(L, "string size will exceed strip length");
    }
    if ( len % channels != 0 ) {
      return luaL_error(L, "string does not contain whole LEDs");
    }

    memcpy(&buffer->values[channels*led], buf, len);
  }
  else
  {
    luaL_argcheck(L, lua_gettop(L) <= 2 + channels, 2 + channels,
        "extra values given");

    for (size_t i = 0; i < channels; i++)
    {
      buffer->values[channels*led+i] = luaL_checkinteger(L, 3+i);
    }
  }

  lua_settop(L, 1);
  return 1;
}

static void pixbuf_shift_circular(pixbuf *buffer, struct pixbuf_shift_params *sp) {
  /* Move a buffer of pixels per iteration; loop repeatedly if needed */
  uint8_t tmpbuf[32];
  uint8_t *v = buffer->values;
  size_t shiftRemaining = sp->shift;
  size_t cursor = sp->offset;

  do {
    size_t shiftNow = MIN(shiftRemaining, sizeof tmpbuf);

    if (sp->shiftLeft) {
      memcpy(tmpbuf, &v[cursor], shiftNow);
      memmove(&v[cursor], &v[cursor+shiftNow], sp->window - shiftNow);
      memcpy(&v[cursor+sp->window-shiftNow], tmpbuf, shiftNow);
    } else {
      memcpy(tmpbuf, &v[cursor+sp->window-shiftNow], shiftNow);
      memmove(&v[cursor+shiftNow], &v[cursor], sp->window - shiftNow);
      memcpy(&v[cursor], tmpbuf, shiftNow);
    }

    cursor += shiftNow;
    shiftRemaining -= shiftNow;
  } while(shiftRemaining > 0);
}

static void pixbuf_shift_logical(pixbuf *buffer, struct pixbuf_shift_params *sp) {
  /* Logical shifts don't require a temporary buffer, so we just move bytes */
  uint8_t *v = buffer->values;

  if (sp->shiftLeft) {
    memmove(&v[sp->offset], &v[sp->offset+sp->shift], sp->window - sp->shift);
    bzero(&v[sp->offset+sp->window-sp->shift], sp->shift);
  } else {
    memmove(&v[sp->offset+sp->shift], &v[sp->offset], sp->window - sp->shift);
    bzero(&v[sp->offset], sp->shift);
  }
}

void pixbuf_shift(pixbuf *b, struct pixbuf_shift_params *sp) {
#if 0
  printf("Pixbuf %p shifting %s %s by %zd from %zd with window %zd\n",
         b,
	 sp->shiftLeft ? "left" : "right",
	 sp->type == PIXBUF_SHIFT_LOGICAL ? "logically" : "circularly",
	 sp->shift, sp->offset, sp->window);
#endif

  switch(sp->type) {
  case PIXBUF_SHIFT_LOGICAL: return pixbuf_shift_logical(b, sp);
  case PIXBUF_SHIFT_CIRCULAR: return pixbuf_shift_circular(b, sp);
  }
}

int pixbuf_shift_lua(lua_State *L) {
  struct pixbuf_shift_params sp;

  pixbuf *buffer = pixbuf_from_lua_arg(L, 1);
  const int shift_shift = luaL_checkinteger(L, 2) * buffer->nchan;
  const unsigned shift_type = luaL_optinteger(L, 3, PIXBUF_SHIFT_LOGICAL);
  const int pos_start = posrelat(luaL_optinteger(L, 4, 1), buffer->npix);
  const int pos_end = posrelat(luaL_optinteger(L, 5, -1), buffer->npix);

  if (shift_shift < 0) {
    sp.shiftLeft = true;
    sp.shift = -shift_shift;
  } else {
    sp.shiftLeft = false;
    sp.shift = shift_shift;
  }

  switch(shift_type) {
  case PIXBUF_SHIFT_LOGICAL:
  case PIXBUF_SHIFT_CIRCULAR:
    sp.type = shift_type;
    break;
  default:
    return luaL_argerror(L, 3, "invalid shift type");
  }

  if (pos_start < 1) {
    return luaL_argerror(L, 4, "start position must be >= 1");
  }

  if (pos_end < pos_start) {
    return luaL_argerror(L, 5, "end position must be >= start");
  }

  sp.offset = (pos_start - 1) * buffer->nchan;
  sp.window = (pos_end - pos_start + 1) * buffer->nchan;

  if (sp.shift > pixbuf_size(buffer)) {
    return luaL_argerror(L, 2, "shifting more elements than buffer size");
  }

  if (sp.shift > sp.window) {
    return luaL_argerror(L, 2, "shifting more than sliced window");
  }

  pixbuf_shift(buffer, &sp);

  return 0;
}

/* XXX for backwards-compat with ws2812_effects; deprecated and should be removed */
void pixbuf_prepare_shift(pixbuf *buffer, struct pixbuf_shift_params *sp,
    int shift, enum pixbuf_shift type, int start, int end)
{
  start = posrelat(start, buffer->npix);
  end = posrelat(end, buffer->npix);

  lua_assert((end > start) && (start > 0) && (end < buffer->npix));

  sp->type   = type;
  sp->offset = (start - 1) * buffer->nchan;
  sp->window = (end - start + 1) * buffer->nchan;

  if (shift < 0) {
    sp->shiftLeft = true;
    sp->shift = -shift * buffer->nchan;
  } else {
    sp->shiftLeft = false;
    sp->shift = shift * buffer->nchan;
  }
}

static int pixbuf_size_lua(lua_State *L) {
  pixbuf *buffer = pixbuf_from_lua_arg(L, 1);
  lua_pushinteger(L, buffer->npix);
  return 1;
}

static int pixbuf_sub_lua(lua_State *L) {
  pixbuf *lhs = pixbuf_from_lua_arg(L, 1);
  size_t l = lhs->npix;
  ssize_t start = posrelat(luaL_checkinteger(L, 2), l);
  ssize_t end = posrelat(luaL_optinteger(L, 3, -1), l);
  if (start <= end) {
    pixbuf *result = pixbuf_new(L, end - start + 1, lhs->nchan);
    memcpy(result->values, lhs->values + lhs->nchan * (start - 1),
           lhs->nchan * (end - start + 1));
    return 1;
  } else {
    pixbuf_new(L, 0, lhs->nchan);
    return 1;
  }
}

static int pixbuf_tostring_lua(lua_State *L) {
  pixbuf *buffer = pixbuf_from_lua_arg(L, 1);

  luaL_Buffer result;
  luaL_buffinit(L, &result);

  luaL_addchar(&result, '[');
  int p = 0;
  for (size_t i = 0; i < buffer->npix; i++) {
    if (i > 0) {
      luaL_addchar(&result, ',');
    }
    luaL_addchar(&result, '(');
    for (size_t j = 0; j < buffer->nchan; j++, p++) {
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

LROT_BEGIN(pixbuf_map, NULL, LROT_MASK_INDEX | LROT_MASK_EQ)
  LROT_TABENTRY ( __index, pixbuf_map )
  LROT_FUNCENTRY( __eq, pixbuf_eq_lua )

  LROT_FUNCENTRY( __concat, pixbuf_concat_lua )
  LROT_FUNCENTRY( __tostring, pixbuf_tostring_lua )

  LROT_FUNCENTRY( channels, pixbuf_channels_lua )
  LROT_FUNCENTRY( dump, pixbuf_dump_lua )
  LROT_FUNCENTRY( fade, pixbuf_fade_lua )
  LROT_FUNCENTRY( fadeI, pixbuf_fadeI_lua )
  LROT_FUNCENTRY( fill, pixbuf_fill_lua )
  LROT_FUNCENTRY( get, pixbuf_get_lua )
  LROT_FUNCENTRY( replace, pixbuf_replace_lua )
  LROT_FUNCENTRY( map, pixbuf_map_lua )
  LROT_FUNCENTRY( mix, pixbuf_mix_lua )
  LROT_FUNCENTRY( mix4I5, pixbuf_mix4I5_lua )
  LROT_FUNCENTRY( power, pixbuf_power_lua )
  LROT_FUNCENTRY( powerI, pixbuf_powerI_lua )
  LROT_FUNCENTRY( set, pixbuf_set_lua )
  LROT_FUNCENTRY( shift, pixbuf_shift_lua )
  LROT_FUNCENTRY( size, pixbuf_size_lua )
  LROT_FUNCENTRY( sub, pixbuf_sub_lua )
LROT_END(pixbuf_map, NULL, LROT_MASK_INDEX | LROT_MASK_EQ)

LROT_BEGIN(pixbuf, NULL, 0)
  LROT_NUMENTRY( FADE_IN, PIXBUF_FADE_IN )
  LROT_NUMENTRY( FADE_OUT, PIXBUF_FADE_OUT )

  LROT_NUMENTRY( SHIFT_CIRCULAR, PIXBUF_SHIFT_CIRCULAR )
  LROT_NUMENTRY( SHIFT_LOGICAL, PIXBUF_SHIFT_LOGICAL )

  LROT_FUNCENTRY( newBuffer, pixbuf_new_lua )
LROT_END(pixbuf, NULL, 0)

int luaopen_pixbuf(lua_State *L) {
  luaL_rometatable(L, PIXBUF_METATABLE, LROT_TABLEREF(pixbuf_map));
  lua_pushrotable(L, LROT_TABLEREF(pixbuf));
  return 1;
}

NODEMCU_MODULE(PIXBUF, "pixbuf", pixbuf, luaopen_pixbuf);
