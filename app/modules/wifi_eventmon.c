// WiFi Event Monitor

#include "module.h"
#include "lauxlib.h"
#include "platform.h"

#include "c_string.h"
#include "c_stdlib.h"

#include "c_types.h"
#include "user_interface.h"
#include "smart.h"
#include "smartconfig.h"
#include "user_config.h"

#include "wifi_common.h"

#if defined(LUA_USE_MODULES_WIFI) && defined(LUA_USE_MODULES_WIFI_EVENTMON)

#ifdef USE_WIFI_STATION_STATUS_MONITOR

//variables for wifi event monitor
static sint32_t wifi_status_cb_ref[6] = {[0 ... 6-1] = LUA_NOREF};
static os_timer_t wifi_sta_status_timer;
static uint8 prev_wifi_status=0;

  // wifi.sta.eventMonStop()
void wifi_station_event_mon_stop(lua_State* L)
{
  os_timer_disarm(&wifi_sta_status_timer);
  if(lua_isnumber(L,1)||lua_isstring(L,1))
  {
	int i;
	for (i=0;i<6;i++)
  	{
  	  if(wifi_status_cb_ref[i] != LUA_NOREF)
	  {
	    luaL_unref(L, LUA_REGISTRYINDEX, wifi_status_cb_ref[i]);
		wifi_status_cb_ref[i] = LUA_NOREF;
	  }
	}
  }
}

static void wifi_status_cb(int arg)
{
  if (wifi_get_opmode()==2)
  {
	  os_timer_disarm(&wifi_sta_status_timer);
	  return;
  }
  int wifi_status=wifi_station_get_connect_status();
  if (wifi_status!=prev_wifi_status)
  {
 	if(wifi_status_cb_ref[wifi_status]!=LUA_NOREF)
 	{
	  lua_rawgeti(gL, LUA_REGISTRYINDEX, wifi_status_cb_ref[wifi_status]);
	  lua_call(gL, 0, 0);
 	}
  }
  prev_wifi_status=wifi_status;
}

// wifi.sta.eventMonReg()
int wifi_station_event_mon_reg(lua_State* L)
{
  uint8 id=(uint8)luaL_checknumber(L, 1);
  if ((id > 5))
  {
	return luaL_error( L, "valid wifi status:0-5" );
  }

  if (lua_type(L, 2) == LUA_TFUNCTION || lua_type(L, 2) == LUA_TLIGHTFUNCTION)
  {
    lua_pushvalue(L, 2);
    if(wifi_status_cb_ref[id] != LUA_NOREF)
    {
      luaL_unref(L, LUA_REGISTRYINDEX, wifi_status_cb_ref[id]);
    }
    wifi_status_cb_ref[id] = luaL_ref(L, LUA_REGISTRYINDEX);
  }
  else
  {
    if(wifi_status_cb_ref[id] != LUA_NOREF)
    {
      luaL_unref(L, LUA_REGISTRYINDEX, wifi_status_cb_ref[id]);
      wifi_status_cb_ref[id] = LUA_NOREF;
    }
  }
  return 0;
}


// wifi.sta.eventMonStart()
int wifi_station_event_mon_start(lua_State* L)
{
//  gL=lua_getstate();
  if(wifi_get_opmode() == SOFTAP_MODE)
  {
	return luaL_error( L, "Can't monitor in SOFTAP mode" );
  }
  if (wifi_status_cb_ref[0]==LUA_NOREF && wifi_status_cb_ref[1]==LUA_NOREF &&
		  wifi_status_cb_ref[2]==LUA_NOREF && wifi_status_cb_ref[3]==LUA_NOREF &&
		  wifi_status_cb_ref[4]==LUA_NOREF && wifi_status_cb_ref[5]==LUA_NOREF )
  {
		return luaL_error( L, "No callbacks defined" );
  }
  uint32 ms=150;
  if(lua_isnumber(L, 1))
  {
    ms=luaL_checknumber(L, 1);
  }

  os_timer_disarm(&wifi_sta_status_timer);
  os_timer_setfn(&wifi_sta_status_timer, (os_timer_func_t *)wifi_status_cb, NULL);
  os_timer_arm(&wifi_sta_status_timer, ms, 1);
  return 0;
}

#endif

#ifdef USE_WIFI_SDK_EVENT_MONITOR

//variables for wifi event monitor
static sint32_t wifi_event_cb_ref[EVENT_MAX+1] = { [0 ... EVENT_MAX] = LUA_NOREF};


