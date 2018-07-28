
// Note: This file is intended to be shared between esp8266 and esp32 platform

#include "u8x8.h"
#include "lauxlib.h"

#include "u8x8_nodemcu_hal.h"

#include "c_stdlib.h"


static const u8x8_display_info_t u8x8_fbrle_display_info =
{
  /* chip_enable_level = */ 0,
  /* chip_disable_level = */ 1,
  
  /* post_chip_enable_wait_ns = */ 0,
  /* pre_chip_disable_wait_ns = */ 0,
  /* reset_pulse_width_ms = */ 0,
  /* post_reset_wait_ms = */ 0,
  /* sda_setup_time_ns = */ 0,
  /* sck_pulse_width_ns = */ 0,
  /* sck_clock_hz = */ 0,
  /* spi_mode = */ 0,
  /* i2c_bus_clock_100kHz = */ 4,
  /* data_setup_time_ns = */ 0,
  /* write_pulse_width_ns = */ 0,
  /* tile_width = */ 16,
  /* tile_hight = */ 8,
  /* default_x_offset = */ 0,
  /* flipmode_x_offset = */ 0,
  /* pixel_width = */ 128,
  /* pixel_height = */ 64
};

static int bit_at( uint8_t *buf, int line, int x )
{
  return buf[x] & (1 << line) ? 1 : 0;
}

struct fbrle_item
{
  uint8_t start_x;
  uint8_t len;
};

struct fbrle_line
{
  uint8_t num_valid;
  struct fbrle_item items[0];
};

static uint8_t u8x8_d_fbrle(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
  u8g2_nodemcu_t *ext_u8g2 = (u8g2_nodemcu_t *)u8x8;

  switch(msg)
  {
  case U8X8_MSG_DISPLAY_SETUP_MEMORY:
    // forward to template display driver
    return ext_u8g2->overlay.template_display_cb(u8x8, msg, arg_int, arg_ptr);

  case U8X8_MSG_DISPLAY_INIT:
    //u8x8_d_helper_display_init(u8x8);
    ext_u8g2->overlay.fb_update_ongoing = 0;
    break;

  case U8X8_MSG_DISPLAY_SET_POWER_SAVE:
  case U8X8_MSG_DISPLAY_SET_FLIP_MODE:
    break;

#ifdef U8X8_WITH_SET_CONTRAST
  case U8X8_MSG_DISPLAY_SET_CONTRAST:
    break;
#endif

  case U8X8_MSG_DISPLAY_REFRESH:
    ext_u8g2->overlay.fb_update_ongoing = 0;
    break;
    
  case U8X8_MSG_DISPLAY_DRAW_TILE:
    if (ext_u8g2->overlay.fb_update_ongoing == 0) {
      // tell rfb callback that a new framebuffer starts
      if (ext_u8g2->overlay.rfb_cb_ref != LUA_NOREF) {
        // fire callback with nil argument
        lua_State *L = lua_getstate();
        lua_rawgeti( L, LUA_REGISTRYINDEX, ext_u8g2->overlay.rfb_cb_ref );
        lua_pushnil( L );
        lua_call( L, 1, 0 );
      }
      // and note ongoing framebuffer update
      ext_u8g2->overlay.fb_update_ongoing = 1;
    }

    {
      // TODO: transport tile_y, needs structural change!
      uint8_t tile_x = ((u8x8_tile_t *)arg_ptr)->x_pos;
      tile_x *= 8;
      tile_x += u8x8->x_offset;
      uint8_t tile_w = ((u8x8_tile_t *)arg_ptr)->cnt * 8;

      size_t fbrle_line_size = sizeof( struct fbrle_line ) + sizeof( struct fbrle_item ) * (tile_w/2);
      int num_lines = 8; /*arg_val / (xwidth/8);*/
      uint8_t *buf = ((u8x8_tile_t *)arg_ptr)->tile_ptr;

      struct fbrle_line *fbrle_line;
      if (!(fbrle_line = (struct fbrle_line *)c_malloc( fbrle_line_size ))) {
        break;
      }

      for (int line = 0; line < num_lines; line++) {
        int start_run = -1;
        fbrle_line->num_valid = 0;

        for (int x = tile_x; x < tile_x+tile_w; x++) {
          if (bit_at( buf, line, x ) == 0) {
            if (start_run >= 0) {
              // inside run, end it and enter result
              fbrle_line->items[fbrle_line->num_valid].start_x = start_run;
              fbrle_line->items[fbrle_line->num_valid++].len = x - start_run;
              //NODE_ERR( "         line: %d x: %d len: %d\n", line, start_run, x - start_run );
              start_run = -1;
            }
          } else {
            if (start_run < 0) {
              // outside run, start it
              start_run = x;
            }
          }

          if (fbrle_line->num_valid >= tile_w/2) break;
        }

        // active run?
        if (start_run >= 0 && fbrle_line->num_valid < tile_w/2) {
          fbrle_line->items[fbrle_line->num_valid].start_x = start_run;
          fbrle_line->items[fbrle_line->num_valid++].len = tile_w - start_run;
        }

        // line done, trigger callback
        if (ext_u8g2->overlay.rfb_cb_ref != LUA_NOREF) {
          lua_State *L = lua_getstate();

          lua_rawgeti( L, LUA_REGISTRYINDEX, ext_u8g2->overlay.rfb_cb_ref );
          lua_pushlstring( L, (const char *)fbrle_line, fbrle_line_size );
          lua_call( L, 1, 0 );
        }
      }

      c_free( fbrle_line );
    }
    break;

  default:
    return 0;
    break;
  }

  return 1;
}

uint8_t u8x8_d_overlay(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
  uint8_t res = 1;
  u8g2_nodemcu_t *ext_u8g2 = (u8g2_nodemcu_t *)u8x8;

  switch(msg)
  {
  case U8X8_MSG_DISPLAY_SETUP_MEMORY:
    // only call for hardware display
    if (ext_u8g2->overlay.hardware_display_cb)
      return ext_u8g2->overlay.hardware_display_cb(u8x8, msg, arg_int, arg_ptr);
    break;

  default:
    // forward all messages first to hardware display and then to fbrle
    if (ext_u8g2->overlay.hardware_display_cb)
      res = ext_u8g2->overlay.hardware_display_cb(u8x8, msg, arg_int, arg_ptr);
    u8x8_d_fbrle(u8x8, msg, arg_int, arg_ptr);
    break;
  }

  return res;
}
