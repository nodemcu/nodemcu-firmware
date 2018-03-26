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

#if defined(LUA_USE_MODULES_WIFI)

#ifdef WIFI_SDK_EVENT_MONITOR_ENABLE

//variables for wifi event monitor
static task_handle_t wifi_event_monitor_task_id; //variable to hold task id for task handler(process_event_queue)

static int wifi_event_cb_ref[EVENT_MAX+1] = { [0 ... EVENT_MAX] = LUA_NOREF}; //holds references to registered Lua callbacks

#ifdef LUA_USE_MODULES_WIFI_MONITOR
static int (*hook_fn)(System_Event_t *);

void wifi_event_monitor_register_hook(int (*fn)(System_Event_t*)) {
  hook_fn = fn;
}
#endif

// wifi.eventmon.register()
int wifi_event_monitor_register(lua_State* L)
{
  uint8 id = (uint8)luaL_checknumber(L, 1);
  if ( id > EVENT_MAX ) //Check if user is trying to register a callback for a valid event.
  {
    return luaL_error( L, "valid wifi events:0-%d", EVENT_MAX );
  }
  else
  {
    if (lua_type(L, 2) == LUA_TFUNCTION || lua_type(L, 2) == LUA_TLIGHTFUNCTION) //check if 2nd item on stack is a function
    {
      lua_pushvalue(L, 2);  // copy argument (func) to the top of stack
      register_lua_cb(L, &wifi_event_cb_ref[id]);  //pop function from top of the stack, register it in the LUA_REGISTRY, then assign lua_ref to wifi_event_cb_ref[id]
    }
    else // unregister user's callback
    {
      unregister_lua_cb(L, &wifi_event_cb_ref[id]);
    }
    return 0;
  }
}

static sint32_t event_queue_ref = LUA_NOREF;

static void wifi_event_monitor_handle_event_cb(System_Event_t *evt)
{
  EVENT_DBG("was called (Event:%d)", evt->event);

#ifdef LUA_USE_MODULES_WIFI_MONITOR
  if (hook_fn && hook_fn(evt)) {
    return;
  }
#endif

  if((wifi_event_cb_ref[evt->event] != LUA_NOREF) || ((wifi_event_cb_ref[EVENT_MAX] != LUA_NOREF) &&
      !(evt->event == EVENT_STAMODE_CONNECTED || evt->event == EVENT_STAMODE_DISCONNECTED ||
          evt->event == EVENT_STAMODE_AUTHMODE_CHANGE || evt->event == EVENT_STAMODE_GOT_IP ||
          evt->event == EVENT_STAMODE_DHCP_TIMEOUT || evt->event == EVENT_SOFTAPMODE_STACONNECTED ||
          evt->event == EVENT_SOFTAPMODE_STADISCONNECTED || evt->event == EVENT_SOFTAPMODE_PROBEREQRECVED ||
          evt->event == EVENT_OPMODE_CHANGED)))
  {
    lua_State* L = lua_getstate();
    if(event_queue_ref == LUA_NOREF){ //if event queue has not been created, create it now
      lua_newtable(L);
      event_queue_ref = luaL_ref(L, LUA_REGISTRYINDEX);
    }
    lua_rawgeti(L, LUA_REGISTRYINDEX, event_queue_ref);

    System_Event_t* evt_tmp = lua_newuserdata(L, sizeof(System_Event_t));
    c_memcpy(evt_tmp, evt, sizeof(System_Event_t)); //copy event data to new struct
    sint32_t evt_ud_ref = luaL_ref(L, LUA_REGISTRYINDEX);
    size_t queue_len = lua_objlen(L, -1);

    //add event to queue
    lua_pushnumber(L, queue_len+1);
    lua_pushnumber(L, evt_ud_ref);
    lua_rawset(L, -3);

    if(queue_len == 0){ //if queue was empty, post task
      EVENT_DBG("Posting task");
      task_post_low(wifi_event_monitor_task_id, false);
    }
    else{
      EVENT_DBG("Appending queue, items in queue: %d", lua_objlen(L, -1));

    }
    lua_pop(L, 1);
   }  //else{} //there are no callbacks registered, so the event can't be processed
}

