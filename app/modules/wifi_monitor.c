// Module for interfacing with WIFI

#include "module.h"
#include "lauxlib.h"
#include "lapi.h"
#include "platform.h"

#include "c_string.h"
#include "c_stdlib.h"
#include "ctype.h"

#include "c_types.h"
#include "user_interface.h"
#include "wifi_common.h"
#include "sys/network_80211.h"

static int recv_cb;
static uint8 mon_offset;
static uint8 mon_value;
static uint8 mon_mask;
static task_handle_t tasknumber;

#define SNIFFER_BUF2_BUF_SIZE       112

#define BITFIELD(byte, start, len)   8 * (byte) + (start), (len)
#define BYTEFIELD(byte, len)   8 * (byte), 8 * (len)

#define IS_SIGNED 1
#define IS_STRING 2
#define IS_HEXSTRING 3

#define IE_TABLE  4
#define SINGLE_IE  5
#define IS_HEADER  6

#define IE(id)  0, (id), SINGLE_IE

typedef struct {
  const char *name;
  unsigned int start : 12;
  unsigned int length : 12;
  uint8 opts;
} field_t;

// must be sorted alphabetically
static const field_t fields[] = {
  { "aggregation", BITFIELD(7, 3, 1), 0 },
  { "ampdu_cnt", BITFIELD(9, 0, 8), 0 },
  { "bssid", BYTEFIELD(28, 6), IS_STRING },
  { "bssid_hex", BYTEFIELD(28, 6), IS_HEXSTRING },
  { "bssidmatch0", BITFIELD(3, 6, 1), 0 },
  { "bssidmatch1", BITFIELD(3, 7, 1), 0 },
  { "channel", BITFIELD(10, 0, 4), 0 },
  { "cwb", BITFIELD(4, 7, 1), 0 },
  { "dmatch0", BITFIELD(3, 4, 1), 0 },
  { "dmatch1", BITFIELD(3, 5, 1), 0 },
  { "dstmac", BYTEFIELD(22, 6), IS_STRING },
  { "dstmac_hex", BYTEFIELD(22, 6), IS_HEXSTRING },
  { "duration", BYTEFIELD(14, 2), 0 },
  { "fec_coding", BITFIELD(7, 6, 1), 0 },
  { "frame", BYTEFIELD(12, 112), IS_STRING },
  { "frame_hex", BYTEFIELD(12, 112), IS_HEXSTRING },
  { "fromds", BITFIELD(13, 1, 1), 0 },
  { "header", BYTEFIELD(12, 0), IS_HEADER },
  { "ht_length", BITFIELD(5, 0, 16), 0 },
  { "ie_20_40_bss_coexistence", IE(72)},
  { "ie_20_40_bss_intolerant_channel_report", IE(73)},
  { "ie_advertisement_protocol", IE(108)},
  { "ie_aid", IE(197)},
  { "ie_antenna", IE(64)},
  { "ie_ap_channel_report", IE(51)},
  { "ie_authenticated_mesh_peering_exchange", IE(139)},
  { "ie_beacon_timing", IE(120)},
  { "ie_bss_ac_access_delay", IE(68)},
  { "ie_bss_available_admission_capacity", IE(67)},
  { "ie_bss_average_access_delay", IE(63)},
  { "ie_bss_load", IE(11)},
  { "ie_bss_max_idle_period", IE(90)},
  { "ie_cf_parameter_set", IE(4)},
  { "ie_challenge_text", IE(16)},
  { "ie_channel_switch_announcement", IE(37)},
  { "ie_channel_switch_timing", IE(104)},
  { "ie_channel_switch_wrapper", IE(196)},
  { "ie_channel_usage", IE(97)},
  { "ie_collocated_interference_report", IE(96)},
  { "ie_congestion_notification", IE(116)},
  { "ie_country", IE(7)},
  { "ie_destination_uri", IE(141)},
  { "ie_diagnostic_report", IE(81)},
  { "ie_diagnostic_request", IE(80)},
  { "ie_dms_request", IE(99)},
  { "ie_dms_response", IE(100)},
  { "ie_dse_registered_location", IE(58)},
  { "ie_dsss_parameter_set", IE(3)},
  { "ie_edca_parameter_set", IE(12)},
  { "ie_emergency_alart_identifier", IE(112)},
  { "ie_erp_information", IE(42)},
  { "ie_event_report", IE(79)},
  { "ie_event_request", IE(78)},
  { "ie_expedited_bandwidth_request", IE(109)},
  { "ie_extended_bss_load", IE(193)},
  { "ie_extended_capabilities", IE(127)},
  { "ie_extended_channel_switch_announcement", IE(60)},
  { "ie_extended_supported_rates", IE(50)},
  { "ie_fast_bss_transition", IE(55)},
  { "ie_fh_parameter_set", IE(2)},
  { "ie_fms_descriptor", IE(86)},
  { "ie_fms_request", IE(87)},
  { "ie_fms_response", IE(88)},
  { "ie_gann", IE(125)},
  { "ie_he_capabilities", IE(255)},
  { "ie_hopping_pattern_parameters", IE(8)},
  { "ie_hopping_pattern_table", IE(9)},
  { "ie_ht_capabilities", IE(45)},
  { "ie_ht_operation", IE(61)},
  { "ie_ibss_dfs", IE(41)},
  { "ie_ibss_parameter_set", IE(6)},
  { "ie_interworking", IE(107)},
  { "ie_link_identifier", IE(101)},
  { "ie_location_parameters", IE(82)},
  { "ie_management_mic", IE(76)},
  { "ie_mccaop", IE(124)},
  { "ie_mccaop_advertisement", IE(123)},
  { "ie_mccaop_advertisement_overview", IE(174)},
  { "ie_mccaop_setup_reply", IE(122)},
  { "ie_mccaop_setup_request", IE(121)},
  { "ie_measurement_pilot_transmission", IE(66)},
  { "ie_measurement_report", IE(39)},
  { "ie_measurement_request", IE(38)},
  { "ie_mesh_awake_window", IE(119)},
  { "ie_mesh_channel_switch_parameters", IE(118)},
  { "ie_mesh_configuration", IE(113)},
  { "ie_mesh_id", IE(114)},
  { "ie_mesh_link_metric_report", IE(115)},
  { "ie_mesh_peering_management", IE(117)},
  { "ie_mic", IE(140)},
  { "ie_mobility_domain", IE(54)},
  { "ie_multiple_bssid", IE(71)},
  { "ie_multiple_bssid_index", IE(85)},
  { "ie_neighbor_report", IE(52)},
  { "ie_nontransmitted_bssid_capability", IE(83)},
  { "ie_operating_mode_notification", IE(199)},
  { "ie_overlapping_bss_scan_parameters", IE(74)},
  { "ie_perr", IE(132)},
  { "ie_power_capability", IE(33)},
  { "ie_power_constraint", IE(32)},
  { "ie_prep", IE(131)},
  { "ie_preq", IE(130)},
  { "ie_proxy_update", IE(137)},
  { "ie_proxy_update_confirmation", IE(138)},
  { "ie_pti_control", IE(105)},
  { "ie_qos_capability", IE(46)},
  { "ie_qos_map_set", IE(110)},
  { "ie_qos_traffic_capability", IE(89)},
  { "ie_quiet", IE(40)},
  { "ie_quiet_channel", IE(198)},
  { "ie_rann", IE(126)},
  { "ie_rcpi", IE(53)},
  { "ie_request", IE(10)},
  { "ie_ric_data", IE(57)},
  { "ie_ric_descriptor", IE(75)},
  { "ie_rm_enabled_capacities", IE(70)},
  { "ie_roaming_consortium", IE(111)},
  { "ie_rsn", IE(48)},
  { "ie_rsni", IE(65)},
  { "ie_schedule", IE(15)},
  { "ie_secondary_channel_offset", IE(62)},
  { "ie_ssid", IE(0)},
  { "ie_ssid_list", IE(84)},
  { "ie_supported_channels", IE(36)},
  { "ie_supported_operating_classes", IE(59)},
  { "ie_supported_rates", IE(1)},
  { "ie_table", BITFIELD(0, 0, 0), IE_TABLE },
  { "ie_tclas", IE(14)},
  { "ie_tclas_processing", IE(44)},
  { "ie_tfs_request", IE(91)},
  { "ie_tfs_response", IE(92)},
  { "ie_tim", IE(5)},
  { "ie_tim_broadcast_request", IE(94)},
  { "ie_tim_broadcast_response", IE(95)},
  { "ie_time_advertisement", IE(69)},
  { "ie_time_zone", IE(98)},
  { "ie_timeout_interval", IE(56)},
  { "ie_tpc_report", IE(35)},
  { "ie_tpc_request", IE(34)},
  { "ie_tpu_buffer_status", IE(106)},
  { "ie_ts_delay", IE(43)},
  { "ie_tspec", IE(13)},
  { "ie_uapsd_coexistence", IE(142)},
  { "ie_vendor_specific", IE(221)},
  { "ie_vht_capabilities", IE(191)},
  { "ie_vht_operation", IE(192)},
  { "ie_vht_transmit_power_envelope", IE(195)},
  { "ie_wakeup_schedule", IE(102)},
  { "ie_wide_bandwidth_channel_switch", IE(194)},
  { "ie_wnm_sleep_mode", IE(93)},
  { "is_group", BITFIELD(1, 4, 1), 0 },
  { "legacy_length", BITFIELD(2, 0, 12), 0 },
  { "mcs", BITFIELD(4, 0, 7), 0 },
  { "moredata", BITFIELD(13, 5, 1), 0 },
  { "moreflag", BITFIELD(13, 2, 1), 0 },
  { "not_counding", BITFIELD(7, 1, 1), 0 },
  { "number", BYTEFIELD(34, 2), 0 },
  { "order", BITFIELD(13, 7, 1), 0 },
  { "protectedframe", BITFIELD(13, 6, 1), 0 },
  { "protocol", BITFIELD(12, 0, 2), 0 },
  { "pwrmgmt", BITFIELD(13, 4, 1), 0 },
  { "radio", BYTEFIELD(0, 12), IS_STRING },
  { "rate", BITFIELD(1, 0, 4), 0 },
  { "retry", BITFIELD(13, 3, 1), 0 },
  { "rssi", BYTEFIELD(0, 1), IS_SIGNED },
  { "rxend_state", BITFIELD(8, 0, 8), 0 },
  { "sgi", BITFIELD(7, 7, 1), 0 },
  { "sig_mode", BITFIELD(1, 6, 2), 0 },
  { "smoothing", BITFIELD(7, 0, 1), 0 },
  { "srcmac", BYTEFIELD(16, 6), IS_STRING },
  { "srcmac_hex", BYTEFIELD(16, 6), IS_HEXSTRING },
  { "stbc", BITFIELD(7, 4, 2), 0 },
  { "subtype", BITFIELD(12, 4, 4), 0 },
  { "tods", BITFIELD(13, 0, 1), 0 },
  { "type", BITFIELD(12, 2, 2), 0 }
};

