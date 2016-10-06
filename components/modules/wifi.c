/*
 * Copyright 2016 Dius Computing Pty Ltd. All rights reserved.
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
 * @author Johny Mattsson <jmattsson@dius.com.au>
 */
#include "module.h"
#include "lauxlib.h"
#include "lextra.h"
#include "esp_wifi.h"
#include "wifi_common.h"
#include "ip_fmt.h"
#include "nodemcu_esp_event.h"
#include <string.h>

typedef void (*fill_cb_arg_fn) (lua_State *L, const system_event_t *evt);
typedef struct
{
  const char *name;
  system_event_id_t event_id;
  fill_cb_arg_fn fill_cb_arg;
} event_desc_t;


// Forward declarations
static void on_event (const system_event_t *evt);

static void sta_conn (lua_State *L, const system_event_t *evt);
static void sta_disconn (lua_State *L, const system_event_t *evt);
static void sta_authmode (lua_State *L, const system_event_t *evt);
static void sta_got_ip (lua_State *L, const system_event_t *evt);
static void ap_staconn (lua_State *L, const system_event_t *evt);
static void ap_stadisconn (lua_State *L, const system_event_t *evt);
static void ap_probe_req (lua_State *L, const system_event_t *evt);

static void empty_arg (lua_State *L, const system_event_t *evt) {}

static const event_desc_t events[] =
{
  { "sta_start",            SYSTEM_EVENT_STA_START,           empty_arg     },
  { "sta_stop",             SYSTEM_EVENT_STA_STOP,            empty_arg     },
  { "sta_connected",        SYSTEM_EVENT_STA_CONNECTED,       sta_conn      },
  { "sta_disconnected",     SYSTEM_EVENT_STA_DISCONNECTED,    sta_disconn   },
  { "sta_authmode_changed", SYSTEM_EVENT_STA_AUTHMODE_CHANGE, sta_authmode  },
  { "sta_got_ip",           SYSTEM_EVENT_STA_GOT_IP,          sta_got_ip    },

  { "ap_start",             SYSTEM_EVENT_AP_START,            empty_arg     },
  { "ap_stop",              SYSTEM_EVENT_AP_STOP,             empty_arg     },
  { "ap_sta_connected",     SYSTEM_EVENT_AP_STACONNECTED,     ap_staconn    },
  { "ap_sta_disconnected",  SYSTEM_EVENT_AP_STADISCONNECTED,  ap_stadisconn },
  { "ap_probe_req",         SYSTEM_EVENT_AP_PROBEREQRECVED,   ap_probe_req  }
};

#define ARRAY_LEN(a) (sizeof(a) / sizeof(a[0]))
static int event_cb[ARRAY_LEN(events)];

NODEMCU_ESP_EVENT(SYSTEM_EVENT_STA_START,           on_event);
NODEMCU_ESP_EVENT(SYSTEM_EVENT_STA_STOP,            on_event);
NODEMCU_ESP_EVENT(SYSTEM_EVENT_STA_CONNECTED,       on_event);
NODEMCU_ESP_EVENT(SYSTEM_EVENT_STA_DISCONNECTED,    on_event);
NODEMCU_ESP_EVENT(SYSTEM_EVENT_STA_AUTHMODE_CHANGE, on_event);
NODEMCU_ESP_EVENT(SYSTEM_EVENT_STA_GOT_IP,          on_event);

NODEMCU_ESP_EVENT(SYSTEM_EVENT_AP_START,            on_event);
NODEMCU_ESP_EVENT(SYSTEM_EVENT_AP_STOP,             on_event);
NODEMCU_ESP_EVENT(SYSTEM_EVENT_AP_STACONNECTED,     on_event);
NODEMCU_ESP_EVENT(SYSTEM_EVENT_AP_STADISCONNECTED,  on_event);
NODEMCU_ESP_EVENT(SYSTEM_EVENT_AP_PROBEREQRECVED,   on_event);


