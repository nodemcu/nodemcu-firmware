/*
  Functions for integrating U8glib into the nodemcu platform.
*/

#include "lauxlib.h"

#include "c_stdlib.h"

#include "u8g.h"
#include "u8g_glue.h"


// ***************************************************************************
// Generic framebuffer device and RLE comm driver
//
uint8_t u8g_dev_gen_fb_fn(u8g_t *u8g, u8g_dev_t *dev, uint8_t msg, void *arg)
{
  switch(msg)
  {
    case U8G_DEV_MSG_PAGE_FIRST:
      // tell comm driver to start new framebuffer
      u8g_SetChipSelect(u8g, dev, 1);
      break;
    case U8G_DEV_MSG_PAGE_NEXT:
      {
        u8g_pb_t *pb = (u8g_pb_t *)(dev->dev_mem);
        if ( u8g_pb_WriteBuffer(pb, u8g, dev) == 0 )
          return 0;
      }
      break;
  }

  return u8g_dev_pb8v1_base_fn(u8g, dev, msg, arg);
}

static int bit_at( uint8_t *buf, int line, int x )
{
    uint8_t byte = buf[x];

    return buf[x] & (1 << line) ? 1 : 0;
}

struct _lu8g_fbrle_item
{
    uint8_t start_x;
    uint8_t len;
};

struct _lu8g_fbrle_line
{
    uint8_t num_valid;
    struct _lu8g_fbrle_item items[0];
};

uint8_t u8g_com_esp8266_fbrle_fn(u8g_t *u8g, uint8_t msg, uint8_t arg_val, void *arg_ptr)
{
    struct _lu8g_userdata_t *lud = (struct _lu8g_userdata_t *)u8g;

    switch(msg)
    {
    case U8G_COM_MSG_INIT:
        // allocate memory -> done
        // init buffer
        break;

    case U8G_COM_MSG_CHIP_SELECT:
        if (arg_val == 1) {
            // new frame starts
            if (lud->cb_ref != LUA_NOREF) {
                // fire callback with nil argument
                lua_State *L = lua_getstate();
                lua_rawgeti( L, LUA_REGISTRYINDEX, lud->cb_ref );
                lua_pushnil( L );
                lua_call( L, 1, 0 );
            }
        }
        break;

    case U8G_COM_MSG_WRITE_SEQ:
    case U8G_COM_MSG_WRITE_SEQ_P:
        {
            uint8_t xwidth = u8g->pin_list[U8G_PI_EN];
            size_t fbrle_line_size = sizeof( struct _lu8g_fbrle_line ) + sizeof( struct _lu8g_fbrle_item ) * (xwidth/2);
            int num_lines = arg_val / (xwidth/8);
            uint8_t *buf = (uint8_t *)arg_ptr;

            struct _lu8g_fbrle_line *fbrle_line;
            if (!(fbrle_line = (struct _lu8g_fbrle_line *)c_malloc( fbrle_line_size ))) {
                break;
            }

            for (int line = 0; line < num_lines; line++) {
                int start_run = -1;
                fbrle_line->num_valid = 0;

                for (int x = 0; x < xwidth; x++) {
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

                    if (fbrle_line->num_valid >= xwidth/2) break;
                }

                // active run?
                if (start_run >= 0 && fbrle_line->num_valid < xwidth/2) {
                    fbrle_line->items[fbrle_line->num_valid].start_x = start_run;
                    fbrle_line->items[fbrle_line->num_valid++].len = xwidth - start_run;
                }

                // line done, trigger callback
                if (lud->cb_ref != LUA_NOREF) {
                    lua_State *L = lua_getstate();

                    lua_rawgeti( L, LUA_REGISTRYINDEX, lud->cb_ref );
                    lua_pushlstring( L, (const char *)fbrle_line, fbrle_line_size );
                    lua_call( L, 1, 0 );
                }
            }

            c_free( fbrle_line );
        }
        break;
    }
    return 1;
}
