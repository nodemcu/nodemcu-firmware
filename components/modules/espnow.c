/*
 * Copyright 2024 Dius Computing Pty Ltd. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the
 *   distribution.
 * - Neither the name of the copyright holders nor the names of
 *   its contributors may be used to endorse or promote products derived
 *   from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * @author Jade Mattsson <jmattsson@dius.com.au>
 */
#include "module.h"
#include "platform.h"
#include "ip_fmt.h"
#include "task/task.h"
#include "lauxlib.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "esp_err.h"

#if ESP_NOW_ETH_ALEN != 6
# error "MAC address length assumption broken"
#endif
#if ESP_NOW_MAX_DATA_LEN > 0xffff
# error "Update len field in received_packet_t"
#endif

typedef struct {
  uint8_t src[6];
  uint8_t dst[6];
  int16_t rssi;
  uint16_t len; // ESP_NOW_MAX_DATA_LEN is currently 250
  char data[0];
} received_packet_t;

typedef struct {
  uint8_t dst[6];
  esp_now_send_status_t status;
} sent_packet_t;


static task_handle_t espnow_task = 0;

static int recv_ref = LUA_NOREF;
static int sent_ref = LUA_NOREF;


// --- Helper functions -----------------------------------

static int *cb_ref_for_event(const char *name)
{
  if (strcmp("receive", name) == 0)
    return &recv_ref;
  else if (strcmp("sent", name) == 0)
    return &sent_ref;
  else
    return NULL;
}

static int hexval(lua_State *L, char c)
{
  if (c >= '0' && c <= '9')
    return c - '0';
  else if (c >= 'A' && c <= 'F')
    return c - 'A' + 10;
  else if (c >= 'a' && c <= 'f')
    return c - 'a' + 10;
  else
    return luaL_error(L, "invalid hex digit '%c'", c);
}

// TODO: share with wifi_sta.c
static bool parse_mac(const char *str, uint8_t out[6])
{
  const char *fmts[] = {
    "%hhx%hhx%hhx%hhx%hhx%hhx",
    "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
    "%hhx-%hhx-%hhx-%hhx-%hhx-%hhx",
    "%hhx %hhx %hhx %hhx %hhx %hhx",
    NULL
  };
  for (unsigned i = 0; fmts[i]; ++i)
  {
    if (sscanf (str, fmts[i],
          &out[0], &out[1], &out[2], &out[3], &out[4], &out[5]) == 6)
      return true;
  }
  return false;
}


static void on_receive(const esp_now_recv_info_t *info, const uint8_t *data, int len)
{
  if (len < 0)
    return; // Don't do that.

  received_packet_t *p = malloc(sizeof(received_packet_t) + len);
  if (!p)
  {
    NODE_ERR("out of memory\n");
    return;
  }
  memcpy(p->src, info->src_addr, sizeof(p->src));
  memcpy(p->dst, info->des_addr, sizeof(p->dst));
  p->rssi = info->rx_ctrl->rssi;
  p->len = len;
  memcpy(p->data, data, len);

  if (!task_post_high(espnow_task, (task_param_t)p))
  {
    NODE_ERR("lost esp-now packet; task queue full\n");
    free(p);
  }
}


static void on_sent(const uint8_t *mac, esp_now_send_status_t status)
{
  sent_packet_t *p = malloc(sizeof(sent_packet_t));
  if (!p)
  {
    NODE_ERR("out of memory\n");
    return;
  }
  memcpy(p->dst, mac, sizeof(p->dst));
  p->status = status;

  if (!task_post_medium(espnow_task, (task_param_t)p))
  {
    NODE_ERR("lost esp-now packet; task queue full\n");
    free(p);
  }
}


