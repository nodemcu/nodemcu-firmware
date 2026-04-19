#include "module.h"
#include "lauxlib.h"

#include <string.h>
#include <stdlib.h>

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

// Cribbed from lua/lstrlib.c
static ssize_t posrelat(ssize_t pos, size_t len) {
  /* relative string position: negative means back from end */
  if (pos < 0)
    pos += (ssize_t)len + 1;
  return (pos >= 0) ? pos : 0;
}

// Models how string.sub treats i
static int posrelat_start(int start, int l) {
  start = posrelat(start, l);
  if (start < 1) start = 1;
  return start;
}

// Models how string.sub treats j
static int posrelat_end(int end, int l) {
  end = posrelat(end, l);
  if (end > (ssize_t)l) end = (ssize_t)l;
  return end;
}

const size_t pixbuf_channels(pixbuf *p) {
  return p->nchan;
}

const size_t pixbuf_size(pixbuf *p) {
  return p->npix * p->nchan;
}

uint8_t *pixbuf_values(pixbuf *buffer) {
  return buffer->values_ptr;
}

static void *strided_memcpy(
  void *dst,
  const void *src,
  size_t npix,
  size_t nchan,
  ssize_t dstride,
  ssize_t sstride
) {
  uint8_t *d = (uint8_t *)dst;
  uint8_t *s = (uint8_t *)src;
  for (int i = 0; i < npix; ++i) {
    for (int j = 0; j < nchan; ++j) {
      d[j] = s[j];
    }
    d += dstride;
    s += sstride;
  }
  return dst;
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
  if ((leds > 8192) || (chans > 32)) {
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
  *(int *)&buffer->base_ref = LUA_REFNIL;
  *(uint8_t* *)&buffer->values_ptr = buffer->values;
  *(signed int *)&buffer->stride = chans;

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

static pixbuf *pixbuf_slice(
    lua_State *L,
    pixbuf *base,
    ssize_t start,
    ssize_t end,
    ssize_t step
) {
  // NOTE start and end are zero-indexed and are assumed to be in bounds
  // of base.

  size_t size = sizeof(pixbuf);
  // This view won't include any pixels of its own.
  pixbuf *buffer = (pixbuf*)lua_newuserdata(L, size);  // +1

  // Associate its metatable
  luaL_getmetatable(L, PIXBUF_METATABLE);  // +1
  lua_setmetatable(L, -2);  // -1

  // Save led strip size
  *(size_t *)&buffer->npix = (end - start + step - 1) / step;
  *(size_t *)&buffer->nchan = base->nchan;
  *(signed int *)&buffer->stride = base->stride * step;

  lua_pushvalue(L, 1); // +1
  *(int *)&buffer->base_ref = luaL_ref(L, LUA_REGISTRYINDEX); // -1
  *(uint8_t* *)&buffer->values_ptr = &pixbuf_values(base)[start * base->stride];

  return buffer;
}

static int pixbuf_gc_lua(lua_State *L) {
  pixbuf *buffer = pixbuf_from_lua_arg(L, 1);
  luaL_unref(L, LUA_REGISTRYINDEX, buffer->base_ref);
  return 0;
}

static int pixbuf_base_lua(lua_State *L) {
  pixbuf *buffer = pixbuf_from_lua_arg(L, 1);
  lua_rawgeti(L, LUA_REGISTRYINDEX, buffer->base_ref);
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

  uint8_t *const values = pixbuf_values(buffer);
  strided_memcpy(
      values,
      pixbuf_values(lhs),
      lhs->npix,
      buffer->nchan,
      buffer->stride,
      lhs->stride
  );
  strided_memcpy(
      values + pixbuf_size(lhs),
      pixbuf_values(rhs),
      rhs->npix,
      buffer->nchan,
      buffer->stride,
      rhs->stride
  );

  return 1;
}

static int pixbuf_channels_lua(lua_State *L) {
  pixbuf *buffer = pixbuf_from_lua_arg(L, 1);
  lua_pushinteger(L, buffer->nchan);
  return 1;
}

static int pixbuf_dump_lua(lua_State *L) {
  pixbuf *buffer = pixbuf_from_lua_arg(L, 1);

  if (pixbuf_size(buffer) == 0) {
    lua_pushliteral(L, "");
  } else {
    // Make a contiguous copy
    char* buf = malloc(pixbuf_size(buffer));
    strided_memcpy(
        buf,
        pixbuf_values(buffer),
        buffer->npix,
        buffer->nchan,
        buffer->nchan,
        buffer->stride
    );
    lua_pushlstring(L, buf, pixbuf_size(buffer));
    free(buf);
  }
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
    const size_t npix = lhs->npix;
    const size_t nchan = lhs->nchan;
    const uint8_t *lhs_value = pixbuf_values(lhs);
    const uint8_t *rhs_value = pixbuf_values(rhs);
    for(size_t i = 0; i < npix; i++) {
      for (size_t j = 0; j < nchan; j++) {
        if(lhs_value[j] != rhs_value[j]) {
          res = false;
          i = npix;
          break;
        }
      }
      lhs_value += lhs->stride;
      rhs_value += rhs->stride;
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

  uint8_t *p = pixbuf_values(buffer);
  for (size_t i = 0; i < buffer->npix; i++, p+=buffer->stride) {
    for (size_t j = 0; j < buffer->nchan; ++j) {
      if (direction == PIXBUF_FADE_OUT)
      {
        p[j] /= fade;
      }
      else
      {
        // as fade in can result in value overflow, an int is used to perform the check afterwards
        int val = p[j] * fade;
        p[j] = MIN(255, val);
      }
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

  uint8_t *p = pixbuf_values(buffer);
  for (size_t i = 0; i < buffer->npix; i++, p+=buffer->stride) {
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
  uint8_t *values = pixbuf_values(buffer);
  for (size_t i = 0; i < buffer->nchan; i++) {
    values[i] = luaL_checkinteger(L, 2+i);
  }

  /* Fill the rest of the pixels from the first */
  strided_memcpy(
      values + buffer->stride,
      values,
      buffer->npix - 1,
      buffer->nchan,
      buffer->stride,
      0
  );

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
  memcpy(tmp, &pixbuf_values(buffer)[buffer->stride*led], channels);

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

  const int ilo = posrelat_start(luaL_optinteger(L, 4, 1), buffer1->npix) - 1;
  const int ihi = posrelat_end(luaL_optinteger(L, 5, buffer1->npix), buffer1->npix) - 1;

  luaL_argcheck(L, ihi >= ilo, 3, "Buffer limits out of order");

  size_t npix = ihi - ilo + 1;

  luaL_argcheck(L, npix == outbuf->npix, 1, "Output buffer wrong size");

  pixbuf *buffer2 = pixbuf_opt_from_lua_arg(L, 6);
  const int ilo2 = buffer2 ? posrelat_start(luaL_optinteger(L, 7, 1), buffer2->npix) - 1 : 0;

  if (buffer2) {
    luaL_argcheck(L, ilo2 + npix <= buffer2->npix, 6, "Second buffer too short");
  }

  const uint8_t *const buffer1_values = pixbuf_values(buffer1);
  const uint8_t *const buffer2_values = buffer2 ? pixbuf_values(buffer2) : NULL;
  uint8_t *const outbuf_values = pixbuf_values(outbuf);
  for (size_t p = 0; p < npix; p++) {
    lua_pushvalue(L, 2);
    for (size_t c = 0; c < buffer1->nchan; c++) {
      lua_pushinteger(L, buffer1_values[(ilo + p) * buffer1->stride + c]);
    }
    if (buffer2) {
      for (size_t c = 0; c < buffer2->nchan; c++) {
        lua_pushinteger(L, buffer2_values[(ilo2 + p) * buffer2->stride + c]);
      }
    }
    lua_call(L, buffer1->nchan + (buffer2 ? buffer2->nchan : 0), outbuf->nchan);
    for (size_t c = 0; c < outbuf->nchan; c++) {
      outbuf_values[p * outbuf->stride + outbuf->nchan - c - 1] = luaL_checkinteger(L, -1);
      lua_pop(L, 1);
    }
  }

  lua_settop(L, 1);
  return 1;
}

struct mix_source {
  int factor;
  const uint8_t *values;
  ptrdiff_t stride;
};

static uint32_t pixbuf_mix_clamp(int32_t v) {
  if (v <   0) { return   0; }
  if (v > 255) { return 255; }
  return v;
}

/* This one can sum straightforwardly, channel by channel */
static void pixbuf_mix_raw(pixbuf *out, size_t n_src, struct mix_source* srcs) {
  const size_t npix = out->npix;
  const size_t nchan = out->nchan;

  uint8_t *const out_values = pixbuf_values(out);
  for (size_t p = 0; p < npix; p++) {
    for (size_t c = 0; c < nchan; c++) {
      int32_t val = 0;
      for (size_t s = 0; s < n_src; s++) {
        const struct mix_source *src = &srcs[s];
        val += (int32_t)src->values[p*src->stride + c] * src->factor;
      }

      val += 128; // rounding instead of floor
      val /= 256; // do not use implemetation dependant right shift

      out_values[p * out->stride + c] = (uint8_t)pixbuf_mix_clamp(val);
    }
  }
}

/* Mix intensity-mediated three-color pixbufs.
 *
 * XXX This is untested in real hardware; do they actually behave like this?
 */
static void pixbuf_mix_i3(pixbuf *out, size_t ibits, size_t n_src,
    struct mix_source* srcs) {
  uint8_t *const out_values = pixbuf_values(out);

  for(size_t p = 0; p < out->npix; p++) {
    int32_t sums[3] = { 0, 0, 0 };

    for (size_t s = 0; s < n_src; s++) {
      for (size_t c = 0; c < 3; c++) {
        const struct mix_source *src = &srcs[s];
        const ptrdiff_t ss = src->stride;
        sums[c] += (int32_t)src->values[ss*p+c+1] // color channel
                   * src->values[ss*p]            // global intensity
                   * src->factor;                // user factor

      }
    }

    uint32_t pmaxc = 0;
    for (size_t c = 0; c < 3; c++) {
      pmaxc = sums[c] > pmaxc ? sums[c] : pmaxc;
    }

    size_t maxgi;
    if (pmaxc == 0) {
      /* Zero value */
      memset(&out_values[out->stride*p], 0, 4);
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

    out_values[out->stride*p] = maxgi;
    for (size_t c = 0; c < 3; c++) {
      out_values[out->stride*p+c+1] = pixbuf_mix_clamp(
        (sums[c] + 256 * maxgi - 127) / (256 * maxgi)
      );
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
    sources[src].values = pixbuf_values(src_buffer);
    sources[src].stride = src_buffer->stride;
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
  uint8_t *value = pixbuf_values(buffer);
  for (size_t i = 0; i < buffer->npix; i++, value+=buffer->stride) {
    for (size_t j = 0; j < buffer->nchan; j++) {
      total += value[j];
    }
  }

  lua_pushinteger(L, total);
  return 1;
}

// Returns the total of all channels, intensity-style
static int pixbuf_powerI_lua(lua_State *L) {
  pixbuf *buffer = pixbuf_from_lua_arg(L, 1);

  int total = 0;
  uint8_t *value = pixbuf_values(buffer);
  for (size_t i = 0; i < buffer->npix; i++, value+=buffer->stride) {
    int inten = value[0];
    for (size_t j = 1; j < buffer->nchan; j++) {
      total += inten * value[j];
    }
  }

  lua_pushinteger(L, total);
  return 1;
}

static int pixbuf_replace_lua(lua_State *L) {
  pixbuf *buffer = pixbuf_from_lua_arg(L, 1);
  ptrdiff_t start = posrelat_start(luaL_optinteger(L, 3, 1), buffer->npix);
  size_t channels = buffer->nchan;

  uint8_t *src;
  size_t srcLen;
  ssize_t srcStride;

  if (lua_type(L, 2) == LUA_TSTRING) {
    size_t length;
    src = (uint8_t *) lua_tolstring(L, 2, &length);
    srcLen = length / channels;
    srcStride = channels;
  } else {
    pixbuf *rhs = pixbuf_from_lua_arg(L, 2);
    luaL_argcheck(L, rhs->nchan == buffer->nchan, 2, "buffers have different channels");
    src = pixbuf_values(rhs);
    srcLen = rhs->npix;
    srcStride = rhs->stride;
  }

  luaL_argcheck(L, srcLen + start - 1 <= buffer->npix, 2, "does not fit into destination");

  strided_memcpy(
      pixbuf_values(buffer) + (start - 1) * buffer->stride,
      src,
      srcLen,
      channels,
      buffer->stride,
      srcStride
  );

  return 0;
}

static int pixbuf_set_lua(lua_State *L) {

  pixbuf *buffer = pixbuf_from_lua_arg(L, 1);
  const int led = luaL_checkinteger(L, 2) - 1;
  const size_t channels = buffer->nchan;
  const ptrdiff_t stride = buffer->stride;
  uint8_t *values = pixbuf_values(buffer);

  luaL_argcheck(L, led >= 0 && led < buffer->npix, 2, "index out of range");

  int type = lua_type(L, 3);
  if(type == LUA_TTABLE)
  {
    for (size_t i = 0; i < channels; i++)
    {
      lua_rawgeti(L, 3, i+1);
      values[stride*led+i] = lua_tointeger(L, -1);
      lua_pop(L, 1);
    }
  }
  else if(type == LUA_TSTRING)
  {
    size_t len;
    const char *buf = lua_tolstring(L, 3, &len);
    const size_t npix = (len + channels - 1) / channels;

    // Overflow check
    if (led + npix > buffer->npix ) {
      return luaL_error(L, "string size will exceed strip length");
    }
    if ( len % channels != 0 ) {
      return luaL_error(L, "string does not contain whole LEDs");
    }

    strided_memcpy(
        &values[stride*led],
        buf,
        npix,
        channels,
        stride,
        channels
    );
  }
  else
  {
    luaL_argcheck(L, lua_gettop(L) <= 2 + channels, 2 + channels,
        "extra values given");

    for (size_t i = 0; i < channels; i++)
    {
      values[stride*led+i] = luaL_checkinteger(L, 3+i);
    }
  }

  lua_settop(L, 1);
  return 1;
}

struct pixbuf_shift_pix {
  enum pixbuf_shift type;
    // 0 <= offset <= npix
  size_t offset;
    // offset <= window + offset <= npix
  size_t window;
    // -window <= shift <= window
  ssize_t shift;
};

static uint32_t gcd(uint32_t n1, uint32_t n2) {
  uint32_t n3;
  while (n2 != 0) {
    n3 = n1;
    n1 = n2;
    n2 = n3 % n2;
  }
  return n1;
}

static void pixbuf_shift_circular(pixbuf *buffer, struct pixbuf_shift_pix *sp) {
  size_t shift = (sp->shift + sp->window) % sp->window;
  if (shift == 0) return;

  const size_t nchan = buffer->nchan;
  const ptrdiff_t stride = buffer->stride;

  // Point at the beginning of the window
  uint8_t *v = &pixbuf_values(buffer)[sp->offset * stride];

  uint8_t tmp[nchan];
  // If `window` and `shift` are not co-prime then the inner loop won't cover
  // all pixels. e.g. if `window` is 12 and `shift` is 8 then it will only
  // rotate 3 pixels. It will need to happen 4 times in total in this example.
  for (int k = 0; k < gcd(sp->window, shift); ++k, v+=stride) {
    size_t i_src = 0;
    size_t i_dst = shift;
    // Copy dst into tmp
    for (int j = 0; j < nchan; ++j) tmp[j] = v[i_dst * stride + j];
    do {
      // Copy src into dst
      for (int j = 0; j < nchan; ++j) v[i_dst * stride + j] = v[i_src * stride + j];
      i_dst = i_src;
      i_src = (sp->window + i_src - shift) % sp->window;
    } while (i_src != shift);
    // Copy tmp into dst
    for (int j = 0; j < nchan; ++j) v[i_dst * stride + j] = tmp[j];
  }
}

static void pixbuf_shift_logical(pixbuf *buffer, struct pixbuf_shift_pix *sp) {
  /* Logical shifts don't require a temporary buffer, so we just move bytes */
  ptrdiff_t stride = buffer->stride;
  size_t nchan = buffer->nchan;

  // Point at the beginning of the window
  uint8_t *v = &pixbuf_values(buffer)[sp->offset * stride];

  uint8_t zero[nchan];
  for (int i = 0; i < nchan; i++) zero[i] = 0;

  size_t u_shift = labs(sp->shift);
  if (sp->shift > 0) {
    // We need to copy higher-indexed pixels first. Achieve this by
    // negating the stride and pointing at the end of the window.
    v += (sp->window - 1) * stride;
    stride = -stride;
  }

  strided_memcpy(
    v,
    &v[u_shift * stride],
    sp->window - u_shift,
    nchan,
    stride,
    stride
  );
  strided_memcpy(
    &v[(sp->window - u_shift) * stride],
    zero,
    u_shift,
    nchan,
    stride,
    0
  );
}

/* XXX for backwards-compat with ws2812_effects; deprecated and should be removed */
void pixbuf_shift(pixbuf *b, struct pixbuf_shift_params *sp) {
  size_t nchan = b->nchan;
  // This code path remains only to support WS2812_EFFECTS and doesn't
  // support strided buffers.
  lua_assert(nchan == b->stride);
  struct pixbuf_shift_pix spp;
  spp.type = sp->type;
  spp.offset = sp->offset / nchan;
  spp.window = sp->window / nchan;
  spp.shift = (sp->shiftLeft ? sp->shift : -sp->shift) / nchan;

  switch(sp->type) {
    case PIXBUF_SHIFT_LOGICAL: return pixbuf_shift_logical(b, &spp);
    case PIXBUF_SHIFT_CIRCULAR: return pixbuf_shift_circular(b, &spp);
  }
}

int pixbuf_shift_lua(lua_State *L) {
  struct pixbuf_shift_pix spp;

  pixbuf *buffer = pixbuf_from_lua_arg(L, 1);

  const int shift = luaL_checkinteger(L, 2);
  const unsigned shift_type = luaL_optinteger(L, 3, PIXBUF_SHIFT_LOGICAL);
  const int pos_start = posrelat_start(luaL_optinteger(L, 4, 1), buffer->npix);
  const int pos_end = posrelat_end(luaL_optinteger(L, 5, -1), buffer->npix);

  spp.shift = shift;

  switch(shift_type) {
  case PIXBUF_SHIFT_LOGICAL:
  case PIXBUF_SHIFT_CIRCULAR:
    spp.type = shift_type;
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

  spp.offset = (pos_start - 1);
  spp.window = (pos_end - pos_start + 1);

  size_t u_shift = labs(spp.shift);
  if (u_shift > buffer->npix) {
    return luaL_argerror(L, 2, "shifting more elements than buffer size");
  }
  if (u_shift > spp.window) {
    return luaL_argerror(L, 2, "shifting more than sliced window");
  }

  switch(spp.type) {
    case PIXBUF_SHIFT_LOGICAL: pixbuf_shift_logical(buffer, &spp); break;
    case PIXBUF_SHIFT_CIRCULAR: pixbuf_shift_circular(buffer, &spp); break;
  }

  return 0;
}

/* XXX for backwards-compat with ws2812_effects; deprecated and should be removed */
void pixbuf_prepare_shift(pixbuf *buffer, struct pixbuf_shift_params *sp,
    int shift, enum pixbuf_shift type, int start, int end)
{
  // shift not yet supported for strided buffers
  lua_assert(buffer->stride == buffer->nchan);

  start = posrelat_start(start, buffer->npix);
  end = posrelat_end(end, buffer->npix);

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

int pixbuf_slice_lua(lua_State *L) {
  pixbuf *lhs = pixbuf_from_lua_arg(L, 1);

  size_t l = lhs->npix;
  ssize_t start = posrelat_start(luaL_optinteger(L, 2, 1), l);
  ssize_t end = posrelat_end(luaL_optinteger(L, 3, -1), l);
  signed int step = luaL_optinteger(L, 4, 1);
  if ((step > 0 && start <= end) ||
      (step < 0 && end <= start)) {
    pixbuf_slice(L, lhs, start - 1, end, step);
    return 1;
  } else {
    pixbuf_new(L, 0, lhs->nchan);
    return 1;
  }

  return 1;
}

static int pixbuf_stride_lua(lua_State *L) {
  pixbuf *buffer = pixbuf_from_lua_arg(L, 1);
  lua_pushinteger(L, buffer->stride);
  return 1;
}

static int pixbuf_sub_lua(lua_State *L) {
  pixbuf *lhs = pixbuf_from_lua_arg(L, 1);
  size_t l = lhs->npix;
  ssize_t start = posrelat_start(luaL_checkinteger(L, 2), l);
  ssize_t end = posrelat_end(luaL_optinteger(L, 3, -1), l);

  if (start <= end) {
    pixbuf *result = pixbuf_new(L, end - start + 1, lhs->nchan);
    strided_memcpy(
        pixbuf_values(result),
        pixbuf_values(lhs) + lhs->stride * (start - 1),
        (end - start + 1),
        lhs->nchan,
        result->stride,
        lhs->stride
    );
    return 1;
  } else {
    pixbuf_new(L, 0, lhs->nchan);
    return 1;
  }
}

static int pixbuf_tostring_lua(lua_State *L) {
  pixbuf *buffer = pixbuf_from_lua_arg(L, 1);
  uint8_t *values = pixbuf_values(buffer);

  luaL_Buffer result;
  luaL_buffinit(L, &result);

  luaL_addchar(&result, '[');
  ssize_t stride = buffer->stride;
  for (size_t i = 0; i < buffer->npix; i++, values+=stride) {
    if (i > 0) {
      luaL_addchar(&result, ',');
    }
    luaL_addchar(&result, '(');
    for (size_t j = 0; j < buffer->nchan; j++) {
      if (j > 0) {
        luaL_addchar(&result, ',');
      }
      char numbuf[5];
      sprintf(numbuf, "%d", values[j]);
      luaL_addstring(&result, numbuf);
    }
    luaL_addchar(&result, ')');
  }

  luaL_addchar(&result, ']');
  luaL_pushresult(&result);

  return 1;
}

LROT_BEGIN(pixbuf_map, NULL, LROT_MASK_INDEX | LROT_MASK_EQ | LROT_MASK_GC)
  /* https://nodemcu.readthedocs.io/en/dev/lua53/#rotables notes:
   * "Some ordering limitations apply", namely that entries beginning
   * with '__' must be first and must be sorted.
   */
  LROT_FUNCENTRY( __concat, pixbuf_concat_lua )
  LROT_FUNCENTRY( __eq, pixbuf_eq_lua )
  LROT_FUNCENTRY( __gc, pixbuf_gc_lua )
  LROT_TABENTRY ( __index, pixbuf_map )
  LROT_FUNCENTRY( __tostring, pixbuf_tostring_lua )

  LROT_FUNCENTRY( base, pixbuf_base_lua )
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
  LROT_FUNCENTRY( slice, pixbuf_slice_lua )
  LROT_FUNCENTRY( stride, pixbuf_stride_lua )
  LROT_FUNCENTRY( sub, pixbuf_sub_lua )
LROT_END(pixbuf_map, NULL, LROT_MASK_INDEX | LROT_MASK_EQ | LROT_MASK_GC)

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
