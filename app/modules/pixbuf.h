#ifndef APP_MODULES_PIXBUF_H_
#define APP_MODULES_PIXBUF_H_

typedef struct pixbuf {
  const size_t npix;
  const size_t nchan;

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