static void espnow_task_fn(task_param_t param, task_prio_t prio)
{
  lua_State* L = lua_getstate();
  int top = lua_gettop(L);
  luaL_checkstack(L, 3, "");

  if (prio == TASK_PRIORITY_HIGH) // received packet
  {
    received_packet_t *p = (received_packet_t *)param;
    if (recv_ref != LUA_NOREF)
    {
      lua_rawgeti (L, LUA_REGISTRYINDEX, recv_ref);
      lua_createtable(L, 0, 4); // src, dst, rssi, data
      char mac[MAC_STR_SZ];
      macstr(mac, p->src);
      lua_pushstring(L, mac);
      lua_setfield(L, -2, "src");
      macstr(mac, p->dst);
      lua_pushstring(L, mac);
      lua_setfield(L, -2, "dst");
      lua_pushinteger(L, p->rssi);
      lua_setfield(L, -2, "rssi");
      lua_pushlstring(L, p->data, p->len);
      lua_setfield(L, -2, "data");
      luaL_pcallx(L, 1, 0);
    }
    free(p);
  }
  else if (prio == TASK_PRIORITY_MEDIUM) // sent
  {
    sent_packet_t *p = (sent_packet_t *)param;
    if (sent_ref != LUA_NOREF)
    {
      lua_rawgeti (L, LUA_REGISTRYINDEX, sent_ref);
      char dst[MAC_STR_SZ];
      macstr(dst, p->dst);
      lua_pushstring(L, dst);
      if (p->status == ESP_NOW_SEND_SUCCESS)
        lua_pushinteger(L, 1);
      else
        lua_pushnil(L);
      luaL_pcallx(L, 2, 0);
    }
    free(p);
  }

  lua_settop(L, top); // restore original before exit
}


static void err_check(lua_State *L, esp_err_t err)
{
  if (err != ESP_OK)
    luaL_error(L, "%s", esp_err_to_name(err));
}


// --- Lua interface functions -----------------------------------

static int lespnow_start(lua_State *L)
{
  err_check(L, esp_now_init());
  err_check(L, esp_now_register_recv_cb(on_receive));
  err_check(L, esp_now_register_send_cb(on_sent));
  return 0;
}

static int lespnow_stop(lua_State *L)
{
  err_check(L, esp_now_unregister_send_cb());
  err_check(L, esp_now_unregister_recv_cb());
  err_check(L, esp_now_deinit());
  return 0;
}

static int lespnow_getversion(lua_State *L)
{
  uint32_t ver;
  err_check(L, esp_now_get_version(&ver));
  lua_pushinteger(L, ver);
  return 1;
}

// espnow.on('sent' or 'received', cb)
//   sent -> cb('ma:ca:dd:00:11:22', status)
//   received -> cb({ src=, dst=, data=, rssi= })
static int lespnow_on(lua_State *L)
{
  const char *evtname = luaL_checkstring(L, 1);
  int *ref = cb_ref_for_event(evtname);
  if (!ref)
    return luaL_error(L, "unknown event type");

  if (lua_isnoneornil(L, 2))
  {
    if (*ref != LUA_NOREF)
    {
      luaL_unref(L, LUA_REGISTRYINDEX, *ref);
      *ref = LUA_NOREF;
    }
  }
  else if (lua_isfunction(L, 2))
  {
    if (*ref != LUA_NOREF)
      luaL_unref(L, LUA_REGISTRYINDEX, *ref);
    lua_pushvalue(L, 2);
    *ref = luaL_ref(L, LUA_REGISTRYINDEX);
  }
  else
    return luaL_error(L, "expected function");
  return 0;
}

// espnow.send('ma:ca:dd:00:11:22' or nil, str)
static int lespnow_send(lua_State *L)
{
  const char *mac = luaL_optstring(L, 1, NULL);
  size_t payloadlen = 0;
  const char *payload = luaL_checklstring(L, 2, &payloadlen);

  uint8_t peer_addr[6];
  if (mac && !parse_mac(mac, peer_addr))
    return luaL_error(L, "bad peer address");

  err_check(L, esp_now_send(
    mac ? peer_addr : NULL, (const uint8_t *)payload, payloadlen));
  return 0;
}