static void wifi_event_monitor_process_event_queue(task_param_t param, uint8 priority)
{
  lua_State* L = lua_getstate();
  lua_rawgeti(L, LUA_REGISTRYINDEX, event_queue_ref);
  int index = 1;
  lua_rawgeti(L, 1, index);
  sint32 event_ref = lua_tonumber(L, -1);
  lua_pop(L, 1);

  //remove event reference from queue
  int queue_length = lua_objlen(L, 1);
  lua_rawgeti(L, 1, index);
  for(; index<queue_length;index++){
    lua_rawgeti(L, 1, index+1);
    lua_rawseti(L, 1, index);
  }
  lua_pushnil(L);
  lua_rawseti(L, 1, queue_length);
  lua_pop(L, 1);

  lua_rawgeti(L, LUA_REGISTRYINDEX, event_ref); //get event userdata from registry
  System_Event_t *evt = lua_touserdata(L, -1);

  lua_pop(L, 1); //pop userdata from stack

  queue_length = lua_objlen(L, 1);
  if (queue_length>0){
    task_post_low(wifi_event_monitor_task_id, false); //post task to process next item in queue
    EVENT_DBG("%d events left in queue, posting task", queue_length);
  }
  lua_pop(L, 1); //pop event queue from stack

  if(wifi_event_cb_ref[evt->event] != LUA_NOREF) // check if user has registered a callback
  {
    lua_rawgeti(L, LUA_REGISTRYINDEX, wifi_event_cb_ref[evt->event]); //get user's callback
  }
  else if((wifi_event_cb_ref[EVENT_MAX]!=LUA_NOREF) &&
      !(evt->event==EVENT_STAMODE_CONNECTED||evt->event==EVENT_STAMODE_DISCONNECTED||
          evt->event==EVENT_STAMODE_AUTHMODE_CHANGE||evt->event==EVENT_STAMODE_GOT_IP||
          evt->event==EVENT_STAMODE_DHCP_TIMEOUT||evt->event==EVENT_SOFTAPMODE_STACONNECTED||
          evt->event==EVENT_SOFTAPMODE_STADISCONNECTED||evt->event==EVENT_SOFTAPMODE_PROBEREQRECVED))
  { //if user has registered an EVENT_MAX(default) callback and event is not implemented...
    lua_rawgeti(L, LUA_REGISTRYINDEX, wifi_event_cb_ref[EVENT_MAX]); //get user's callback
  }

  lua_newtable( L );

  switch (evt->event)
  {
    case EVENT_STAMODE_CONNECTED:
      EVENT_DBG("Event: %d (STAMODE_CONNECTED)", EVENT_STAMODE_CONNECTED);
      wifi_add_sprintf_field(L, "SSID", (char*)evt->event_info.connected.ssid);
      wifi_add_sprintf_field(L, "BSSID", MACSTR, MAC2STR(evt->event_info.connected.bssid));
      wifi_add_int_field(L, "channel", evt->event_info.connected.channel);
      EVENT_DBG("Connected to SSID %s, Channel %d",
          evt->event_info.connected.ssid,
          evt->event_info.connected.channel);
      break;

    case EVENT_STAMODE_DISCONNECTED:
      EVENT_DBG("Event: %d (STAMODE_DISCONNECTED)", EVENT_STAMODE_DISCONNECTED);
      wifi_add_sprintf_field(L, "SSID", (char*)evt->event_info.disconnected.ssid);
      wifi_add_int_field(L, "reason", evt->event_info.disconnected.reason);
      wifi_add_sprintf_field(L, "BSSID", MACSTR, MAC2STR(evt->event_info.disconnected.bssid));
      EVENT_DBG("Disconnect from SSID %s, reason %d",
          evt->event_info.disconnected.ssid,
          evt->event_info.disconnected.reason);
      break;

    case EVENT_STAMODE_AUTHMODE_CHANGE:
      EVENT_DBG("Event: %d (STAMODE_AUTHMODE_CHANGE)", EVENT_STAMODE_AUTHMODE_CHANGE);
      wifi_add_int_field(L, "old_auth_mode", evt->event_info.auth_change.old_mode);
      wifi_add_int_field(L, "new_auth_mode", evt->event_info.auth_change.new_mode);
      EVENT_DBG("Authmode: %u -> %u",
          evt->event_info.auth_change.old_mode,
          evt->event_info.auth_change.new_mode);
      break;

    case EVENT_STAMODE_GOT_IP:
      EVENT_DBG("Event: %d (STAMODE_GOT_IP)", EVENT_STAMODE_GOT_IP);
      wifi_add_sprintf_field(L, "IP", IPSTR, IP2STR(&evt->event_info.got_ip.ip));
      wifi_add_sprintf_field(L, "netmask", IPSTR, IP2STR(&evt->event_info.got_ip.mask));
      wifi_add_sprintf_field(L, "gateway", IPSTR, IP2STR(&evt->event_info.got_ip.gw));
      EVENT_DBG("IP:" IPSTR ",Mask:" IPSTR ",GW:" IPSTR "",
          IP2STR(&evt->event_info.got_ip.ip),
          IP2STR(&evt->event_info.got_ip.mask),
          IP2STR(&evt->event_info.got_ip.gw));
      break;

    case EVENT_STAMODE_DHCP_TIMEOUT:
      EVENT_DBG("Event: %d (STAMODE_DHCP_TIMEOUT)", EVENT_STAMODE_DHCP_TIMEOUT);
      break;

    case EVENT_SOFTAPMODE_STACONNECTED:
      EVENT_DBG("Event: %d (SOFTAPMODE_STACONNECTED)", EVENT_SOFTAPMODE_STACONNECTED);
      wifi_add_sprintf_field(L, "MAC", MACSTR, MAC2STR(evt->event_info.sta_connected.mac));
      wifi_add_int_field(L, "AID", evt->event_info.sta_connected.aid);
      EVENT_DBG("Station: " MACSTR "join, AID = %d",
          MAC2STR(evt->event_info.sta_connected.mac),
          evt->event_info.sta_connected.aid);
      break;

    case EVENT_SOFTAPMODE_STADISCONNECTED:
      EVENT_DBG("Event: %d (SOFTAPMODE_STADISCONNECTED)", EVENT_SOFTAPMODE_STADISCONNECTED);
      wifi_add_sprintf_field(L, "MAC", MACSTR, MAC2STR(evt->event_info.sta_disconnected.mac));
      wifi_add_int_field(L, "AID", evt->event_info.sta_disconnected.aid);
      EVENT_DBG("station: " MACSTR "leave, AID = %d",
          MAC2STR(evt->event_info.sta_disconnected.mac),
          evt->event_info.sta_disconnected.aid);
      break;

    case EVENT_SOFTAPMODE_PROBEREQRECVED:
      EVENT_DBG("Event: %d (SOFTAPMODE_PROBEREQRECVED)", EVENT_SOFTAPMODE_PROBEREQRECVED);
      wifi_add_sprintf_field(L, "MAC", MACSTR, MAC2STR(evt->event_info.ap_probereqrecved.mac));
      wifi_add_int_field(L, "RSSI", evt->event_info.ap_probereqrecved.rssi);
      EVENT_DBG("Station PROBEREQ: " MACSTR " RSSI = %d",
          MAC2STR(evt->event_info.ap_probereqrecved.mac),
          evt->event_info.ap_probereqrecved.rssi);
      break;

    case EVENT_OPMODE_CHANGED:
      EVENT_DBG("Event: %d (OPMODE_CHANGED)", EVENT_OPMODE_CHANGED);
      wifi_add_int_field(L, "old_mode", evt->event_info.opmode_changed.old_opmode);
      wifi_add_int_field(L, "new_mode", evt->event_info.opmode_changed.new_opmode);
      EVENT_DBG("opmode: %u -> %u",
          evt->event_info.opmode_changed.old_opmode,
          evt->event_info.opmode_changed.new_opmode);
      break;

    default://if event is not implemented, return event id
      EVENT_DBG("Event: %d (switch/case default)", evt->event);
      wifi_add_sprintf_field(L, "info", "event %u not implemented", evt->event);
      break;
  }


  luaL_unref(L, LUA_REGISTRYINDEX, event_ref); //the userdata containing event info is no longer needed
  event_ref = LUA_NOREF;

  lua_call(L, 1, 0); //execute user's callback and pass Lua table
  return;
}