static int wifi_event_mon_reg(lua_State* L)
{
  uint8 id=(uint8)luaL_checknumber(L, 1);
  if ( id > EVENT_MAX )
  {
	return luaL_error( L, "valid wifi events:0-%d", EVENT_MAX );
  }
  else
  {
	if (lua_type(L, 2) == LUA_TFUNCTION || lua_type(L, 2) == LUA_TLIGHTFUNCTION)
	{
      lua_pushvalue(L, 2);  // copy argument (func) to the top of stack
      if(wifi_event_cb_ref[id] != LUA_NOREF)
      {
        luaL_unref(L, LUA_REGISTRYINDEX, wifi_event_cb_ref[id]);
      }
      wifi_event_cb_ref[id] = luaL_ref(L, LUA_REGISTRYINDEX);
	}
	else
	{
      if(wifi_event_cb_ref[id] != LUA_NOREF)
      {
    	luaL_unref(L, LUA_REGISTRYINDEX, wifi_event_cb_ref[id]);
    	wifi_event_cb_ref[id] = LUA_NOREF;
      }
	}
    return 0;
  }
}


static void wifi_handle_event_cb(System_Event_t *evt)
{
  NODE_DBG("\t\tevent %u\n", evt->event);
  if(evt->event <= EVENT_MAX)
  {
	if(wifi_event_cb_ref[evt->event]!=LUA_NOREF)
	{
	  lua_rawgeti(gL, LUA_REGISTRYINDEX, wifi_event_cb_ref[evt->event]);
	}
	else if((wifi_event_cb_ref[EVENT_MAX]!=LUA_NOREF) &&
        !(evt->event==EVENT_STAMODE_CONNECTED||evt->event==EVENT_STAMODE_DISCONNECTED||
         evt->event==EVENT_STAMODE_AUTHMODE_CHANGE||evt->event==EVENT_STAMODE_GOT_IP||
		 evt->event==EVENT_STAMODE_DHCP_TIMEOUT||evt->event==EVENT_SOFTAPMODE_STACONNECTED||
		 evt->event==EVENT_SOFTAPMODE_STADISCONNECTED||evt->event==EVENT_SOFTAPMODE_PROBEREQRECVED))
	{
	  lua_rawgeti(gL, LUA_REGISTRYINDEX, wifi_event_cb_ref[EVENT_MAX]);
	}
	else
	{
	  return;
	}
  }
  else
  {
	return;
  }
  lua_newtable( gL );

  switch (evt->event)
  {
    case EVENT_STAMODE_CONNECTED:
	  NODE_DBG("\n\tSTAMODE_CONNECTED\n");
	  lua_table_add_string(gL, "SSID", (char *) evt->event_info.connected.ssid);
	  lua_table_add_string(gL, "BSSID", MACSTR, MAC2STR(evt->event_info.connected.bssid));
	  lua_table_add_int(gL, "channel", evt->event_info.connected.channel);
	  NODE_DBG("\tConnected to SSID %s, Channel %d\n",
			   evt->event_info.connected.ssid,
			   evt->event_info.connected.channel);
	  break;

	case EVENT_STAMODE_DISCONNECTED:
	  NODE_DBG("\n\tSTAMODE_DISCONNECTED\n");
	  lua_table_add_string(gL, "SSID", (char *) evt->event_info.disconnected.ssid);
	  lua_table_add_int(gL, "reason", evt->event_info.disconnected.reason);
	  lua_table_add_string(gL, "BSSID", MACSTR, MAC2STR(evt->event_info.disconnected.bssid));
	  NODE_DBG("\tDisconnect from SSID %s, reason %d\n",
			   evt->event_info.disconnected.ssid,
			   evt->event_info.disconnected.reason);
	  break;

	case EVENT_STAMODE_AUTHMODE_CHANGE:
	  NODE_DBG("\n\tSTAMODE_AUTHMODE_CHANGE\n");
	  lua_table_add_int(gL, "old_auth_mode", evt->event_info.auth_change.old_mode);
	  lua_table_add_int(gL, "new_auth_mode", evt->event_info.auth_change.new_mode);
	  NODE_DBG("\tAuthmode: %u -> %u\n",
			   evt->event_info.auth_change.old_mode,
			   evt->event_info.auth_change.new_mode);
	  break;

	case EVENT_STAMODE_GOT_IP:
      NODE_DBG("\n\tGOT_IP\n");
      lua_table_add_string(gL, "IP", IPSTR, IP2STR(&evt->event_info.got_ip.ip));
      lua_table_add_string(gL, "netmask", IPSTR, IP2STR(&evt->event_info.got_ip.mask));
      lua_table_add_string(gL, "gateway", IPSTR, IP2STR(&evt->event_info.got_ip.gw));
      NODE_DBG("\tIP:" IPSTR ",Mask:" IPSTR ",GW:" IPSTR "\n",
    		   IP2STR(&evt->event_info.got_ip.ip),
			   IP2STR(&evt->event_info.got_ip.mask),
			   IP2STR(&evt->event_info.got_ip.gw));
      break;

	case EVENT_STAMODE_DHCP_TIMEOUT:
	  NODE_DBG("\n\tSTAMODE_DHCP_TIMEOUT\n");
	  break;

	case EVENT_SOFTAPMODE_STACONNECTED:
      NODE_DBG("\n\tSOFTAPMODE_STACONNECTED\n");
      lua_table_add_string(gL, "MAC", MACSTR, MAC2STR(evt->event_info.sta_connected.mac));
      lua_table_add_int(gL, "AID", evt->event_info.sta_connected.aid);
      NODE_DBG("\tStation: " MACSTR "join, AID = %d\n",
    		   MAC2STR(evt->event_info.sta_connected.mac),
			   evt->event_info.sta_connected.aid);
      break;

	case EVENT_SOFTAPMODE_STADISCONNECTED:
	  NODE_DBG("\n\tSOFTAPMODE_STADISCONNECTED\n");
	  lua_table_add_string(gL, "MAC", MACSTR, MAC2STR(evt->event_info.sta_disconnected.mac));
	  lua_table_add_int(gL, "AID", evt->event_info.sta_disconnected.aid);
	  NODE_DBG("\tstation: " MACSTR "leave, AID = %d\n",
			   MAC2STR(evt->event_info.sta_disconnected.mac),
			   evt->event_info.sta_disconnected.aid);
	  break;

	case EVENT_SOFTAPMODE_PROBEREQRECVED:
	  NODE_DBG("\n\tSOFTAPMODE_PROBEREQRECVED\n");
	  lua_table_add_string(gL, "MAC", MACSTR, MAC2STR(evt->event_info.ap_probereqrecved.mac));
	  lua_table_add_int(gL, "RSSI", evt->event_info.ap_probereqrecved.rssi);
	  NODE_DBG("Station PROBEREQ: " MACSTR " RSSI = %d\n",
			   MAC2STR(evt->event_info.ap_probereqrecved.mac),
			   evt->event_info.ap_probereqrecved.rssi);
	  break;



	default:
	  NODE_DBG("\n\tswitch/case default\n");
	  lua_table_add_string(gL, "info", "event %u not implemented", evt->event);
	  break;
  }
	lua_call(gL, 1, 0);
}