// espnow.addpeer('ma:ca:dd:00:11:22', { lmk=, channel=, encrypt= }
static int lespnow_addpeer(lua_State *L)
{
  esp_now_peer_info_t peer_info;
  memset(&peer_info, 0, sizeof(peer_info));

  const char *mac = luaL_checkstring(L, 1);

  wifi_mode_t mode = WIFI_MODE_NULL;
  err_check(L, esp_wifi_get_mode(&mode));

  switch (mode)
  {
    case WIFI_MODE_STA: peer_info.ifidx = WIFI_IF_STA; break;
    case WIFI_MODE_APSTA: // fall-through
    case WIFI_MODE_AP: peer_info.ifidx = WIFI_IF_AP; break;
    default: return luaL_error(L, "No wifi interface found");
  }

  if (!parse_mac(mac, peer_info.peer_addr))
    return luaL_error(L, "bad peer address");

  lua_settop(L, 2); // Discard excess parameters, to ensure we have space
  if (lua_istable(L, 2))
  {
    lua_getfield(L, 2, "encrypt");
    peer_info.encrypt = luaL_optint(L, -1, 0);
    lua_pop(L, 1);
    if (peer_info.encrypt)
    {
      lua_getfield(L, 2, "lmk");
      size_t lmklen = 0;
      const char *lmkstr = luaL_checklstring(L, -1, &lmklen);
      lua_pop(L, 1);
      if (lmklen != 2*sizeof(peer_info.lmk))
        return luaL_error(L, "LMK must be %d hex digits", 2*ESP_NOW_KEY_LEN);
      for (unsigned i = 0; i < sizeof(peer_info.lmk); ++i)
        peer_info.lmk[i] =
          (hexval(L, lmkstr[i*2]) << 4) + hexval(L, lmkstr[i*2 +1]);
    }

    lua_getfield(L, 2, "channel");
    peer_info.channel = luaL_optint(L, -1, 0);
    lua_pop(L, 1);
  }

  err_check(L, esp_now_add_peer(&peer_info));
  return 0;
}

// espnow.delpeer('ma:ca:dd:00:11:22')
static int lespnow_delpeer(lua_State *L)
{
  const char *mac = luaL_checkstring(L, 1);
  uint8_t peer_addr[6];
  if (!parse_mac(mac, peer_addr))
    return luaL_error(L, "bad peer address");

  err_check(L, esp_now_del_peer(peer_addr));
  return 0;
}

static int lespnow_setpmk(lua_State *L)
{
  uint8_t pmk[ESP_NOW_KEY_LEN] = { 0, };
  size_t len = 0;
  const char *str = luaL_checklstring(L, 1, &len);
  if (len != sizeof(pmk) * 2)
    return luaL_error(L, "PMK must be %d hex digits", 2*ESP_NOW_KEY_LEN);
  for (unsigned i = 0; i < sizeof(pmk); ++i)
    pmk[i] = (hexval(L, str[i*2]) << 4) + hexval(L, str[i*2 +1]);

  err_check(L, esp_now_set_pmk(pmk));
  return 0;
}

static int lespnow_setwakewindow(lua_State *L)
{
  int n = luaL_checkint(L, 1);
  if (n < 0 || n > 0xffff)
    return luaL_error(L, "wake window out of bounds");
  err_check(L, esp_now_set_wake_window(n));
  return 0;
}

static int lespnow_init(lua_State *L)
{
  espnow_task = task_get_id(espnow_task_fn);
  return 0;
}

LROT_BEGIN(espnow, NULL, 0)
  LROT_FUNCENTRY( start,         lespnow_start )
  LROT_FUNCENTRY( stop,          lespnow_stop )
  LROT_FUNCENTRY( getversion,    lespnow_getversion )
  LROT_FUNCENTRY( on,            lespnow_on ) // 'receive', 'sent'
  LROT_FUNCENTRY( send,          lespnow_send )
  LROT_FUNCENTRY( addpeer,       lespnow_addpeer )
  LROT_FUNCENTRY( delpeer,       lespnow_delpeer )
  LROT_FUNCENTRY( setpmk,        lespnow_setpmk )
  LROT_FUNCENTRY( setwakewindow, lespnow_setwakewindow )
LROT_END(espnow, NULL, 0)

NODEMCU_MODULE(ESPNOW, "espnow", espnow, lespnow_init);