static int wifi_getmode (lua_State *L)
{
  wifi_mode_t mode;
  esp_err_t err = esp_wifi_get_mode (&mode);
  if (err != ESP_OK)
    return luaL_error (L, "failed to get mode, code %d", err);
  lua_pushinteger (L, mode);
  return 1;
}


static int event_idx_by_name (const char *name)
{
  for (unsigned i = 0; i < ARRAY_LEN(events); ++i)
    if (strcmp (events[i].name, name) == 0)
      return i;
  return -1;
}


static int event_idx_by_id (system_event_id_t id)
{
  for (unsigned i = 0; i < ARRAY_LEN(events); ++i)
    if (events[i].event_id == id)
      return i;
  return -1;
}


static void sta_conn (lua_State *L, const system_event_t *evt)
{
  lua_pushlstring (L,
    (const char *)evt->event_info.connected.ssid,
    evt->event_info.connected.ssid_len);
  lua_setfield (L, -2, "ssid");

  char bssid_str[MAC_STR_SZ];
  macstr (bssid_str, evt->event_info.connected.bssid);
  lua_pushstring (L, bssid_str);
  lua_setfield (L, -2, "bssid");

  lua_pushinteger (L, evt->event_info.connected.channel);
  lua_setfield (L, -2, "channel");

  lua_pushinteger (L, evt->event_info.connected.authmode);
  lua_setfield (L, -2, "auth");
}

static void sta_disconn (lua_State *L, const system_event_t *evt)
{
  lua_pushlstring (L,
    (const char *)evt->event_info.disconnected.ssid,
    evt->event_info.disconnected.ssid_len);
  lua_setfield (L, -2, "ssid");

  char bssid_str[MAC_STR_SZ];
  macstr (bssid_str, evt->event_info.disconnected.bssid);
  lua_pushstring (L, bssid_str);
  lua_setfield (L, -2, "bssid");

  lua_pushinteger (L, evt->event_info.disconnected.reason);
  lua_setfield (L, -2, "reason");
}

static void sta_authmode (lua_State *L, const system_event_t *evt)
{
  lua_pushinteger (L, evt->event_info.auth_change.old_mode);
  lua_setfield (L, -2, "old_mode");
  lua_pushinteger (L, evt->event_info.auth_change.new_mode);
  lua_setfield (L, -2, "new_mode");
}

static void sta_got_ip (lua_State *L, const system_event_t *evt)
{
  char ipstr[IP_STR_SZ] = { 0 };
  ip4str (ipstr, &evt->event_info.got_ip.ip_info.ip);
  lua_pushstring (L, ipstr);
  lua_setfield (L, -2, "ip");

  ip4str (ipstr, &evt->event_info.got_ip.ip_info.netmask);
  lua_pushstring (L, ipstr);
  lua_setfield (L, -2, "netmask");

  ip4str (ipstr, &evt->event_info.got_ip.ip_info.gw);
  lua_pushstring (L, ipstr);
  lua_setfield (L, -2, "gw");
}

static void ap_staconn (lua_State *L, const system_event_t *evt)
{
  char mac[MAC_STR_SZ];
  macstr (mac, evt->event_info.sta_connected.mac);
  lua_pushstring (L, mac);
  lua_setfield (L, -2, "mac");

  lua_pushinteger (L, evt->event_info.sta_connected.aid);
  lua_setfield (L, -2, "id");
}

static void ap_stadisconn (lua_State *L, const system_event_t *evt)
{
  char mac[MAC_STR_SZ];
  macstr (mac, evt->event_info.sta_disconnected.mac);
  lua_pushstring (L, mac);
  lua_setfield (L, -2, "mac");

  lua_pushinteger (L, evt->event_info.sta_disconnected.aid);
  lua_setfield (L, -2, "id");
}

static void ap_probe_req (lua_State *L, const system_event_t *evt)
{
  char str[MAC_STR_SZ];
  macstr (str, evt->event_info.ap_probereqrecved.mac);
  lua_pushstring (L, str);
  lua_setfield (L, -2, "from");

  lua_pushinteger (L, evt->event_info.ap_probereqrecved.rssi);
  lua_setfield (L, -2, "rssi");
}

