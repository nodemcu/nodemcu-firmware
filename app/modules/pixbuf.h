#ifndef APP_MODULES_PIXBUF_H_
#define APP_MODULES_PIXBUF_H_

enum pixbuf_type {
  PIXBUF_TYPE_RGB = 0 /* 3-color: Red, Green, Blue order */,
  PIXBUF_TYPE_GRB     /* 3-color: Green, Red, Blue order */,
  PIXBUF_TYPE_RGBW    /* 4-color: RGB + White */,
  PIXBUF_TYPE_GRBW    /* 4-color: GRB + White */,
  PIXBUF_TYPE_WWA     /* 3-color: warm White, cold White, Amber */,
  PIXBUF_TYPE_I5BGR   /* 3-color (Blue, Green, Red) after 5-bit Intensity */,
};

static const enum pixbuf_type PIXBUF_NTYPES = PIXBUF_TYPE_I5BGR + 1;

typedef struct pixbuf {
  const size_t npix;
  const uint8_t type;

  /* Flexible Array Member; true size is npix * pixbuf_channels_for(type) */
  uint8_t values[];
} pixbuf;

enum pixbuf_fade {
  PIXBUF_FADE_IN,
  PIXBUF_FADE_OUT
};

enum pixbuf_shift {
  PIXBUF_SHIFT_LOGICAL,
  PIXBUF_SHIFT_CIRCULAR
};

pixbuf *pixbuf_from_lua_arg(lua_State *, int);
const size_t pixbuf_size(pixbuf *);

// Exported for backwards compat with ws2812 module
int pixbuf_new_lua(lua_State *);

/*
 * WS2812_EFFECTS does pixbuf manipulation directly in C, which isn't the
 * intended use case, but for backwards compat, we export just what it needs.
 * Move this struct to pixbuf.c and mark these exports static instead once
 * WS2812_EFFECTS is no more.
 */
struct pixbuf_shift_params {
  enum pixbuf_shift type;
    // 0 <= offset <= buffer length
  size_t offset;
    // 0 <= window + offset <= buffer length
  size_t window;
    // 0 <= shift <= window_size
  size_t shift;
  bool   shiftLeft;
};
void pixbuf_shift(pixbuf *, struct pixbuf_shift_params *);
void pixbuf_prepare_shift(pixbuf *, struct pixbuf_shift_params *,
    int val, enum pixbuf_shift, int start, int end);
const size_t pixbuf_channels(pixbuf *);
/* end WS2812_EFFECTS exports */

#endif