//#undef INCLUDE_WIFI_EVENT_DISCONNECT_REASON_LIST

#ifdef INCLUDE_WIFI_EVENT_DISCONNECT_REASON_LIST
static const LUA_REG_TYPE event_monitor_reason_map[] =
{
  { LSTRKEY( "UNSPECIFIED" ), LNUMVAL( REASON_UNSPECIFIED ) },
  { LSTRKEY( "AUTH_EXPIRE" ), LNUMVAL( REASON_AUTH_EXPIRE ) },
  { LSTRKEY( "AUTH_LEAVE" ), LNUMVAL	( REASON_AUTH_LEAVE ) },
  { LSTRKEY( "ASSOC_EXPIRE" ), LNUMVAL( REASON_ASSOC_EXPIRE ) },
  { LSTRKEY( "ASSOC_TOOMANY" ), LNUMVAL( REASON_ASSOC_TOOMANY ) },
  { LSTRKEY( "NOT_AUTHED" ), LNUMVAL( REASON_NOT_AUTHED ) },
  { LSTRKEY( "NOT_ASSOCED" ), LNUMVAL( REASON_NOT_ASSOCED ) },
  { LSTRKEY( "ASSOC_LEAVE" ), LNUMVAL( REASON_ASSOC_LEAVE ) },
  { LSTRKEY( "ASSOC_NOT_AUTHED" ), LNUMVAL( REASON_ASSOC_NOT_AUTHED ) },
  { LSTRKEY( "DISASSOC_PWRCAP_BAD" ), LNUMVAL( REASON_DISASSOC_PWRCAP_BAD ) },
  { LSTRKEY( "DISASSOC_SUPCHAN_BAD" ), LNUMVAL( REASON_DISASSOC_SUPCHAN_BAD ) },
  { LSTRKEY( "IE_INVALID" ), LNUMVAL( REASON_IE_INVALID ) },
  { LSTRKEY( "MIC_FAILURE" ), LNUMVAL( REASON_MIC_FAILURE ) },
  { LSTRKEY( "4WAY_HANDSHAKE_TIMEOUT" ), LNUMVAL( REASON_4WAY_HANDSHAKE_TIMEOUT ) },
  { LSTRKEY( "GROUP_KEY_UPDATE_TIMEOUT" ), LNUMVAL( REASON_GROUP_KEY_UPDATE_TIMEOUT ) },
  { LSTRKEY( "IE_IN_4WAY_DIFFERS" ), LNUMVAL( REASON_IE_IN_4WAY_DIFFERS ) },
  { LSTRKEY( "GROUP_CIPHER_INVALID" ), LNUMVAL( REASON_GROUP_CIPHER_INVALID ) },
  { LSTRKEY( "PAIRWISE_CIPHER_INVALID" ), LNUMVAL( REASON_PAIRWISE_CIPHER_INVALID ) },
  { LSTRKEY( "AKMP_INVALID" ), LNUMVAL( REASON_AKMP_INVALID ) },
  { LSTRKEY( "UNSUPP_RSN_IE_VERSION" ), LNUMVAL( REASON_UNSUPP_RSN_IE_VERSION ) },
  { LSTRKEY( "INVALID_RSN_IE_CAP" ), LNUMVAL( REASON_INVALID_RSN_IE_CAP ) },
  { LSTRKEY( "802_1X_AUTH_FAILED" ), LNUMVAL( REASON_802_1X_AUTH_FAILED ) },
  { LSTRKEY( "CIPHER_SUITE_REJECTED" ), LNUMVAL( REASON_CIPHER_SUITE_REJECTED ) },
  { LSTRKEY( "BEACON_TIMEOUT" ), LNUMVAL( REASON_BEACON_TIMEOUT ) },
  { LSTRKEY( "NO_AP_FOUND" ), LNUMVAL( REASON_NO_AP_FOUND ) },
  { LSTRKEY( "AUTH_FAIL" ), LNUMVAL( REASON_AUTH_FAIL ) },
  { LSTRKEY( "ASSOC_FAIL" ), LNUMVAL( REASON_ASSOC_FAIL ) },
  { LSTRKEY( "HANDSHAKE_TIMEOUT" ), LNUMVAL( REASON_HANDSHAKE_TIMEOUT ) },
  { LNILKEY, LNILVAL }
};
#endif

