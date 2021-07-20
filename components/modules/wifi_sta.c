/*
 * Copyright 2016-2021 Dius Computing Pty Ltd. All rights reserved.
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
#include "lmem.h"
#include "nodemcu_esp_event.h"
#include "wifi_common.h"
#include "ip_fmt.h"
#include "nodemcu_esp_event.h"
#include <string.h>
#include "esp_netif.h"

static esp_netif_t *wifi_sta = NULL;
static int scan_cb_ref = LUA_NOREF;

// --- Event handling -----------------------------------------------------

static void sta_conn (lua_State *L, const void *data);
static void sta_disconn (lua_State *L, const void *data);
static void sta_authmode (lua_State *L, const void *data);
static void sta_got_ip (lua_State *L, const void *data);
static void empty_arg (lua_State *L, const void *data) {}

static const event_desc_t events[] =
{
  { "start",            &WIFI_EVENT, WIFI_EVENT_STA_START,           empty_arg},
  { "stop",             &WIFI_EVENT, WIFI_EVENT_STA_STOP,            empty_arg},
  { "connected",        &WIFI_EVENT, WIFI_EVENT_STA_CONNECTED,       sta_conn },
  { "disconnected",     &WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED,    sta_disconn   },
  { "authmode_changed", &WIFI_EVENT, WIFI_EVENT_STA_AUTHMODE_CHANGE, sta_authmode  },
  { "got_ip",           &IP_EVENT,   IP_EVENT_STA_GOT_IP,            sta_got_ip},
};

#define ARRAY_LEN(a) (sizeof(a) / sizeof(a[0]))
static int event_cb[ARRAY_LEN(events)];

static void sta_conn (lua_State *L, const void *data)
{
  const wifi_event_sta_connected_t *connected =
    (const wifi_event_sta_connected_t *)data;
  lua_pushlstring (L, (const char *)connected->ssid, connected->ssid_len);
  lua_setfield (L, -2, "ssid");

  char bssid_str[MAC_STR_SZ];
  macstr (bssid_str, connected->bssid);
  lua_pushstring (L, bssid_str);
  lua_setfield (L, -2, "bssid");

  lua_pushinteger (L, connected->channel);
  lua_setfield (L, -2, "channel");

  lua_pushinteger (L, connected->authmode);
  lua_setfield (L, -2, "auth");
}

static void sta_disconn (lua_State *L, const void *data)
{
  const wifi_event_sta_disconnected_t *disconnected =
    (const wifi_event_sta_disconnected_t *)data;
  lua_pushlstring (L, (const char *)disconnected->ssid, disconnected->ssid_len);
  lua_setfield (L, -2, "ssid");

  char bssid_str[MAC_STR_SZ];
  macstr(bssid_str, disconnected->bssid);
  lua_pushstring (L, bssid_str);
  lua_setfield (L, -2, "bssid");

  lua_pushinteger (L, disconnected->reason);
  lua_setfield (L, -2, "reason");
}

static void sta_authmode (lua_State *L, const void *data)
{
  const wifi_event_sta_authmode_change_t *auth_change =
    (const wifi_event_sta_authmode_change_t *)data;
  lua_pushinteger (L, auth_change->old_mode);
  lua_setfield (L, -2, "old_mode");
  lua_pushinteger (L, auth_change->new_mode);
  lua_setfield (L, -2, "new_mode");
}

static void sta_got_ip (lua_State *L, const void *data)
{
  const esp_netif_ip_info_t *ip_info =
    (const esp_netif_ip_info_t *)data;

  char ipstr[IP_STR_SZ] = { 0 };
  ip4str_esp (ipstr, &ip_info->ip);
  lua_pushstring (L, ipstr);
  lua_setfield (L, -2, "ip");

  ip4str_esp (ipstr, &ip_info->netmask);
  lua_pushstring (L, ipstr);
  lua_setfield (L, -2, "netmask");

  ip4str_esp (ipstr, &ip_info->gw);
  lua_pushstring (L, ipstr);
  lua_setfield (L, -2, "gw");
}

static void on_event (esp_event_base_t base, int32_t id, const void *data)
{
  int idx = wifi_event_idx_by_id (events, ARRAY_LEN(events), base, id);
  if (idx < 0 || event_cb[idx] == LUA_NOREF)
    return;

  lua_State *L = lua_getstate ();
  lua_rawgeti (L, LUA_REGISTRYINDEX, event_cb[idx]);
  lua_pushstring (L, events[idx].name);
  lua_createtable (L, 0, 5);
  events[idx].fill_cb_arg (L, data);
  lua_call (L, 2, 0);
}

NODEMCU_ESP_EVENT(WIFI_EVENT, WIFI_EVENT_STA_START,           on_event);
NODEMCU_ESP_EVENT(WIFI_EVENT, WIFI_EVENT_STA_STOP,            on_event);
NODEMCU_ESP_EVENT(WIFI_EVENT, WIFI_EVENT_STA_CONNECTED,       on_event);
NODEMCU_ESP_EVENT(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED,    on_event);
NODEMCU_ESP_EVENT(WIFI_EVENT, WIFI_EVENT_STA_AUTHMODE_CHANGE, on_event);
NODEMCU_ESP_EVENT(IP_EVENT,   IP_EVENT_STA_GOT_IP,            on_event);
// TODO: support WPS events?

void wifi_sta_init (void)
{
  wifi_sta = esp_netif_create_default_wifi_sta();

  for (unsigned i = 0; i < ARRAY_LEN(event_cb); ++i)
    event_cb[i] = LUA_NOREF;
}


// --- Helper functions -----------------------------------------------------


static void do_connect (esp_event_base_t base, int32_t id, const void *data)
{
  (void)base; (void)id; (void)data;
  esp_wifi_connect ();
}


// --- Lua API functions ----------------------------------------------------
static int wifi_sta_setip(lua_State *L)
{
  luaL_checkanytable (L, 1);

  size_t len = 0;
  const char *str = NULL;
  esp_netif_ip_info_t ip_info = { 0, };

  lua_getfield (L, 1, "ip");
  str = luaL_checklstring (L, -1, &len);
  if (esp_netif_str_to_ip4(str, &ip_info.ip) != ESP_OK)
  {
    return luaL_error(L, "Could not parse IP address, aborting");
  }

  lua_getfield (L, 1, "netmask");
  str = luaL_checklstring (L, -1, &len);
  if (esp_netif_str_to_ip4(str, &ip_info.netmask) != ESP_OK)
  {
    return luaL_error(L, "Could not parse Netmask, aborting");
  }

  lua_getfield (L, 1, "gateway");
  str = luaL_checklstring (L, -1, &len);
  if (esp_netif_str_to_ip4(str, &ip_info.gw) != ESP_OK)
  {
    return luaL_error(L, "Could not parse Gateway address, aborting");
  }

  esp_netif_dns_info_t dns_info = { .ip = { .type = ESP_IPADDR_TYPE_V4 } };
  lua_getfield (L, 1, "dns");
  str = luaL_optlstring(L, -1, str, &len); // default to gateway
  if (esp_netif_str_to_ip4(str, &dns_info.ip.u_addr.ip4) != ESP_OK)
  {
    return luaL_error(L, "Could not parse DNS address, aborting");
  }

  ESP_ERROR_CHECK(esp_netif_dhcpc_stop(wifi_sta));

  esp_netif_set_ip_info(wifi_sta, &ip_info);
  esp_netif_set_dns_info(wifi_sta, ESP_NETIF_DNS_MAIN, &dns_info);

  return 0;
}

static int wifi_sta_sethostname(lua_State *L)
{
  size_t l;
  const char *hostname = luaL_checklstring(L, 1, &l);

  esp_err_t err = esp_netif_set_hostname(wifi_sta, hostname);

  if (err != ESP_OK)
    return luaL_error (L, "failed to set hostname, code %d", err);

  lua_pushboolean (L, err==ESP_OK);

  return 1;
}

static int wifi_sta_config (lua_State *L)
{
  luaL_checkanytable (L, 1);
  bool save = luaL_optbool (L, 2, DEFAULT_SAVE);
  lua_settop (L, 1);

  wifi_config_t cfg;
  memset (&cfg, 0, sizeof (cfg));

  lua_getfield (L, 1, "ssid");
  size_t len;
  const char *str = luaL_checklstring (L, -1, &len);
  if (len > sizeof (cfg.sta.ssid))
    len = sizeof (cfg.sta.ssid);
  strncpy ((char *)cfg.sta.ssid, str, len);

  lua_getfield (L, 1, "pwd");
  str = luaL_optlstring (L, -1, "", &len);
  if (len > sizeof (cfg.sta.password))
    len = sizeof (cfg.sta.password);
  strncpy ((char *)cfg.sta.password, str, len);

  lua_getfield (L, 1, "bssid");
  cfg.sta.bssid_set = false;
  if (lua_isstring (L, -1))
  {
    const char *bssid = luaL_checklstring (L, -1, &len);
    const char *fmts[] = {
      "%hhx%hhx%hhx%hhx%hhx%hhx",
      "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
      "%hhx-%hhx-%hhx-%hhx-%hhx-%hhx",
      "%hhx %hhx %hhx %hhx %hhx %hhx",
      NULL
    };
    for (unsigned i = 0; fmts[i]; ++i)
    {
      if (sscanf (bssid, fmts[i],
        &cfg.sta.bssid[0], &cfg.sta.bssid[1], &cfg.sta.bssid[2],
        &cfg.sta.bssid[3], &cfg.sta.bssid[4], &cfg.sta.bssid[5]) == 6)
      {
        cfg.sta.bssid_set = true;
        break;
      }
    }
    if (!cfg.sta.bssid_set)
      return luaL_error (L, "invalid BSSID: %s", bssid);
  }

  SET_SAVE_MODE(save);
  esp_err_t err = esp_wifi_set_config (WIFI_IF_STA, &cfg);
  if (err != ESP_OK)
    return luaL_error (L, "failed to set wifi config, code %d", err);

  return 0;
}


static int wifi_sta_connect (lua_State *L)
{
  esp_err_t err = esp_wifi_connect ();
  return (err == ESP_OK) ? 0 : luaL_error (L, "connect failed, code %d", err);
}


static int wifi_sta_disconnect (lua_State *L)
{
  esp_err_t err = esp_wifi_disconnect ();
  return (err == ESP_OK) ? 0 : luaL_error(L, "disconnect failed, code %d", err);
}


static int wifi_sta_getconfig (lua_State *L)
{
  wifi_config_t cfg;
  esp_err_t err = esp_wifi_get_config (WIFI_IF_STA, &cfg);
  if (err != ESP_OK)
    return luaL_error (L, "failed to get config, code %d", err);

  lua_createtable (L, 0, 3);
  size_t ssid_len = strnlen ((char *)cfg.sta.ssid, sizeof (cfg.sta.ssid));
  lua_pushlstring (L, (char *)cfg.sta.ssid, ssid_len);
  lua_setfield (L, -2, "ssid");

  size_t pwd_len = strnlen ((char *)cfg.sta.password, sizeof (cfg.sta.password));
  lua_pushlstring (L, (char *)cfg.sta.password, pwd_len);
  lua_setfield (L, -2, "pwd");

  if (cfg.sta.bssid_set)
  {
    char bssid_str[MAC_STR_SZ];
    macstr (bssid_str, cfg.sta.bssid);
    lua_pushstring (L, bssid_str);
    lua_setfield (L, -2, "bssid");
  }

  return 1;
}

static int wifi_sta_getmac (lua_State *L)
{
  return wifi_getmac(WIFI_IF_STA, L);
}

static void on_scan_done(esp_event_base_t base, int32_t id, const void *data)
{
  (void)data;

  lua_State *L = lua_getstate ();
  lua_rawgeti (L, LUA_REGISTRYINDEX, scan_cb_ref);
  luaL_unref (L, LUA_REGISTRYINDEX, scan_cb_ref);
  scan_cb_ref = LUA_NOREF;
  int nargs = 1;
  if (!lua_isnoneornil (L, -1))
  {
    uint16_t num_ap = 0;
    esp_err_t err = esp_wifi_scan_get_ap_num (&num_ap);
    wifi_ap_record_t *aps = luaM_malloc (L, num_ap * sizeof (wifi_ap_record_t));
    if ((err == ESP_OK) && (aps) &&
        (err = esp_wifi_scan_get_ap_records (&num_ap, aps)) == ESP_OK)
    {
      lua_pushnil (L); // no error

      lua_createtable (L, num_ap, 0); // prepare array
      ++nargs;
      for (unsigned i = 0; i < num_ap; ++i)
      {
        lua_createtable (L, 0, 6); // prepare table for AP entry

        char bssid_str[MAC_STR_SZ];
        macstr (bssid_str, aps[i].bssid);
        lua_pushstring (L, bssid_str);
        lua_setfield (L, -2, "bssid");

        size_t ssid_len =
          strnlen ((const char *)aps[i].ssid, sizeof (aps[i].ssid));
        lua_pushlstring (L, (const char *)aps[i].ssid, ssid_len);
        lua_setfield (L, -2, "ssid");

        lua_pushinteger (L, aps[i].primary);
        lua_setfield (L, -2, "channel");

        lua_pushinteger (L, aps[i].rssi);
        lua_setfield (L, -2, "rssi");

        lua_pushinteger (L, aps[i].authmode);
        lua_setfield (L, -2, "auth");

        lua_pushstring (L, wifi_second_chan_names[aps[i].second]);
        lua_setfield (L, -2, "bandwidth");

        lua_rawseti (L, -2, i + 1); // add table to array
      }
    }
    else
      lua_pushfstring (L, "failure on scan done");
    luaM_free (L, aps);
    lua_call (L, nargs, 0);
  }
}


static int wifi_sta_on (lua_State *L)
{
  return wifi_on (L, events, ARRAY_LEN(events), event_cb);
}

static int wifi_sta_scan (lua_State *L)
{
  if (scan_cb_ref != LUA_NOREF)
    return luaL_error (L, "scan already in progress");

  luaL_checkanytable (L, 1);

  luaL_checkanyfunction (L, 2);
  lua_settop (L, 2);
  scan_cb_ref = luaL_ref (L, LUA_REGISTRYINDEX);

  wifi_scan_config_t scan_cfg;
  memset (&scan_cfg, 0, sizeof (scan_cfg));

  lua_getfield (L, 1, "ssid");
  scan_cfg.ssid = (uint8_t *)luaL_optstring (L, -1, NULL);

  lua_getfield (L, 1, "bssid");
  scan_cfg.bssid = (uint8_t *)luaL_optstring (L, -1, NULL);

  lua_getfield (L, 1, "channel");
  scan_cfg.channel = luaL_optint (L, -1, 0);

  lua_getfield (L, 1, "hidden");
  scan_cfg.show_hidden = luaL_optint (L, -1, 0);

  esp_err_t err = esp_wifi_scan_start (&scan_cfg, false);
  if (err != ESP_OK)
  {
    luaL_unref (L, LUA_REGISTRYINDEX, scan_cb_ref);
    scan_cb_ref = LUA_NOREF;
    return luaL_error (L, "failed to start scan, code %d", err);
  }
  else
    return 0;
}


LROT_PUBLIC_BEGIN(wifi_sta)
  LROT_FUNCENTRY( setip,       wifi_sta_setip )
  LROT_FUNCENTRY( sethostname, wifi_sta_sethostname )
  LROT_FUNCENTRY( config,      wifi_sta_config )
  LROT_FUNCENTRY( connect,     wifi_sta_connect )
  LROT_FUNCENTRY( disconnect,  wifi_sta_disconnect )
  LROT_FUNCENTRY( getconfig,   wifi_sta_getconfig )
  LROT_FUNCENTRY( getmac,      wifi_sta_getmac )
  LROT_FUNCENTRY( on,          wifi_sta_on )
  LROT_FUNCENTRY( scan,        wifi_sta_scan )
LROT_END(wifi_sta, NULL, 0)


// Currently no auto-connect, so do that in response to events
NODEMCU_ESP_EVENT(WIFI_EVENT, WIFI_EVENT_STA_START, do_connect);
NODEMCU_ESP_EVENT(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, do_connect);
NODEMCU_ESP_EVENT(WIFI_EVENT, WIFI_EVENT_SCAN_DONE, on_scan_done);
