/*
 * Copyright 2016 DiUS Computing Pty Ltd. All rights reserved.
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

#include "sdkconfig.h"
#ifdef CONFIG_NODEMCU_CMODULE_BTHCI

#include "module.h"
#include "lauxlib.h"
#include "task/task.h"
#include "platform.h"
#include "esp_bt.h"
#include <stdlib.h>
#include <string.h>

#include <esp_log.h>
#define TAG "bthci"

#define BD_ADDR_LEN 6
typedef uint8_t bd_addr_t[BD_ADDR_LEN];

#define st(x) do { x } while (0)

#define STREAM_U8(p, v)  st( *(p)++ =  (uint8_t)(v); )
#define STREAM_U16(p, v) \
  st( \
    *(p)++ = (uint8_t)(v); \
    *(p)++ = (uint8_t)((v) >> 8); \
  )
#define STREAM_BD_ADDR(p, addr) \
  st( \
    for (int i = 0; i < BD_ADDR_LEN; ++i) \
      *(p)++ = (uint8_t)addr[BD_ADDR_LEN - 1 - i];\
  )
#define STREAM_ARRAY(p, arr, len) \
  st( \
    for (int i = 0; i < len; ++i) \
      *(p)++ = (uint8_t)arr[i]; \
  )


enum {
  H4_TYPE_COMMAND = 1,
  H4_TYPE_ACL     = 2,
  H4_TYPE_SCO     = 3,
  H4_TYPE_EVENT   = 4
};

#define EVENT_COMMAND_COMPLETE    0x0e
#define EVENT_LE_META             0x3e

// Subevent codes for LE-Meta
#define EVENT_LE_META_ADV_REPORT  0x02

#define ERROR_UNSPECIFIED 0x1f

#define HCI_OGF(x)  (x << 10)

#define HCI_GRP_HOST_CONT_BASEBAND_CMDS   HCI_OGF(0x03)
#define HCI_GRP_BLE_CMDS                  HCI_OGF(0x08)

#define HCI_RESET                   (HCI_GRP_HOST_CONT_BASEBAND_CMDS | 0x0003)
#define HCI_SET_EVENT_MASK          (HCI_GRP_HOST_CONT_BASEBAND_CMDS | 0x0001)
#define HCI_BLE_ADV_WRI_PARAMS      (HCI_GRP_BLE_CMDS | 0x0006)
#define HCI_BLE_ADV_WRI_DATA        (HCI_GRP_BLE_CMDS | 0x0008)
#define HCI_BLE_ADV_WRI_ENABLE      (HCI_GRP_BLE_CMDS | 0x000a)
#define HCI_BLE_SET_SCAN_PARAMS     (HCI_GRP_BLE_CMDS | 0x000b)
#define HCI_BLE_SET_SCAN_ENABLE     (HCI_GRP_BLE_CMDS | 0x000c)

#define SZ_HDR                      4

#define SZ_HCI_RESET                4
#define SZ_HCI_BLE_ADV_WRI_PARAMS   19
#define SZ_HCI_BLE_ADV_WRI_DATA     36
#define SZ_HCI_BLE_ADV_WRI_ENABLE   5
#define SZ_HCI_BLE_SET_SCAN_ENABLE  6
#define SZ_HCI_BLE_SET_SCAN_PARAMS  11

#define ADV_MIN_INTERVAL 0x00a0
#define ADV_MAX_DATA     31

enum {
  ADV_IND                = 0x00,
  ADV_DIRECT_IND_HI_DUTY = 0x01,
  ADV_SCAN_IND           = 0x02,
  ADV_NONCONN_IND        = 0x03,
  ADV_DIRECT_IND_LO_DUTY = 0x04
};
enum {
  ADV_OWN_ADDR_PUB          = 0x00,
  ADV_OWN_ADDR_RAND         = 0x01,
  ADV_OWN_ADDR_PRIV_OR_PUB  = 0x02,
  ADV_OWN_ADDR_PRIV_OR_RAND = 0x03
};
enum {
  ADV_PEER_ADDR_PUB  = 0x00,
  ADV_PEER_ADDR_RAND = 0x01
};
#define ADV_CHAN_37   0x01
#define ADV_CHAN_38   0x02
#define ADV_CHAN_39   0x03
#define ADV_CHAN_ALL  (ADV_CHAN_37 | ADV_CHAN_38 | ADV_CHAN_39)

#define ADV_FILTER_NONE 0x00
#define ADV_FILTER_SCAN_WHITELIST 0x01
#define ADV_FILTER_CONN_WHITELIST 0x02
#define ADV_FILTER_SCAN_CONN_WHITELIST \
  (ADV_FILTER_SCAN_WHITELIST | ADV_FILTER_CONN_WHITELIST)


#define get_opt_field_int(idx, var, name) \
  st( \
    lua_getfield (L, idx, name); \
    if (!lua_isnil (L, -1)) \
      var = lua_tointeger (L, -1); \
  )


#define MAX_CMD_Q 5


// --- Local state --------------------------------------------

static struct cmd_list
{
  uint16_t cmd;
  int      cb_ref;
} __attribute__((packed)) cmd_q[MAX_CMD_Q];

static task_handle_t hci_event_task_handle;
static int adv_rep_cb_ref = LUA_NOREF;


// --- VHCI callbacks ------------------------------------------

static void on_bthci_can_send (void)
{
  // Unused, we don't support queuing up commands on this level
}


static int on_bthci_receive (uint8_t *data, uint16_t len)
{
#if 0
  printf ("BT:");
  for (int i = 0; i < len; ++i)
    printf (" %02x", data[i]);
  printf ("\n");
#endif

  if (data[0] == H4_TYPE_EVENT)
  {
    unsigned len = data[2];
    char *copy = malloc (SZ_HDR + len);
    if (copy)
      memcpy (copy, data, SZ_HDR + len);
    if (!copy || !task_post_high (hci_event_task_handle, (task_param_t)copy))
    {
      NODE_ERR("Dropped BT event due to no mem!");
      free (copy);
      return 0;
    }
  }

  return 0;
}

static const esp_vhci_host_callback_t bthci_callbacks =
{
  on_bthci_can_send,
  on_bthci_receive
};


// --- Helper functions ---------------------------------

// Expects callback function at top of stack
static int send_hci_command (lua_State *L, uint8_t *data, unsigned len)
{
  if (esp_vhci_host_check_send_available ())
  {
    uint16_t cmd = (((uint16_t)data[2]) << 8) | data[1];
    for (int i = 0; i < MAX_CMD_Q; ++i)
    {
      if (cmd_q[i].cb_ref == LUA_NOREF)
      {
        if (lua_gettop (L) > 0 && !lua_isnil (L, -1))
        {
          cmd_q[i].cmd = cmd;
          luaL_checkfunction (L, -1);
          lua_pushvalue (L, -1);
          cmd_q[i].cb_ref = luaL_ref (L, LUA_REGISTRYINDEX);
        }
        esp_vhci_host_send_packet (data, len);
        return 0;
      }
    }
  }
  // Nope, couldn't send this command!
  lua_pushinteger (L, ERROR_UNSPECIFIED);
  lua_pushlstring (L, NULL, 0);
  lua_call (L, 2, 0);
  return 0;
}


static void enable_le_meta_events (void)
{
  uint8_t buf[SZ_HDR + 8];
  uint8_t *p = buf;

  STREAM_U8 (p, H4_TYPE_COMMAND);
  STREAM_U16(p, HCI_SET_EVENT_MASK);
  STREAM_U8 (p, sizeof (buf) - SZ_HDR);

  STREAM_U8 (p, 0xff);
  STREAM_U8 (p, 0xff);
  STREAM_U8 (p, 0xff);
  STREAM_U8 (p, 0xff);
  STREAM_U8 (p, 0xff);
  STREAM_U8 (p, 0x1f);
  STREAM_U8 (p, 0x00);
  STREAM_U8 (p, 0x20); // LE Meta-Event

  esp_vhci_host_send_packet (buf, sizeof (buf));
}


// --- Lua context handlers ------------------------------------

static void invoke_cmd_q_callback (
  lua_State *L, unsigned idx, uint8_t code, unsigned len, const uint8_t *data)
{
  if (cmd_q[idx].cb_ref != LUA_NOREF)
  {
    lua_rawgeti (L, LUA_REGISTRYINDEX, cmd_q[idx].cb_ref);
    luaL_unref (L, LUA_REGISTRYINDEX, cmd_q[idx].cb_ref);
    cmd_q[idx].cb_ref = LUA_NOREF;

    if (code) // non-zero response code?
      lua_pushinteger (L, code);
    else
      lua_pushnil (L); // no error
    lua_pushlstring (L, (const char *)data, len ); // extra bytes, if any
    lua_call (L, 2, 0);
  }
}


static void handle_hci_event (task_param_t arg, task_prio_t prio)
{
  (void)prio;
  lua_State *L = lua_getstate ();
  uint8_t *hci_event = (uint8_t *)arg;

  unsigned type = hci_event[1];
  unsigned len = hci_event[2];
  if (type == EVENT_COMMAND_COMPLETE)
  {
    uint16_t cmd = (((uint16_t)hci_event[5]) << 8) | hci_event[4];
    for (int i = 0; i < MAX_CMD_Q; ++i)
    {
      if (cmd_q[i].cb_ref != LUA_NOREF && cmd_q[i].cmd == cmd)
      {
        invoke_cmd_q_callback (L, i, hci_event[6], len - 4, &hci_event[7]);
        break;
      }
    }
    if (cmd == HCI_RESET) // clear cmd_q to prevent leaking slots
    {
      for (int i = 0; i < MAX_CMD_Q; ++i)
        invoke_cmd_q_callback (L, i, ERROR_UNSPECIFIED, 0, NULL);

      enable_le_meta_events (); // renenable le events
    }
  }
  else if (type == EVENT_LE_META)
  {
    unsigned subtype = hci_event[3];
    if (subtype == EVENT_LE_META_ADV_REPORT)
    {
      unsigned num_reps = hci_event[4];
      // The encoding of multiple reports is not clear in spec, and I've never
      // seen a multiple-report event, so for now we only handle single reports
      if (adv_rep_cb_ref != LUA_NOREF && num_reps == 1)
      {
        uint8_t *report = &hci_event[5];
        lua_rawgeti (L, LUA_REGISTRYINDEX, adv_rep_cb_ref);
        lua_pushlstring (L, (const char *)report, len - 2);
        lua_call (L, 1, 0);
      }
    }
  }

  free (hci_event);
}


// --- Lua functions ------------------------------------

static int lbthci_init (lua_State *L)
{
  hci_event_task_handle = task_get_id (handle_hci_event);
  for (int i = 0; i < MAX_CMD_Q; ++i)
    cmd_q[i].cb_ref = LUA_NOREF;

  esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();

  esp_err_t ret;
  if ((ret = esp_bt_controller_init(&bt_cfg)) != ESP_OK) {
    ESP_LOGE(TAG, "%s initialize controller failed: %s\n", __func__, esp_err_to_name(ret));
    return -1;
  }

#ifdef CONFIG_BTDM_CONTROLLER_MODE_BTDM
  esp_bt_mode_t mode = ESP_BT_MODE_BTDM;
#elif defined CONFIG_BTDM_CONTROLLER_MODE_BLE_ONLY
  esp_bt_mode_t mode = ESP_BT_MODE_BLE;
#else
  ESP_LOGE(TAG, "%s configuration mismatch. Select BLE Only or BTDM mode from menuconfig", __func__);
  return -1;
#endif

  if ((ret = esp_bt_controller_enable(mode)) != ESP_OK) {
    ESP_LOGE(TAG, "%s enable controller failed: %s\n", __func__, esp_err_to_name(ret));
    return -1;
  }

  esp_vhci_host_register_callback (&bthci_callbacks);

  enable_le_meta_events ();
  return 0;
}


static int lbthci_reset (lua_State *L)
{
  uint8_t buf[SZ_HCI_RESET];
  uint8_t *p = buf;
  STREAM_U8 (p, H4_TYPE_COMMAND);
  STREAM_U16(p, HCI_RESET);
  STREAM_U8 (p, 0);

  lua_settop (L, 1);
  return send_hci_command (L, buf, sizeof (buf));
}


static int lbthci_adv_enable (lua_State *L)
{
  bool onoff = lua_tointeger (L, 1) > 0;

  uint8_t buf[SZ_HCI_BLE_ADV_WRI_ENABLE];
  uint8_t *p = buf;

  STREAM_U8 (p, H4_TYPE_COMMAND);
  STREAM_U16(p, HCI_BLE_ADV_WRI_ENABLE);
  STREAM_U8 (p, 1);
  STREAM_U8 (p, onoff);

  lua_settop (L, 2);
  return send_hci_command (L, buf, sizeof (buf));
}


static int lbthci_adv_setdata (lua_State *L)
{
  size_t len;
  const char *data = luaL_checklstring (L, 1, &len);
  if (len > ADV_MAX_DATA)
    len = ADV_MAX_DATA;

  uint8_t buf[SZ_HCI_BLE_ADV_WRI_DATA];
  uint8_t *p = buf;
  STREAM_U8 (p, H4_TYPE_COMMAND);
  STREAM_U16(p, HCI_BLE_ADV_WRI_DATA);
  STREAM_U8 (p, sizeof (buf) - SZ_HDR);

  STREAM_U8 (p, len);
  STREAM_ARRAY(p, data, len);

  lua_settop (L, 2);
  return send_hci_command (L, buf, sizeof (buf));
}


static int lbthci_adv_setparams (lua_State *L)
{
  uint16_t adv_interval_min = 0x0400; // 0.64s
  uint16_t adv_interval_max = 0x0800; // 1.28s
  uint8_t  adv_type = ADV_IND;
  uint8_t  own_addr_type = ADV_OWN_ADDR_PUB;
  uint8_t  peer_addr_type = ADV_PEER_ADDR_PUB;
  bd_addr_t peer_addr;
  uint8_t   adv_chan_map = ADV_CHAN_ALL;
  uint8_t   adv_filter_pol = ADV_FILTER_NONE;

  luaL_checktable (L, 1);
  lua_settop (L, 2); // Pad a nil into the function slot if necessary

  get_opt_field_int (1, adv_interval_min, "interval_min");
  get_opt_field_int (1, adv_interval_max, "interval_max");
  get_opt_field_int (1, adv_type,         "type");
  get_opt_field_int (1, own_addr_type,    "own_addr_type");
  get_opt_field_int (1, peer_addr_type,   "peer_addr_type");
  // TODO: peer addr
  get_opt_field_int (1, adv_chan_map,     "channel_map");
  get_opt_field_int (1, adv_filter_pol,   "filter_policy");


  if (adv_type == ADV_SCAN_IND || adv_type == ADV_NONCONN_IND)
  {
    if (adv_interval_min < ADV_MIN_INTERVAL)
      adv_interval_min = ADV_MIN_INTERVAL;
    if (adv_interval_max < ADV_MIN_INTERVAL)
      adv_interval_max = ADV_MIN_INTERVAL;
  }
  else if (adv_type == ADV_DIRECT_IND_HI_DUTY ||
           adv_type == ADV_DIRECT_IND_LO_DUTY)
  {
    // TODO: enforce peer addr validity
  }

  if (adv_chan_map == 0 || adv_chan_map > ADV_CHAN_ALL)
    adv_chan_map = ADV_CHAN_ALL;

  if (adv_filter_pol > ADV_FILTER_SCAN_CONN_WHITELIST)
    adv_filter_pol = ADV_FILTER_SCAN_CONN_WHITELIST;

  uint8_t buf[SZ_HCI_BLE_ADV_WRI_PARAMS];
  uint8_t *p = buf;
  STREAM_U8 (p, H4_TYPE_COMMAND);
  STREAM_U16(p, HCI_BLE_ADV_WRI_PARAMS);
  STREAM_U8 (p, sizeof (buf) - SZ_HDR);

  STREAM_U16(p, adv_interval_min);
  STREAM_U16(p, adv_interval_max);
  STREAM_U8 (p, adv_type);
  STREAM_U8 (p, own_addr_type);
  STREAM_U8 (p, peer_addr_type);
  STREAM_BD_ADDR(p, peer_addr);
  STREAM_U8 (p, adv_chan_map);
  STREAM_U8 (p, adv_filter_pol);

  lua_settop (L, 2);
  return send_hci_command (L, buf, sizeof (buf));
}


static int lbthci_scan (lua_State *L)
{
  bool onoff = lua_tointeger (L, 1) > 0;

  uint8_t buf[SZ_HCI_BLE_SET_SCAN_ENABLE];
  uint8_t *p = buf;
  STREAM_U8 (p, H4_TYPE_COMMAND);
  STREAM_U16(p, HCI_BLE_SET_SCAN_ENABLE);
  STREAM_U8 (p, sizeof (buf) - SZ_HDR);
  STREAM_U8 (p, onoff ? 0x01 : 0x00);
  STREAM_U8 (p, 0x00); // no filter duplicates

  lua_settop (L, 2);
  return send_hci_command (L, buf, sizeof (buf));
}


static int lbthci_scan_setparams (lua_State *L)
{
  uint8_t  scan_mode = 0;
  uint16_t scan_interval = 0x0010;
  uint16_t scan_window   = 0x0010;
  uint8_t  own_addr_type = 0;
  uint8_t  filter_policy = 0;

  luaL_checktable (L, 1);
  lua_settop (L, 2); // Pad a nil into the function slot if necessary

  get_opt_field_int (1, scan_mode,     "mode");
  get_opt_field_int (1, scan_interval, "interval");
  get_opt_field_int (1, scan_window,   "window");
  get_opt_field_int (1, own_addr_type, "own_addr_type");
  get_opt_field_int (1, filter_policy, "filter_policy");

  // TODO clamp ranges

  uint8_t buf[SZ_HCI_BLE_SET_SCAN_PARAMS];
  uint8_t *p = buf;
  STREAM_U8 (p, H4_TYPE_COMMAND);
  STREAM_U16(p, HCI_BLE_SET_SCAN_PARAMS);
  STREAM_U8 (p, sizeof (buf) - SZ_HDR);

  STREAM_U8 (p, scan_mode);
  STREAM_U16(p, scan_interval);
  STREAM_U16(p, scan_window);
  STREAM_U8 (p, own_addr_type);
  STREAM_U8 (p, filter_policy);

  lua_settop (L, 2);
  return send_hci_command (L, buf, sizeof (buf));
}


static int lbthci_scan_on (lua_State *L)
{
  const char *on_what = luaL_checkstring (L, 1);
  if (strcmp (on_what, "adv_report") == 0)
  {
    lua_settop (L, 2);
    luaL_unref (L, LUA_REGISTRYINDEX, adv_rep_cb_ref);
    adv_rep_cb_ref = LUA_NOREF;
    if (!lua_isnil (L, 2))
      adv_rep_cb_ref = luaL_ref (L, LUA_REGISTRYINDEX);
    return 0;
  }
  else
    return luaL_error (L, "unknown event '%s'", on_what);
}

static int lbthci_rawhci (lua_State *L)
{
  size_t len;
  uint8_t *data = (uint8_t *)luaL_checklstring (L, 1, &len);
  if (len < SZ_HDR || data[0] != H4_TYPE_COMMAND)
    return luaL_error (L, "definitely not a valid HCI command");

  lua_settop (L, 2);
  return send_hci_command (L, data, len);
}


LROT_BEGIN(bthci_adv, NULL, 0)
  LROT_FUNCENTRY( enable,        lbthci_adv_enable )
  LROT_FUNCENTRY( setdata,       lbthci_adv_setdata )
  LROT_FUNCENTRY( setparams,     lbthci_adv_setparams )

  // Advertising types
  LROT_NUMENTRY ( CONN_UNDIR,    ADV_IND )
  LROT_NUMENTRY ( CONN_DIR_HI,   ADV_DIRECT_IND_HI_DUTY )
  LROT_NUMENTRY ( SCAN_UNDIR,    ADV_SCAN_IND )
  LROT_NUMENTRY ( NONCONN_UNDIR, ADV_NONCONN_IND )
  LROT_NUMENTRY ( CONN_DIR_LO,   ADV_DIRECT_IND_LO_DUTY )

  LROT_NUMENTRY ( CHAN_37,       ADV_CHAN_37 )
  LROT_NUMENTRY ( CHAN_38,       ADV_CHAN_38 )
  LROT_NUMENTRY ( CHAN_39,       ADV_CHAN_39 )
  LROT_NUMENTRY ( CHAN_ALL,      ADV_CHAN_ALL )
LROT_END(bthci_adv, NULL, 0)


LROT_BEGIN(bthci_scan, NULL, 0)
  LROT_FUNCENTRY( enable,    lbthci_scan )
  LROT_FUNCENTRY( setparams, lbthci_scan_setparams )
  LROT_FUNCENTRY( on,        lbthci_scan_on )
LROT_END(bthci_scan, NULL, 0)


LROT_BEGIN(bthci, NULL, 0)
  LROT_FUNCENTRY( rawhci, lbthci_rawhci )
  LROT_FUNCENTRY( reset,  lbthci_reset )
  LROT_TABENTRY ( adv,    bthci_adv )
  LROT_TABENTRY ( scan,   bthci_scan )
LROT_END(bthci, NULL, 0)

NODEMCU_MODULE(BTHCI, "bthci", bthci, lbthci_init);
#endif