const LUA_REG_TYPE wifi_eventmon_map[] =
{
  { LSTRKEY( "register" ), LFUNCVAL( wifi_event_mon_reg ) },
  { LSTRKEY( "unregister" ), LFUNCVAL( wifi_event_mon_reg ) },
  { LSTRKEY( "STA_CONNECTED" ), LNUMVAL( EVENT_STAMODE_CONNECTED ) },
  { LSTRKEY( "STA_DISCONNECTED" ), LNUMVAL( EVENT_STAMODE_DISCONNECTED ) },
  { LSTRKEY( "STA_AUTHMODE_CHANGE" ), LNUMVAL( EVENT_STAMODE_AUTHMODE_CHANGE ) },
  { LSTRKEY( "STA_GOT_IP" ), LNUMVAL( EVENT_STAMODE_GOT_IP ) },
  { LSTRKEY( "STA_DHCP_TIMEOUT" ), LNUMVAL( EVENT_STAMODE_DHCP_TIMEOUT ) },
  { LSTRKEY( "AP_STACONNECTED" ), LNUMVAL( EVENT_SOFTAPMODE_STACONNECTED ) },
  { LSTRKEY( "AP_STADISCONNECTED" ), LNUMVAL( EVENT_SOFTAPMODE_STADISCONNECTED ) },
  { LSTRKEY( "AP_PROBEREQRECVED" ), LNUMVAL( EVENT_SOFTAPMODE_PROBEREQRECVED ) },
  { LSTRKEY( "EVENT_MAX" ), LNUMVAL( EVENT_MAX ) },
#ifdef INCLUDE_WIFI_EVENT_DISCONNECT_REASON_LIST
  { LSTRKEY( "reason" ), LROVAL( event_monitor_reason_map ) },
#endif
  { LNILKEY, LNILVAL }
};

int luaopen_wifi_eventmon( lua_State *L )
{
  gL=lua_getstate();
  wifi_set_event_handler_cb(wifi_handle_event_cb);
  return 0;
}

NODEMCU_MODULE(WIFI_EVENTMON, "\0WEM", wifi_eventmon_map, luaopen_wifi_eventmon);

#endif
#endif