static void on_event (const system_event_t *evt)
{
  int idx = event_idx_by_id (evt->event_id);
  if (idx < 0 || event_cb[idx] == LUA_NOREF)
    return;

  lua_State *L = lua_getstate ();
  lua_rawgeti (L, LUA_REGISTRYINDEX, event_cb[idx]);
  lua_pushstring (L, events[idx].name);
  lua_createtable (L, 0, 5);
  events[idx].fill_cb_arg (L, evt);
  lua_call (L, 2, 0);
}


static int wifi_eventmon (lua_State *L)
{
  const char *event_name = luaL_checkstring (L, 1);
  if (!lua_isnoneornil (L, 2))
    luaL_checkanyfunction (L, 2);
  lua_settop (L, 2);

  int idx = event_idx_by_name (event_name);
  if (idx < 0)
    return luaL_error (L, "unknown event: %s", event_name);

  luaL_unref (L, LUA_REGISTRYINDEX, event_cb[idx]);
  event_cb[idx] = lua_isnoneornil (L, 2) ?
    LUA_NOREF : luaL_ref (L, LUA_REGISTRYINDEX);

  return 0;
}


static int wifi_mode (lua_State *L)
{
  int mode = luaL_checkinteger (L, 1);
  bool save = luaL_optbool (L, 2, DEFAULT_SAVE);
  SET_SAVE_MODE(save);
  esp_err_t err;
  switch (mode)
  {
    case WIFI_MODE_NULL:
    case WIFI_MODE_STA:
    case WIFI_MODE_AP:
    case WIFI_MODE_APSTA:
      return ((err = esp_wifi_set_mode (mode)) == ESP_OK) ?
        0 : luaL_error (L, "failed to set mode, code %d", err);
    default:
      return luaL_error (L, "invalid wifi mode %d", mode);
  }
}


static int wifi_start (lua_State *L)
{
  esp_err_t err = esp_wifi_start ();
  return (err == ESP_OK) ?
    0 : luaL_error(L, "failed to start wifi, code %d", err);
}


static int wifi_stop (lua_State *L)
{
  esp_err_t err = esp_wifi_stop ();
  return (err == ESP_OK) ?
    0 : luaL_error (L, "failed to stop wifi, code %d", err);
}


static int wifi_init (lua_State *L)
{
  for (unsigned i = 0; i < ARRAY_LEN(event_cb); ++i)
    event_cb[i] = LUA_NOREF;

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  esp_err_t err = esp_wifi_init (&cfg);
  return (err == ESP_OK) ?
    0 : luaL_error (L, "failed to init wifi, code %d", err);
}


extern const LUA_REG_TYPE wifi_sta_map[];
extern const LUA_REG_TYPE wifi_ap_map[];

static const LUA_REG_TYPE wifi_map[] =
{
  { LSTRKEY( "getmode" ),     LFUNCVAL( wifi_getmode )        },
  { LSTRKEY( "on" ),          LFUNCVAL( wifi_eventmon )       },
  { LSTRKEY( "mode" ),        LFUNCVAL( wifi_mode )           },
  { LSTRKEY( "start" ),       LFUNCVAL( wifi_start )          },
  { LSTRKEY( "stop" ),        LFUNCVAL( wifi_stop )           },

  { LSTRKEY( "sta" ),         LROVAL( wifi_sta_map )          },
  { LSTRKEY( "ap" ),          LROVAL( wifi_ap_map )           },


  { LSTRKEY( "NULLMODE" ),    LNUMVAL( WIFI_MODE_NULL )       },
  { LSTRKEY( "STATION" ),     LNUMVAL( WIFI_MODE_STA )        },
  { LSTRKEY( "SOFTAP" ),      LNUMVAL( WIFI_MODE_AP )         },
  { LSTRKEY( "STATIONAP" ),   LNUMVAL( WIFI_MODE_APSTA )      },

  { LNILKEY, LNILVAL }
};

NODEMCU_MODULE(WIFI, "wifi", wifi_map, wifi_init);
