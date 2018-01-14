// Module for interfacing with PCM functionality

#include "module.h"
#include "lauxlib.h"
#include "task/task.h"
#include "c_string.h"
#include "c_stdlib.h"

#include "pcm.h"
#include "pcm_drv.h"


#define GET_PUD() pud_t *pud = (pud_t *)luaL_checkudata(L, 1, "pcm.driver"); \
  cfg_t *cfg = &(pud->cfg);

#define UNREF_CB(_cb) luaL_unref( L, LUA_REGISTRYINDEX, _cb ); \
  _cb = LUA_NOREF;

#define COND_REF(_cond) _cond ? luaL_ref( L, LUA_REGISTRYINDEX ) : LUA_NOREF


// task handles
task_handle_t pcm_data_vu_task, pcm_data_play_task, pcm_start_play_task;

static void dispatch_callback( lua_State *L, int self_ref, int cb_ref, int returns )
{
  if (cb_ref != LUA_NOREF) {
    lua_rawgeti( L, LUA_REGISTRYINDEX, cb_ref );
    lua_rawgeti( L, LUA_REGISTRYINDEX, self_ref );
    lua_call( L, 1, returns );
  }
}

static int pcm_drv_free( lua_State *L )
{
  GET_PUD();

  UNREF_CB( cfg->cb_data_ref );
  UNREF_CB( cfg->cb_drained_ref );
  UNREF_CB( cfg->cb_paused_ref );
  UNREF_CB( cfg->cb_stopped_ref );
  UNREF_CB( cfg->cb_vu_ref );
  UNREF_CB( cfg->self_ref );

  if (cfg->bufs[0].data) {
    c_free( cfg->bufs[0].data );
    cfg->bufs[0].data = NULL;
  }
  if (cfg->bufs[1].data) {
    c_free( cfg->bufs[1].data );
    cfg->bufs[1].data = NULL;
  }

  return 0;
}

// Lua: drv:close()
static int pcm_drv_close( lua_State *L )
{
  GET_PUD();

  pud->drv->close( cfg );

  return pcm_drv_free( L );
}

// Lua: drv:stop(self)
static int pcm_drv_stop( lua_State *L )
{
  GET_PUD();

  // throttle ISR and reader
  cfg->isr_throttled = -1;

  pud->drv->stop( cfg );

  // invalidate the buffers
  cfg->bufs[0].empty = cfg->bufs[1].empty = TRUE;

  dispatch_callback( L, cfg->self_ref, cfg->cb_stopped_ref, 0 );

  return 0;
}

// Lua: drv:pause(self)
static int pcm_drv_pause( lua_State *L )
{
  GET_PUD();

  // throttle ISR and reader
  cfg->isr_throttled = -1;

  pud->drv->stop( cfg );

  dispatch_callback( L, cfg->self_ref, cfg->cb_paused_ref, 0 );

  return 0;
}

static void pcm_start_play( task_param_t param, uint8 prio )
{
  lua_State *L = lua_getstate();
  pud_t *pud = (pud_t *)param;
  cfg_t *cfg = &(pud->cfg);

  // stop driver before starting it in case it hasn't been stopped from Lua
  pud->drv->stop( cfg );

  if (!pud->drv->play( cfg )) {
      luaL_error( L, "pcm driver start" );
  }

  // unthrottle ISR and reader
  pud->cfg.isr_throttled = 0;
}

// Lua: drv:play(self, rate)
static int pcm_drv_play( lua_State *L )
{
  GET_PUD();

  cfg->rate = luaL_optinteger( L, 2, PCM_RATE_8K );

  luaL_argcheck( L, (cfg->rate >= PCM_RATE_1K) && (cfg->rate <= PCM_RATE_16K), 2, "invalid bit rate" );

  if (cfg->self_ref == LUA_NOREF) {
    lua_pushvalue( L, 1 );  // copy self userdata to the top of stack
    cfg->self_ref = luaL_ref( L, LUA_REGISTRYINDEX );
  }

  // schedule actions for play in separate task since drv:play() might have been called
  // in the callback fn of pcm_data_play() which in turn gets called when starting play...
  task_post_low( pcm_start_play_task, (os_param_t)pud );

  return 0;
}

// Lua: drv.on(self, event, cb_fn)
static int pcm_drv_on( lua_State *L )
{
  size_t len;
  const char *event;
  uint8_t is_func = FALSE;

  GET_PUD();

  event = luaL_checklstring( L, 2, &len );

  if ((lua_type( L, 3 ) == LUA_TFUNCTION) ||
      (lua_type( L, 3 ) == LUA_TLIGHTFUNCTION)) {
    lua_pushvalue( L, 3 );  // copy argument (func) to the top of stack
    is_func = TRUE;
  }

  if ((len == 4) && (c_strcmp( event, "data" ) == 0)) {
    luaL_unref( L, LUA_REGISTRYINDEX, cfg->cb_data_ref);
    cfg->cb_data_ref = COND_REF( is_func );
  } else if ((len == 7) && (c_strcmp( event, "drained" ) == 0)) {
    luaL_unref( L, LUA_REGISTRYINDEX, cfg->cb_drained_ref);
    cfg->cb_drained_ref = COND_REF( is_func );
  } else if ((len == 6) && (c_strcmp( event, "paused" ) == 0)) {
    luaL_unref( L, LUA_REGISTRYINDEX, cfg->cb_paused_ref);
    cfg->cb_paused_ref = COND_REF( is_func );
  } else if ((len == 7) && (c_strcmp( event, "stopped" ) == 0)) {
    luaL_unref( L, LUA_REGISTRYINDEX, cfg->cb_stopped_ref);
    cfg->cb_stopped_ref = COND_REF( is_func );
  } else if ((len == 2) && (c_strcmp( event, "vu" ) == 0)) {
    luaL_unref( L, LUA_REGISTRYINDEX, cfg->cb_vu_ref);
    cfg->cb_vu_ref = COND_REF( is_func );

    int freq = luaL_optinteger( L, 4, 10 );
    luaL_argcheck( L, (freq > 0) && (freq <= 200), 4, "invalid range" );
    cfg->vu_freq = (uint8_t)freq;
  } else {
    if (is_func) {
      // need to pop pushed function arg
      lua_pop( L, 1 );
    }
    return luaL_error( L, "method not supported" );
  }

  return 0;
}

