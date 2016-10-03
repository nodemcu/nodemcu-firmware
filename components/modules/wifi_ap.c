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
#include "wifi_common.h"
#include "esp_wifi.h"
#include <string.h>


#define DEFAULT_AP_CHANNEL 11
#define DEFAULT_AP_MAXCONNS 4
#define DEFAULT_AP_BEACON 100

static int wifi_ap_config (lua_State *L)
{
  luaL_checkanytable (L, 1);
  bool save = luaL_optbool (L, 2, DEFAULT_SAVE);

  wifi_config_t cfg;
  memset (&cfg, 0, sizeof (cfg));

  lua_getfield (L, 1, "ssid");
  size_t len;
  const char *str = luaL_checklstring (L, -1, &len);
  if (len > sizeof (cfg.ap.ssid))
    len = sizeof (cfg.ap.ssid);
  strncpy (cfg.ap.ssid, str, len);
  cfg.ap.ssid_len = len;

  lua_getfield (L, 1, "pwd");
  str = luaL_optlstring (L, -1, "", &len);
  if (len > sizeof (cfg.ap.password))
    len = sizeof (cfg.ap.password);
  strncpy (cfg.ap.password, str, len);

  lua_getfield (L, 1, "auth");
  int authmode = luaL_optint (L, -1, WIFI_AUTH_WPA2_PSK);
  if (authmode < 0 || authmode >= WIFI_AUTH_MAX)
    return luaL_error (L, "unknown auth mode %d", authmode);
  cfg.ap.authmode = authmode;

  lua_getfield (L, 1, "channel");
  cfg.ap.channel = luaL_optint (L, -1, DEFAULT_AP_CHANNEL);

  lua_getfield (L, 1, "hidden");
  cfg.ap.ssid_hidden = luaL_optbool (L, -1, false);

  lua_getfield (L, 1, "max");
  cfg.ap.max_connection = luaL_optint (L, -1, DEFAULT_AP_MAXCONNS);

  lua_getfield (L, 1, "beacon");
  cfg.ap.beacon_interval = luaL_optint (L, -1, DEFAULT_AP_BEACON);
  
  SET_SAVE_MODE(save);
  esp_err_t err = esp_wifi_set_config (WIFI_IF_AP, &cfg);
  return (err == ESP_OK) ?
    0 : luaL_error (L, "failed to set wifi config, code %d", err);
}


const LUA_REG_TYPE wifi_ap_map[] =
{
  { LSTRKEY( "config" ),              LFUNCVAL( wifi_ap_config )        },

  { LSTRKEY( "AUTH_OPEN" ),           LNUMVAL( WIFI_AUTH_OPEN )         },
  { LSTRKEY( "AUTH_WEP" ),            LNUMVAL( WIFI_AUTH_WEP )          },
  { LSTRKEY( "AUTH_WPA_PSK" ),        LNUMVAL( WIFI_AUTH_WPA_PSK )      },
  { LSTRKEY( "AUTH_WPA2_PSK" ),       LNUMVAL( WIFI_AUTH_WPA2_PSK )     },
  { LSTRKEY( "AUTH_WPA_WPA2_PSK" ),   LNUMVAL( WIFI_AUTH_WPA_WPA2_PSK ) },

  { LNILKEY, LNILVAL }
};
