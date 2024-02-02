// Module for working with the rmt driver

#include <string.h>
#include "module.h"
#include "lauxlib.h"
#include "platform.h"
#include "platform_rmt.h"
#include "task/task.h"

#include "driver/rmt.h"

#include "common.h"

typedef struct {
  bool tx;
  int channel;
  int cb_ref;
  struct _lrmt_cb_params *rx_params;
} *lrmt_channel_t;

typedef struct _lrmt_cb_params {
  bool dont_call;
  bool rx_shutting_down;
  rmt_channel_t channel;
  int cb_ref;
  int data_ref;
  int rc;
  size_t len;
  rmt_item32_t *data;
} lrmt_cb_params;

static task_handle_t cb_task_id;

static int get_divisor(lua_State *L, int index) {
  int bittime = luaL_checkinteger(L, index);

  int divisor = bittime / 12500;   // 80MHz clock

  luaL_argcheck(L, divisor >= 1 && divisor <= 255, index, "Bit time out of range");

  return divisor;
}

static int configure_channel(lua_State *L, rmt_config_t *config, rmt_mode_t mode) {
  lrmt_channel_t ud = (lrmt_channel_t)lua_newuserdata(L, sizeof(*ud));
  if (!ud) return luaL_error(L, "not enough memory");
  memset(ud, 0, sizeof(*ud));
  luaL_getmetatable(L, "rmt.channel");
  lua_setmetatable(L, -2);

  // We have allocated the channel -- must free it if the rest of this method fails
  int channel = platform_rmt_allocate(config->mem_block_num, mode);

  if (channel < 0) {
    return luaL_error(L, "no spare RMT channel");
  }

  config->channel = channel;

  ud->channel = channel;
  ud->tx = mode == RMT_MODE_TX;

  esp_err_t rc = rmt_config(config);
  if (rc) {
    platform_rmt_release(config->channel);
    return luaL_error(L, "Failed to configure RMT");
  }

  rc = rmt_driver_install(config->channel, config->mem_block_num * 600, 0);
  if (rc) {
    platform_rmt_release(config->channel);
    return luaL_error(L, "Failed to install RMT driver");
  }

  return 1;
}

static int lrmt_txsetup(lua_State *L) {
  int gpio = luaL_checkinteger(L, 1);
  int divisor = get_divisor(L, 2);

  // We will set the channel later
  rmt_config_t config = RMT_DEFAULT_CONFIG_TX(gpio, 0);
  config.clk_div = divisor;

  if (lua_type(L, 3) == LUA_TTABLE) {
    lua_getfield(L, 3, "carrier_hz");
    int hz = lua_tointeger(L, -1);
    if (hz) {
      config.tx_config.carrier_freq_hz = hz;
      config.tx_config.carrier_en = true;
    }
    lua_pop(L, 1);

    lua_getfield(L, 3, "carrier_duty");
    int duty = lua_tointeger(L, -1);
    if (duty) {
      config.tx_config.carrier_duty_percent = duty;
    }
    lua_pop(L, 1);

    lua_getfield(L, 3, "idle_level");
    if (!lua_isnil(L, -1)) {
      int level = lua_tointeger(L, -1);
      config.tx_config.idle_level = level;
      config.tx_config.idle_output_en = true;
    }
    lua_pop(L, 1);

    lua_getfield(L, 3, "invert");
    if (lua_toboolean(L, -1)) {
      config.flags |= RMT_CHANNEL_FLAGS_INVERT_SIG;
    }
    lua_pop(L, 1);

    lua_getfield(L, 3, "slots");
    int slots = lua_tointeger(L, -1);
    if (slots > 0 && slots <= 8) {
      config.mem_block_num = slots;
    }
    lua_pop(L, 1);
  }

  configure_channel(L, &config, RMT_MODE_TX);
  lua_pushinteger(L, divisor * 12500);
  return 2;
}