static int8 variable_start[16] = {
   4,   // assoc req
   6,   // assoc response
  10,   // reassoc req
   6,   // reassoc resp
   0,   // probe req
  12,   // probe resp
  -1,
  -1,
  12,   // beacon
  -1,   // ATIM
   2,   // Disassociation
   6,   // authentication
   2,   // Deauthentication
   2,   // action
  -1,
  -1
};

typedef struct {
  uint16 len;
  uint8 buf[];
} packet_t;

static const LUA_REG_TYPE packet_function_map[];

static void wifi_rx_cb(uint8 *buf, uint16 len) {
  if (len != sizeof(struct sniffer_buf2)) {
    return;
  }

  struct sniffer_buf2 *snb = (struct sniffer_buf2 *) buf;
  management_request_t *mgt = (management_request_t *) snb->buf;

  if (mon_offset > len) {
    return;
  }

  if ((buf[mon_offset] & mon_mask) != mon_value) {
    return;
  }

  packet_t *packet = (packet_t *) c_malloc(len + sizeof(packet_t));
  if (packet) {
    packet->len = len;
    memcpy(packet->buf, buf, len);
    if (!task_post_medium(tasknumber, (ETSParam) packet)) {
      c_free(packet);
    }
  }
}

static void monitor_task(os_param_t param, uint8_t prio) 
{
  packet_t *input = (packet_t *) param;
  (void) prio;

  lua_State *L = lua_getstate();

  if (recv_cb != LUA_NOREF) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, recv_cb);

    packet_t *packet = (packet_t *) lua_newuserdata(L, input->len + sizeof(packet_t));
    packet->len = input->len;
    memcpy(packet->buf, input->buf, input->len);
    luaL_getmetatable(L, "wifi.packet");
    lua_setmetatable(L, -2);

    c_free(input);

    lua_call(L, 1, 0);
  } else {
    c_free(input);
  }
}

