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
#include "task/task.h"

static int wifi_getmode (lua_State *L)
{
  wifi_mode_t mode;
  esp_err_t err = esp_wifi_get_mode (&mode);
  if (err != ESP_OK)
    return luaL_error (L, "failed to get mode, code %d", err);
  lua_pushinteger (L, mode);
  return 1;
}

static int wifi_getchannel (lua_State *L)
{
  uint8_t primary;
  wifi_second_chan_t secondary;
  esp_err_t err = esp_wifi_get_channel (&primary, &secondary);
  if (err != ESP_OK)
    return luaL_error (L, "failed to get channel, code %d", err);
  lua_pushinteger (L, primary);
  lua_pushinteger (L, secondary);
  return 2;
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

#if defined(CONFIG_ESP_CONSOLE_USB_CDC)
// For some unknown reason, on an S2 with USB CDC console enabled, if we allow
// the esp_wifi_init() to run during initial startup, something Bad(tm)
// happens and the S2 fails to enumerate on the USB bus. However, if we defer
// the wifi initialisation, it starts up fine. This is an ugly workaround, but
// I'm out of ideas at this point. If I use a UART console, I see no errors
// even with the immediate init.
static task_handle_t th;
#endif

static void do_esp_wifi_init(task_param_t p, task_prio_t)
{
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init (&cfg));
}

extern void wifi_ap_init (void);
extern void wifi_sta_init (void);
static int wifi_init (lua_State *L)
{
  wifi_ap_init ();
  wifi_sta_init ();

#if defined(CONFIG_ESP_CONSOLE_USB_CDC)
  th = task_get_id(do_esp_wifi_init);
  task_post_low(th, 0);
#else
  do_esp_wifi_init(0, 0);
#endif
  return 0;
}


extern LROT_TABLE(wifi_sta);
extern LROT_TABLE(wifi_ap);

LROT_BEGIN(wifi, NULL, 0)
  LROT_FUNCENTRY( getchannel,                 wifi_getchannel )
  LROT_FUNCENTRY( getmode,                    wifi_getmode )
  LROT_FUNCENTRY( mode,                       wifi_mode )
  LROT_FUNCENTRY( start,                      wifi_start )
  LROT_FUNCENTRY( stop,                       wifi_stop )

  LROT_TABENTRY ( sta,                        wifi_sta )
  LROT_TABENTRY ( ap,                         wifi_ap )


  LROT_NUMENTRY ( NULLMODE,                   WIFI_MODE_NULL )
  LROT_NUMENTRY ( STATION,                    WIFI_MODE_STA )
  LROT_NUMENTRY ( SOFTAP,                     WIFI_MODE_AP )
  LROT_NUMENTRY ( STATIONAP,                  WIFI_MODE_APSTA )

  LROT_NUMENTRY ( AUTH_OPEN,                  WIFI_AUTH_OPEN )
  LROT_NUMENTRY ( AUTH_WEP,                   WIFI_AUTH_WEP )
  LROT_NUMENTRY ( AUTH_WPA_PSK,               WIFI_AUTH_WPA_PSK )
  LROT_NUMENTRY ( AUTH_WPA2_PSK,              WIFI_AUTH_WPA2_PSK )
  LROT_NUMENTRY ( AUTH_WPA_WPA2_PSK,          WIFI_AUTH_WPA_WPA2_PSK )
  LROT_NUMENTRY ( AUTH_WPA2_ENTERPRISE,       WIFI_AUTH_WPA2_ENTERPRISE )
  LROT_NUMENTRY ( AUTH_WPA3_PSK,              WIFI_AUTH_WPA3_PSK )
  LROT_NUMENTRY ( AUTH_WPA2_WPA3_PSK,         WIFI_AUTH_WPA2_WPA3_PSK )
  LROT_NUMENTRY ( AUTH_WAPI_PSK,              WIFI_AUTH_WAPI_PSK )

  LROT_NUMENTRY ( SAE_PWE_UNSPECIFIED,        WPA3_SAE_PWE_UNSPECIFIED )
  LROT_NUMENTRY ( SAE_PWE_HUNT_AND_PECK,      WPA3_SAE_PWE_HUNT_AND_PECK )
  LROT_NUMENTRY ( SAE_PWE_HASH_TO_ELEMENT,    WPA3_SAE_PWE_HASH_TO_ELEMENT )
  LROT_NUMENTRY ( SAE_PWE_BOTH,               WPA3_SAE_PWE_BOTH )

  LROT_NUMENTRY ( STR_WIFI_SECOND_CHAN_NONE,  WIFI_SECOND_CHAN_NONE )
  LROT_NUMENTRY ( STR_WIFI_SECOND_CHAN_ABOVE, WIFI_SECOND_CHAN_ABOVE )
  LROT_NUMENTRY ( STR_WIFI_SECOND_CHAN_BELOW, WIFI_SECOND_CHAN_BELOW )
LROT_END(wifi, NULL, 0)

NODEMCU_MODULE(WIFI, "wifi", wifi, wifi_init);