#ifdef WIFI_EVENT_MONITOR_DISCONNECT_REASON_LIST_ENABLE
static const LUA_REG_TYPE wifi_event_monitor_reason_map[] =
{
  { LSTRKEY( "UNSPECIFIED" ),              LNUMVAL( REASON_UNSPECIFIED ) },
  { LSTRKEY( "AUTH_EXPIRE" ),              LNUMVAL( REASON_AUTH_EXPIRE ) },
  { LSTRKEY( "AUTH_LEAVE" ),               LNUMVAL( REASON_AUTH_LEAVE ) },
  { LSTRKEY( "ASSOC_EXPIRE" ),             LNUMVAL( REASON_ASSOC_EXPIRE ) },
  { LSTRKEY( "ASSOC_TOOMANY" ),            LNUMVAL( REASON_ASSOC_TOOMANY ) },
  { LSTRKEY( "NOT_AUTHED" ),               LNUMVAL( REASON_NOT_AUTHED ) },
  { LSTRKEY( "NOT_ASSOCED" ),              LNUMVAL( REASON_NOT_ASSOCED ) },
  { LSTRKEY( "ASSOC_LEAVE" ),              LNUMVAL( REASON_ASSOC_LEAVE ) },
  { LSTRKEY( "ASSOC_NOT_AUTHED" ),         LNUMVAL( REASON_ASSOC_NOT_AUTHED ) },
  { LSTRKEY( "DISASSOC_PWRCAP_BAD" ),      LNUMVAL( REASON_DISASSOC_PWRCAP_BAD ) },
  { LSTRKEY( "DISASSOC_SUPCHAN_BAD" ),     LNUMVAL( REASON_DISASSOC_SUPCHAN_BAD ) },
  { LSTRKEY( "IE_INVALID" ),               LNUMVAL( REASON_IE_INVALID ) },
  { LSTRKEY( "MIC_FAILURE" ),              LNUMVAL( REASON_MIC_FAILURE ) },
  { LSTRKEY( "4WAY_HANDSHAKE_TIMEOUT" ),   LNUMVAL( REASON_4WAY_HANDSHAKE_TIMEOUT ) },
  { LSTRKEY( "GROUP_KEY_UPDATE_TIMEOUT" ), LNUMVAL( REASON_GROUP_KEY_UPDATE_TIMEOUT ) },
  { LSTRKEY( "IE_IN_4WAY_DIFFERS" ),       LNUMVAL( REASON_IE_IN_4WAY_DIFFERS ) },
  { LSTRKEY( "GROUP_CIPHER_INVALID" ),     LNUMVAL( REASON_GROUP_CIPHER_INVALID ) },
  { LSTRKEY( "PAIRWISE_CIPHER_INVALID" ),  LNUMVAL( REASON_PAIRWISE_CIPHER_INVALID ) },
  { LSTRKEY( "AKMP_INVALID" ),             LNUMVAL( REASON_AKMP_INVALID ) },
  { LSTRKEY( "UNSUPP_RSN_IE_VERSION" ),    LNUMVAL( REASON_UNSUPP_RSN_IE_VERSION ) },
  { LSTRKEY( "INVALID_RSN_IE_CAP" ),       LNUMVAL( REASON_INVALID_RSN_IE_CAP ) },
  { LSTRKEY( "802_1X_AUTH_FAILED" ),       LNUMVAL( REASON_802_1X_AUTH_FAILED ) },
  { LSTRKEY( "CIPHER_SUITE_REJECTED" ),    LNUMVAL( REASON_CIPHER_SUITE_REJECTED ) },
  { LSTRKEY( "BEACON_TIMEOUT" ),           LNUMVAL( REASON_BEACON_TIMEOUT ) },
  { LSTRKEY( "NO_AP_FOUND" ),              LNUMVAL( REASON_NO_AP_FOUND ) },
  { LSTRKEY( "AUTH_FAIL" ),                LNUMVAL( REASON_AUTH_FAIL ) },
  { LSTRKEY( "ASSOC_FAIL" ),               LNUMVAL( REASON_ASSOC_FAIL ) },
  { LSTRKEY( "HANDSHAKE_TIMEOUT" ),        LNUMVAL( REASON_HANDSHAKE_TIMEOUT ) },
  { LNILKEY, LNILVAL }
};
#endif