static ptrdiff_t posrelat (ptrdiff_t pos, size_t len) {
  /* relative string position: negative means back from end */
  if (pos < 0) pos += (ptrdiff_t)len + 1;
  return (pos >= 0) ? pos : 0;
}

static int packet_sub(lua_State *L, int buf_offset, int buf_length) {
  packet_t *packet = luaL_checkudata(L, 1, "wifi.packet");

  ptrdiff_t start = posrelat(luaL_checkinteger(L, 2), buf_length);
  ptrdiff_t end = posrelat(luaL_optinteger(L, 3, -1), buf_length);

  if (start < 1) start = 1;
  if (end > buf_length) end = buf_length;
  if (start <= end) {
    lua_pushlstring(L, packet->buf+start-1 + buf_offset, end-start+1);
  } else {
    lua_pushliteral(L, "");
  }

  return 1;
}

static int packet_frame_sub(lua_State *L) {
  return packet_sub(L, sizeof(struct RxControl), SNIFFER_BUF2_BUF_SIZE);
}

static int packet_radio_sub(lua_State *L) {
  return packet_sub(L, 0, sizeof(struct RxControl));
}

static void push_hex_string(lua_State *L, const uint8 *buf, int len, char *sep) {
  luaL_Buffer b;
  luaL_buffinit(L, &b);

  int i;
  for (i = 0; i < len; i++) {
    if (i && sep && *sep) {
      luaL_addstring(&b, sep);
    }
    char hex[3];

    uint8 c = buf[i];

    hex[0] = "0123456789abcdef"[c >> 4];
    hex[1] = "0123456789abcdef"[c & 0xf];
    hex[2] = 0;
    luaL_addstring(&b, hex);
  }
  
  luaL_pushresult(&b);
}

