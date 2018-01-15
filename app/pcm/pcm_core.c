/*
  This file contains core routines for the PCM sub-system.

  pcm_data_play_task()
    Triggered by the driver ISR when further data for play mode is required.
    It handles the play buffer allocation and forwards control to 'data' and 'drained'
    callbacks in Lua land.

  pcm_data_rec_task() - n/a yet
    Triggered by the driver ISR when data for record mode is available.

*/

#include "lauxlib.h"
#include "task/task.h"
#include "c_string.h"
#include "c_stdlib.h"

#include "pcm.h"

static void dispatch_callback( lua_State *L, int self_ref, int cb_ref, int returns )
{
  if (cb_ref != LUA_NOREF) {
    lua_rawgeti( L, LUA_REGISTRYINDEX, cb_ref );
    lua_rawgeti( L, LUA_REGISTRYINDEX, self_ref );
    lua_call( L, 1, returns );
  }
}

void pcm_data_vu( task_param_t param, uint8 prio )
{
  cfg_t *cfg = (cfg_t *)param;
  lua_State *L = lua_getstate();

  if (cfg->cb_vu_ref != LUA_NOREF) {
    lua_rawgeti( L, LUA_REGISTRYINDEX, cfg->cb_vu_ref );
    lua_rawgeti( L, LUA_REGISTRYINDEX, cfg->self_ref );
    lua_pushnumber( L, (LUA_NUMBER)(cfg->vu_peak) );
    lua_call( L, 2, 0 );
  }
}

void pcm_data_play( task_param_t param, uint8 prio )
{
  cfg_t *cfg = (cfg_t *)param;
  pcm_buf_t *buf = &(cfg->bufs[cfg->fbuf_idx]);
  size_t string_len;
  const char *data = NULL;
  uint8_t need_pop = FALSE;
  lua_State *L = lua_getstate();

  // retrieve new data from callback
  if ((cfg->isr_throttled >= 0) &&
      (cfg->cb_data_ref != LUA_NOREF)) {
    dispatch_callback( L, cfg->self_ref, cfg->cb_data_ref, 1 );
    need_pop = TRUE;

    if (lua_type( L, -1 ) == LUA_TSTRING) {
      data = lua_tolstring( L, -1, &string_len );
      if (string_len > buf->buf_size) {
        uint8_t *new_data = (uint8_t *) c_malloc( string_len );
        if (new_data) {
          if (buf->data) c_free( buf->data );
          buf->buf_size = string_len;
          buf->data = new_data;
        }
      }
    }
  }

  if (data) {
    size_t to_copy = string_len > buf->buf_size ? buf->buf_size : string_len;
    c_memcpy( buf->data, data, to_copy );

    buf->rpos  = 0;
    buf->len   = to_copy;
    buf->empty = FALSE;
    dbg_platform_gpio_write( PLATFORM_GPIO_HIGH );
    lua_pop( L, 1 );

    if (cfg->isr_throttled > 0) {
      uint8_t other_buf = cfg->fbuf_idx ^ 1;

      if (cfg->bufs[other_buf].empty) {
        // rerun data callback to get next buffer chunk
        dbg_platform_gpio_write( PLATFORM_GPIO_LOW );
        cfg->fbuf_idx = other_buf;
        pcm_data_play( param, 0 );
      }
      // unthrottle ISR
      cfg->isr_throttled = 0;
    }
  } else {
    dbg_platform_gpio_write( PLATFORM_GPIO_HIGH );
    if (need_pop) lua_pop( L, 1 );

    if (cfg->isr_throttled > 0) {
      // ISR found no further data
      // this was the last invocation of the reader task, fire drained cb

      cfg->isr_throttled = -1;

      dispatch_callback( L, cfg->self_ref, cfg->cb_drained_ref, 0 );
    }
  }

}