const LUA_REG_TYPE wifi_event_monitor_map[] =
{
  { LSTRKEY( "register" ),            LFUNCVAL( wifi_event_monitor_register ) },
  { LSTRKEY( "unregister" ),          LFUNCVAL( wifi_event_monitor_register ) },
  { LSTRKEY( "STA_CONNECTED" ),       LNUMVAL( EVENT_STAMODE_CONNECTED ) },
  { LSTRKEY( "STA_DISCONNECTED" ),    LNUMVAL( EVENT_STAMODE_DISCONNECTED ) },
  { LSTRKEY( "STA_AUTHMODE_CHANGE" ), LNUMVAL( EVENT_STAMODE_AUTHMODE_CHANGE ) },
  { LSTRKEY( "STA_GOT_IP" ),          LNUMVAL( EVENT_STAMODE_GOT_IP ) },
  { LSTRKEY( "STA_DHCP_TIMEOUT" ),    LNUMVAL( EVENT_STAMODE_DHCP_TIMEOUT ) },
  { LSTRKEY( "AP_STACONNECTED" ),     LNUMVAL( EVENT_SOFTAPMODE_STACONNECTED ) },
  { LSTRKEY( "AP_STADISCONNECTED" ),  LNUMVAL( EVENT_SOFTAPMODE_STADISCONNECTED ) },
  { LSTRKEY( "AP_PROBEREQRECVED" ),   LNUMVAL( EVENT_SOFTAPMODE_PROBEREQRECVED ) },
  { LSTRKEY( "WIFI_MODE_CHANGED" ),   LNUMVAL( EVENT_OPMODE_CHANGED ) },
  { LSTRKEY( "EVENT_MAX" ),           LNUMVAL( EVENT_MAX ) },
#ifdef WIFI_EVENT_MONITOR_DISCONNECT_REASON_LIST_ENABLE
  { LSTRKEY( "reason" ),              LROVAL( wifi_event_monitor_reason_map ) },
#endif
  { LNILKEY, LNILVAL }
};

void wifi_eventmon_init()
{
  wifi_event_monitor_task_id = task_get_id(wifi_event_monitor_process_event_queue);//get task id from task interface
  wifi_set_event_handler_cb(wifi_event_monitor_handle_event_cb);
  return;
}

#endif
#endif