static void push_hex_string_colon(lua_State *L, const uint8 *buf, int len) {
  push_hex_string(L, buf, len, ":");
}

static int comparator(const void *key, const void *obj) {
  field_t *f = (field_t *) obj;
  const char *name = f->name;

  if (!((uint32)key & 3) && !((uint32)name & 3)) {
    // Since all strings are 3 characters or more, can do accelerated first comparison
    uint32 key32 = htonl(*(uint32 *) key);
    uint32 name32 = htonl(*(uint32 *) name);

    if (key32 < name32) {
      return -1;
    }
    if (key32 > name32) {
      return 1;
    }
  }

  return strcmp((const char *) key, name);
}

static bool push_field_value_string(lua_State *L, const uint8 *pkt,
    const uint8 *packet_end, const char *field) {

  field_t *f = bsearch(field, fields, sizeof(fields) / sizeof(fields[0]), sizeof(fields[0]), comparator);

  if (f) {
    const struct RxControl *rxc = (struct RxControl *) pkt; 
    const management_request_t *mgt = (management_request_t *) (rxc + 1);

    if (f->opts == SINGLE_IE) {
      int varstart = variable_start[mgt->framectrl.Subtype];
      if (varstart >= 0) {
        const uint8 *var = (uint8 *) (mgt + 1) + varstart;

        while (var + 2 <= packet_end && var + 2 + var[1] <= packet_end) {
          if (*var == f->length) {
            lua_pushlstring(L, var + 2, var[1]);
            return true;
          }
          var += 2 + var[1];
        }
      }

      lua_pushnil(L);

      return true;
    }

    if (f->opts == IE_TABLE) {
      lua_newtable(L);
      int varstart = variable_start[mgt->framectrl.Subtype];
      if (varstart >= 0) {
        const uint8 *var = (uint8 *) (mgt + 1) + varstart;

        while (var + 2 <= packet_end && var + 2 + var[1] <= packet_end) {
          lua_pushlstring(L, var + 2, var[1]);
          lua_rawseti(L, -2, *var);
          var += 2 + var[1];
        }
      }

      return true;
    }

    if (f->opts == IS_STRING) {
      int use = f->length >> 3;
      const uint8 *start = pkt + (f->start >> 3);
      if (start + use > packet_end) {
        use = packet_end - start;
      }
      lua_pushlstring(L, start, use);
      return true;
    }

    if (f->opts == IS_HEXSTRING) {
      int use = f->length >> 3;
      const uint8 *start = pkt + (f->start >> 3);
      if (start + use > packet_end) {
        use = packet_end - start;
      }
      push_hex_string_colon(L, start, use);
      return true;
    }

    if (f->opts == IS_HEADER) {
      int varstart = variable_start[mgt->framectrl.Subtype];
      if (varstart >= 0) {
        lua_pushlstring(L, (const uint8 *) (mgt + 1), varstart);
      } else {
        lua_pushnil(L);
      }

      return true;
    }

    if (f->opts == 0 || f->opts == IS_SIGNED) {
      // bits start from the bottom of the byte.
      int value = 0;
      int bits = 0;
      int bitoff = f->start & 7;
      int byteoff = f->start >> 3;

      while (bits < f->length) {
        uint8 b = pkt[byteoff];

        value |= (b >> bitoff) << bits;
        bits += (8 - bitoff);
        bitoff = 0;
        byteoff++;
      }
      // get rid of excess bits
      value &= (1 << f->length) - 1;
      if (f->opts & IS_SIGNED) {
        if (value & (1 << (f->length - 1))) {
          value |= - (1 << f->length);
        }
      }

      lua_pushinteger(L, value);
      return true;
    }
  }

  return false;
}