// Lua: pcm.new( type, pin )
static int pcm_new( lua_State *L )
{
  pud_t *pud = (pud_t *) lua_newuserdata( L, sizeof( pud_t ) );
  cfg_t *cfg = &(pud->cfg);
  int driver;

  cfg->rbuf_idx = cfg->fbuf_idx = 0;
  cfg->isr_throttled = -1;  // start ISR and reader in throttled mode

  driver = luaL_checkinteger( L, 1 );
  luaL_argcheck( L, (driver >= 0) && (driver < PCM_DRIVER_END), 1, "invalid driver" );

  cfg->self_ref      = LUA_NOREF;
  cfg->cb_data_ref   = cfg->cb_drained_ref = LUA_NOREF;
  cfg->cb_paused_ref = cfg->cb_stopped_ref = LUA_NOREF;
  cfg->cb_vu_ref     = LUA_NOREF;

  cfg->bufs[0].buf_size = cfg->bufs[1].buf_size = 0;
  cfg->bufs[0].data     = cfg->bufs[1].data     = NULL;
  cfg->bufs[0].len      = cfg->bufs[1].len      = 0;
  cfg->bufs[0].rpos     = cfg->bufs[1].rpos     = 0;
  cfg->bufs[0].empty    = cfg->bufs[1].empty    = TRUE;

  cfg->vu_freq         = 10;

  if (driver == PCM_DRIVER_SD) {
    cfg->pin = luaL_checkinteger( L, 2 );
    MOD_CHECK_ID(sigma_delta, cfg->pin);

    pud->drv = &pcm_drv_sd;

    pud->drv->init( cfg );

    /* set its metatable */
    lua_pushvalue( L, -1 );  // copy self userdata to the top of stack
    luaL_getmetatable( L, "pcm.driver" );
    lua_setmetatable( L, -2 );

    return 1;
  } else {
    pud->drv = NULL;
    return 0;
  }
}


static const LUA_REG_TYPE pcm_driver_map[] = {
  { LSTRKEY( "play" ),    LFUNCVAL( pcm_drv_play ) },
  { LSTRKEY( "pause" ),   LFUNCVAL( pcm_drv_pause ) },
  { LSTRKEY( "stop" ),    LFUNCVAL( pcm_drv_stop ) },
  { LSTRKEY( "close" ),   LFUNCVAL( pcm_drv_close ) },
  { LSTRKEY( "on" ),      LFUNCVAL( pcm_drv_on ) },
  { LSTRKEY( "__gc" ),    LFUNCVAL( pcm_drv_free ) },
  { LSTRKEY( "__index" ), LROVAL( pcm_driver_map ) },
  { LNILKEY, LNILVAL }
};

// Module function map
static const LUA_REG_TYPE pcm_map[] = {
  { LSTRKEY( "new" ),      LFUNCVAL( pcm_new ) },
  { LSTRKEY( "SD" ),       LNUMVAL( PCM_DRIVER_SD ) },
  { LSTRKEY( "RATE_1K" ),  LNUMVAL( PCM_RATE_1K ) },
  { LSTRKEY( "RATE_2K" ),  LNUMVAL( PCM_RATE_2K ) },
  { LSTRKEY( "RATE_4K" ),  LNUMVAL( PCM_RATE_4K ) },
  { LSTRKEY( "RATE_5K" ),  LNUMVAL( PCM_RATE_5K ) },
  { LSTRKEY( "RATE_8K" ),  LNUMVAL( PCM_RATE_8K ) },
  { LSTRKEY( "RATE_10K" ), LNUMVAL( PCM_RATE_10K ) },
  { LSTRKEY( "RATE_12K" ), LNUMVAL( PCM_RATE_12K ) },
  { LSTRKEY( "RATE_16K" ), LNUMVAL( PCM_RATE_16K ) },
  { LNILKEY, LNILVAL }
};

int luaopen_pcm( lua_State *L ) {
  luaL_rometatable( L, "pcm.driver", (void *)pcm_driver_map );  // create metatable

  pcm_data_vu_task    = task_get_id( pcm_data_vu );
  pcm_data_play_task  = task_get_id( pcm_data_play );
  pcm_start_play_task = task_get_id( pcm_start_play );
  return 0;
}

NODEMCU_MODULE(PCM, "pcm", pcm_map, luaopen_pcm);