static int lrmt_rxsetup(lua_State *L) {
  int gpio = luaL_checkinteger(L, 1);
  int divisor = get_divisor(L, 2);

  // We will set the channel later
  rmt_config_t config = RMT_DEFAULT_CONFIG_RX(gpio, 0);
  config.clk_div = divisor;
  config.rx_config.idle_threshold = 65535;

  if (lua_type(L, 3) == LUA_TTABLE) {
    lua_getfield(L, 3, "invert");
    if (lua_toboolean(L, -1)) {
      config.flags |= RMT_CHANNEL_FLAGS_INVERT_SIG;
    }
    lua_pop(L, 1);

    lua_getfield(L, 3, "filter_ticks");
    if (!lua_isnil(L, -1)) {
      int ticks = lua_tointeger(L, -1);
      if (ticks < 0 || ticks > 255) {
        return luaL_error(L, "filter_ticks must be in the range 0 - 255");
      }
      config.rx_config.filter_ticks_thresh = ticks;
      config.rx_config.filter_en = true;
    }
    lua_pop(L, 1);

    lua_getfield(L, 3, "idle_threshold");
    if (!lua_isnil(L, -1)) {
      int threshold = lua_tointeger(L, -1);
      if (threshold < 0 || threshold > 65535) {
        return luaL_error(L, "idle_threshold must be in the range 0 - 65535");
      }
      config.rx_config.idle_threshold = threshold;
    }
    lua_pop(L, 1);

    lua_getfield(L, 3, "slots");
    int slots = lua_tointeger(L, -1);
    if (slots > 0 && slots <= 8) {
      config.mem_block_num = slots;
    }
    lua_pop(L, 1);
  }

  configure_channel(L, &config, RMT_MODE_RX);
  lua_pushinteger(L, divisor * 12500);
  return 2;
}

static void free_transmit_wait_params(lua_State *L, lrmt_cb_params *p) {
  if (!p->data) {
    luaL_unref(L, LUA_REGISTRYINDEX, p->cb_ref);
    luaL_unref(L, LUA_REGISTRYINDEX, p->data_ref);
  }
  free(p);
}

static void handle_receive(void *param) {
  lrmt_cb_params *p = (lrmt_cb_params *) param;

  RingbufHandle_t rb = NULL;

  //get RMT RX ringbuffer
  rmt_get_ringbuf_handle(p->channel, &rb);
  // Start receive
  rmt_rx_start(p->channel, true);
  while (!p->rx_shutting_down) {
    size_t length = 0;
    rmt_item32_t *items = NULL;

    items = (rmt_item32_t *) xRingbufferReceive(rb, &length, 50 / portTICK_PERIOD_MS);
    if (items && length) {
      lrmt_cb_params *rx_params = malloc(sizeof(lrmt_cb_params) + length);
      if (rx_params) {
        memset(rx_params, 0, sizeof(*rx_params));
	memcpy(rx_params + 1, items, length);
        rx_params->cb_ref = p->cb_ref;
	rx_params->data = (void *) (rx_params + 1);
	rx_params->len = length;
        rx_params->channel = p->channel;
        task_post_high(cb_task_id, (task_param_t) rx_params);
      } else {
	printf("Unable allocate receive data memory\n");
      }
    }
    if (items) {
      vRingbufferReturnItem(rb, (void *) items);
    }
  }
  rmt_rx_stop(p->channel);

  p->dont_call = true;
  task_post_high(cb_task_id, (task_param_t) p);

  /* Destroy this task */
  vTaskDelete(NULL);
}

static int lrmt_on(lua_State *L) {
  lrmt_channel_t ud = (lrmt_channel_t)luaL_checkudata(L, 1, "rmt.channel");
  if (ud->tx) {
    return luaL_error(L, "Cannot receive on a TX channel");
  }

  luaL_argcheck(L, !strcmp(lua_tostring(L, 2), "data") , 2, "Must be 'data'");

  luaL_argcheck(L, lua_type(L, 3) == LUA_TFUNCTION, 3, "Must be a function");

  if (ud->rx_params) {
    return luaL_error(L, "Can only call 'on' once");
  }

  // We have a callback
  lrmt_cb_params *params = (lrmt_cb_params *) malloc(sizeof(*params));
  if (!params) {
    return luaL_error(L, "Cannot allocate memory");
  }
  memset(params, 0, sizeof(*params));

  lua_pushvalue(L, 3);
  params->cb_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  ud->rx_params = params;
  params->channel = ud->channel;

  xTaskCreate(handle_receive, "rmt-rx-receiver", 3000, params, 2, NULL);

  return 0;
}

static void wait_for_transmit(void *param) {
  lrmt_cb_params *p = (lrmt_cb_params *) param;
  esp_err_t rc = rmt_wait_tx_done(p->channel, 10000 / portTICK_PERIOD_MS);

  p->rc = rc;
  task_post_high(cb_task_id, (task_param_t) p);

  /* Destroy this task */
  vTaskDelete(NULL);
}