static bool push_field_value_int(lua_State *L, management_request_t *mgt, 
    const uint8 *packet_end, int field) {

  int varstart = variable_start[mgt->framectrl.Subtype];
  if (varstart >= 0) {
    uint8 *var = (uint8 *) (mgt + 1) + varstart;

    while (var + 2 <= packet_end && var + 2 + var[1] <= packet_end) {
      if (*var == field) {
        lua_pushlstring(L, var + 2, var[1]);
        return true;
      }
      var += var[1] + 2;
    }
  }

  return false;
}

static int packet_map_lookup(lua_State *L) {
  packet_t *packet = luaL_checkudata(L, 1, "wifi.packet");
  struct RxControl *rxc = (struct RxControl *) packet->buf; 
  management_request_t *mgt = (management_request_t *) (rxc + 1);
  const uint8 *packet_end = packet->buf + packet->len;

  if ((void *) (mgt + 1) > (void *) packet_end) {
    return 0;
  }
  if (mgt->framectrl.Type != FRAME_TYPE_MANAGEMENT) {
    return 0;
  }

  if (lua_type(L, 2) == LUA_TNUMBER) {
    int field = luaL_checkinteger(L, 2);
    if (push_field_value_int(L, mgt, packet_end, field)) {
      return 1;
    }
  } else {
    const char *field = luaL_checkstring(L, 2);
    if (push_field_value_string(L, packet->buf, packet_end, field)) {
      return 1;
    }

    // Now search the packet function map
    const TValue *res = luaR_findentry((void *) packet_function_map, field, 0, NULL);
    if (res) {
      luaA_pushobject(L, res);
      return 1;
    }
  }

  return 0;
}

static int packet_byte(lua_State *L, int buf_offset, int buf_length) {
  packet_t *packet = luaL_checkudata(L, 1, "wifi.packet");

  int offset = luaL_checkinteger(L, 2);

  if (offset < 1 || offset > buf_length) {
    return 0;
  }

  lua_pushinteger(L, packet->buf[offset - 1 + buf_offset]);

  return 1;
}

static int packet_frame_byte(lua_State *L) {
  return packet_byte(L, sizeof(struct RxControl), SNIFFER_BUF2_BUF_SIZE);
}

static int packet_radio_byte(lua_State *L) {
  return packet_byte(L, 0, sizeof(struct RxControl));
}

static int packet_subhex(lua_State *L, int buf_offset, int buf_length) {
  packet_t *packet = luaL_checkudata(L, 1, "wifi.packet");

  ptrdiff_t start = posrelat(luaL_checkinteger(L, 2), buf_length);
  ptrdiff_t end = posrelat(luaL_optinteger(L, 3, -1), buf_length);
  const char *sep = luaL_optstring(L, 4, "");

  if (start < 1) start = 1;
  if (end > buf_length) end = buf_length;
  if (start <= end) {
    luaL_Buffer b;
    luaL_buffinit(L, &b);

    int i;
    for (i = start - 1; i < end; i++) {
      char hex[3];

      if (i >= start) {
        luaL_addstring(&b, sep);
      }

      uint8 c = packet->buf[i + buf_offset];

      hex[0] = "0123456789abcdef"[c >> 4];
      hex[1] = "0123456789abcdef"[c & 0xf];
      hex[2] = 0;
      luaL_addstring(&b, hex);
    }
    
    luaL_pushresult(&b);
  } else {
    lua_pushliteral(L, "");
  }

  return 1;
}

