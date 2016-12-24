//
// This module allows performance monitoring by looking at
// the PC at regular intervals and building a histogram
// 
// perf.start(start, end, nbins[, pc offset on stack])
// perf.stop()  -> total sample, samples outside range, table { addr -> count , .. }


#include "ets_sys.h"
#include "os_type.h"
#include "osapi.h"
#include "c_stdlib.h"

#include "module.h"
#include "lauxlib.h"
#include "platform.h"
#include "hw_timer.h"
#include "cpu_esp8266.h"

typedef struct {
  int ref;
  uint32_t start;
  uint32_t bucket_shift;
  uint32_t bucket_count;
  uint32_t total_samples;
  uint32_t outside_samples;
  uint32_t bucket[1];
} DATA;

static DATA *data;
extern char _flash_used_end[];

#define TIMER_OWNER ((os_param_t) 'p')

static void ICACHE_RAM_ATTR hw_timer_cb(os_param_t p)
{
  (void) p;
  uint32_t stackaddr;

  if (data) {
    uint32_t pc;
    asm (
      "rsr   %0, EPC1;"    /* read out the EPC */
      :"=r"(pc)
    );

    uint32_t bucket_number = (pc - data->start) >> data->bucket_shift;
    if (bucket_number < data->bucket_count) {
      data->bucket[bucket_number]++;
    } else {
      data->outside_samples++;
    }
    data->total_samples++;
  }
}

static int perf_start(lua_State *L)
{
  uint32_t start = luaL_optinteger(L, 1, 0x40000000);
  uint32_t end = luaL_optinteger(L, 2, (uint32_t) _flash_used_end);
  uint32_t bins = luaL_optinteger(L, 3, 1024);

  if (end <= start) {
    luaL_error(L, "end must be larger than start");
  }

  uint32_t binsize = (end - start + bins - 1) / bins;

  // Round up to a power of two
  int shift;
  binsize = binsize - 1;
  for (shift = 0; binsize > 0; shift++) {
    binsize >>= 1;
  }

  bins = (end - start + (1 << shift) - 1) / (1 << shift);

  size_t data_size = sizeof(DATA) + bins * sizeof(uint32_t);
  DATA *d = (DATA *) lua_newuserdata(L, data_size);
  memset(d, 0, data_size);
  d->ref = luaL_ref(L, LUA_REGISTRYINDEX);
  d->start = start;
  d->bucket_shift = shift;
  d->bucket_count = bins;

  if (data) {
    lua_unref(L, data->ref);
  }

  data = d;

  // Start the timer
  if (!platform_hw_timer_init(TIMER_OWNER, FRC1_SOURCE, TRUE)) {
    // Failed to init the timer
    data = NULL;
    lua_unref(L, d->ref);
    luaL_error(L, "Unable to initialize timer");
  }

  platform_hw_timer_set_func(TIMER_OWNER, hw_timer_cb, 0);
  platform_hw_timer_arm_us(TIMER_OWNER, 50);

  return 0;
}

static int perf_stop(lua_State *L)
{
  if (!data) {
    return 0;
  }

  // stop the timer
  platform_hw_timer_close(TIMER_OWNER);

  DATA *d = data;
  data = NULL;

  lua_pushnumber(L, d->total_samples);
  lua_pushnumber(L, d->outside_samples);
  lua_newtable(L);
  int i;
  uint32_t addr = d->start;
  for (i = 0; i < d->bucket_count; i++, addr += (1 << d->bucket_shift)) {
    if (d->bucket[i]) {
      lua_pushnumber(L, addr);
      lua_pushnumber(L, d->bucket[i]);
      lua_settable(L, -3);
    }
  }

  lua_pushnumber(L, 1 << d->bucket_shift);

  lua_unref(L, d->ref);

  return 4;
}

static const LUA_REG_TYPE perf_map[] = {
  { LSTRKEY( "start" ),   LFUNCVAL( perf_start ) },
  { LSTRKEY( "stop" ),    LFUNCVAL( perf_stop ) },
  { LNILKEY, LNILVAL }
};

NODEMCU_MODULE(PERF, "perf", perf_map, NULL);