static int lrmt_send(lua_State *L) {
  lrmt_channel_t ud = (lrmt_channel_t)luaL_checkudata(L, 1, "rmt.channel");
  if (!ud->tx) {
    return luaL_error(L, "Cannot send on an RX channel");
  }
  int string_index = 2;

  if (lua_type(L, 2) == LUA_TTABLE) {
    lua_getfield(L, 2, "concat");
    lua_pushvalue(L, 2);
    lua_pushstring(L, "");
    lua_call(L, 2, 1);
    string_index = -1;
  }

  size_t len;
  const char *data = lua_tolstring(L, string_index, &len);
  if (!data || !len) {
    return 0;
  }

  if (len & 1) {
    return luaL_error(L, "Length must be a multiple of 2");
  }

  if (len & 3) {
    // Just tack on a "\0\0" -- this is needed as the hardware can
    // only deal with multiple of 4 bytes.
    luaL_Buffer b;
    luaL_buffinit(L, &b);
    luaL_addlstring(&b, data, len);
    luaL_addlstring(&b, "\0\0", 2);
    luaL_pushresult(&b);
    data = lua_tolstring(L, -1, &len);
    string_index = -1;
  }

  bool wait_for_done = true;

  if (lua_type(L, 3) == LUA_TFUNCTION) {
    // We have a callback
    lrmt_cb_params *params = (lrmt_cb_params *) malloc(sizeof(*params));
    if (!params) {
      return luaL_error(L, "Cannot allocate memory");
    }
    memset(params, 0, sizeof(*params));

    params->channel = ud->channel;

    lua_pushvalue(L, 3);
    params->cb_ref = luaL_ref(L, LUA_REGISTRYINDEX);
    lua_pushvalue(L, string_index);
    params->data_ref = luaL_ref(L, LUA_REGISTRYINDEX);
    xTaskCreate(wait_for_transmit, "rmt-tx-waiter", 1024, params, 2, NULL);
    wait_for_done = false;
  }

  // We want to transmit it
  rmt_write_items(ud->channel, (rmt_item32_t *) data, len / sizeof(rmt_item32_t), wait_for_done);
  return 0;
}

static int lrmt_close(lua_State *L) {
  lrmt_channel_t ud = (lrmt_channel_t)luaL_checkudata(L, 1, "rmt.channel");

  if (ud->channel >= 0) {
    if (ud->rx_params) {
      // We need to stop the listening task
      ud->rx_params->rx_shutting_down = true;
    } else {
      rmt_driver_uninstall(ud->channel);
      platform_rmt_release(ud->channel);
    }
    ud->channel = -1;
  }

  return 0;
}

static void cb_task(task_param_t param, task_prio_t prio) {
  lrmt_cb_params *p = (lrmt_cb_params *) param;
  lua_State *L = lua_getstate();

  if (!p->dont_call) {
    lua_rawgeti (L, LUA_REGISTRYINDEX, p->cb_ref);
    if (p->data) {
      lua_pushlstring(L, (char *) p->data, p->len);
    } else {
      lua_pushinteger(L, p->rc);
    }

    int res = luaL_pcallx(L, 1, 0);
    if (res) {
      printf("rmt callback threw an error\n");
    }
  }

  if (p->rx_shutting_down) {
    rmt_driver_uninstall(p->channel);
    platform_rmt_release(p->channel);
  }

  free_transmit_wait_params(L, p);
}

// Module function map
LROT_BEGIN(rmt_channel, NULL, LROT_MASK_GC_INDEX)
  LROT_FUNCENTRY( __gc,            lrmt_close )
  LROT_TABENTRY ( __index,         rmt_channel )
  LROT_FUNCENTRY( on,              lrmt_on )
  LROT_FUNCENTRY( close,           lrmt_close )
  LROT_FUNCENTRY( send,            lrmt_send )
LROT_END(rmt_channel, NULL, LROT_MASK_GC_INDEX)

LROT_BEGIN(rmt, NULL, LROT_MASK_INDEX)
  LROT_TABENTRY ( __index,         rmt )
  LROT_FUNCENTRY( txsetup,         lrmt_txsetup )
  LROT_FUNCENTRY( rxsetup,         lrmt_rxsetup )
LROT_END(rmt, NULL, LROT_MASK_INDEX)

int luaopen_rmt(lua_State *L) {
  luaL_rometatable(L, "rmt.channel", LROT_TABLEREF(rmt_channel));  // create metatable
  cb_task_id = task_get_id(cb_task);
  return 0;
}

NODEMCU_MODULE(RMT, "rmt", rmt, luaopen_rmt);