static int packet_frame_subhex(lua_State *L) {
  return packet_subhex(L, sizeof(struct RxControl), SNIFFER_BUF2_BUF_SIZE);
}

static int packet_radio_subhex(lua_State *L) {
  return packet_subhex(L, 0, sizeof(struct RxControl));
}

static int wifi_monitor_start(lua_State *L) {
  int argno = 1;
  if (lua_type(L, argno) == LUA_TNUMBER) {
    int offset = luaL_checkinteger(L, argno);
    argno++;
    if (lua_type(L, argno) == LUA_TNUMBER) {
      int value = luaL_checkinteger(L, argno);

      int mask = 0xff;
      argno++;

      if (lua_type(L, argno) == LUA_TNUMBER) {
        mask = luaL_checkinteger(L, argno);
        argno++;
      }
      mon_offset = offset - 1;
      mon_value = value;
      mon_mask = mask;
    } else {
      return luaL_error(L, "Must supply offset and value");
    }
  } else {
    // Management frames by default
    mon_offset = 12;
    mon_value = 0x00;
    mon_mask = 0x0C;
  }
  if (lua_type(L, argno) == LUA_TFUNCTION || lua_type(L, argno) == LUA_TLIGHTFUNCTION)
  {
    lua_pushvalue(L, argno);  // copy argument (func) to the top of stack
    recv_cb = luaL_ref(L, LUA_REGISTRYINDEX);
    // this is very delicate code. If the timing is wrong, then the code crashes
    // as it appears that the sniffer buffers have not been allocated.
    wifi_station_set_auto_connect(0);
    os_delay_us(1000);
    wifi_set_opmode_current(1);
    os_delay_us(1000);
    wifi_promiscuous_enable(0);
    os_delay_us(1000);
    wifi_station_disconnect();
    os_delay_us(1000);
    wifi_set_promiscuous_rx_cb(wifi_rx_cb);
    wifi_set_channel(1);
    wifi_promiscuous_enable(1);
    return 0;
  }
  return luaL_error(L, "Missing callback");
}

static int wifi_monitor_channel(lua_State *L) {
  lua_pushinteger(L, wifi_get_channel());
  if (lua_type(L, 1) == LUA_TNUMBER) {
    int channel = luaL_checkinteger(L, 1);

    if (channel < 1 || channel > 15) {
      return luaL_error(L, "Channel number (%d) is out of range", channel);
    }
    wifi_set_channel(channel);
  }

  return 1;
}

static int wifi_monitor_stop(lua_State *L) {
  wifi_promiscuous_enable(0);
  wifi_set_opmode_current(1);
  luaL_unref(L, LUA_REGISTRYINDEX, recv_cb);
  recv_cb = LUA_NOREF;
  return 0;
}

static const LUA_REG_TYPE packet_function_map[] = {
  { LSTRKEY( "radio_byte" ),        LFUNCVAL( packet_radio_byte ) },
  { LSTRKEY( "frame_byte" ),        LFUNCVAL( packet_frame_byte ) },
  { LSTRKEY( "radio_sub" ),         LFUNCVAL( packet_radio_sub ) },
  { LSTRKEY( "frame_sub" ),         LFUNCVAL( packet_frame_sub ) },
  { LSTRKEY( "radio_subhex" ),      LFUNCVAL( packet_radio_subhex ) },
  { LSTRKEY( "frame_subhex" ),      LFUNCVAL( packet_frame_subhex ) },
  { LNILKEY, LNILVAL }
};

static const LUA_REG_TYPE packet_map[] = {
  { LSTRKEY( "__index" ),     LFUNCVAL( packet_map_lookup ) },
  { LNILKEY, LNILVAL }
};

// Module function map
const LUA_REG_TYPE wifi_monitor_map[] = {
  { LSTRKEY( "start" ),      LFUNCVAL( wifi_monitor_start ) },
  { LSTRKEY( "stop" ),       LFUNCVAL( wifi_monitor_stop ) },
  { LSTRKEY( "channel" ),    LFUNCVAL( wifi_monitor_channel ) },
  { LNILKEY, LNILVAL }
};

int wifi_monitor_init(lua_State *L)
{
  luaL_rometatable(L, "wifi.packet", (void *)packet_map);
  tasknumber = task_get_id(monitor_task);
  return 0;
}

//NODEMCU_MODULE(WIFI_MONITOR, "wifi_monitor", wifi_monitor_map, wifi_monitor_init);
