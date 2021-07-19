/*
 * Copyright (c) 2016 Johny Mattsson
 * All rights reserved.
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
 */

#include "wifi_common.h"
#include "ip_fmt.h"
#include "lauxlib.h"
#include <string.h>

// Important: match order of wifi_second_chan_t enum
const char *const wifi_second_chan_names[] =
{
  STR_WIFI_SECOND_CHAN_NONE,
  STR_WIFI_SECOND_CHAN_ABOVE,
  STR_WIFI_SECOND_CHAN_BELOW,
};

int wifi_event_idx_by_name (const event_desc_t *table, unsigned n, const char *name)
{
  for (unsigned i = 0; i < n; ++i)
    if (strcmp (table[i].name, name) == 0)
      return i;
  return -1;
}

int wifi_event_idx_by_id (const event_desc_t *table, unsigned n, esp_event_base_t base, int32_t id)
{
  for (unsigned i = 0; i < n; ++i)
    if (*table[i].event_base_ptr == base && table[i].event_id == id)
      return i;
  return -1;
}

int wifi_on (lua_State *L, const event_desc_t *table, unsigned n, int *event_cb)
{
  const char *event_name = luaL_checkstring (L, 1);
  if (!lua_isnoneornil (L, 2))
    luaL_checkanyfunction (L, 2);
  lua_settop (L, 2);

  int idx = wifi_event_idx_by_name (table, n, event_name);
  if (idx < 0)
    return luaL_error (L, "unknown event: %s", event_name);

  luaL_unref (L, LUA_REGISTRYINDEX, event_cb[idx]);
  event_cb[idx] = lua_isnoneornil (L, 2) ?
    LUA_NOREF : luaL_ref (L, LUA_REGISTRYINDEX);

  return 0;
}

int wifi_getmac (wifi_interface_t interface, lua_State *L)
{
  uint8_t mac[6];
  esp_err_t err = esp_wifi_get_mac(interface, mac);
  if (err != ESP_OK)
    return luaL_error (L, "failed to get mac, code %d", err);

  char mac_str[MAC_STR_SZ];
  macstr (mac_str, mac);
  lua_pushstring (L, mac_str);

  return 1;
}

