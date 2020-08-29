// WiFi Event Monitor

#include "module.h"
#include "lauxlib.h"
#include "platform.h"

#include <string.h>
#include <stddef.h>

#include <stdint.h>
#include "user_interface.h"
#include "smart/smart.h"
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
  uint8 id = (uint8)luaL_checkinteger(L, 1);
  if ( id > EVENT_MAX ) //Check if user is trying to register a callback for a valid event.
  {
    return luaL_error( L, "valid wifi events:0-%d", EVENT_MAX );
  }
  else
  {
    if (lua_isfunction(L, 2)) //check if 2nd item on stack is a function
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
    memcpy(evt_tmp, evt, sizeof(System_Event_t)); //copy event data to new struct
    sint32_t evt_ud_ref = luaL_ref(L, LUA_REGISTRYINDEX);
    size_t queue_len = lua_objlen(L, -1);

    //add event to queue
    lua_pushinteger(L, queue_len+1);
    lua_pushinteger(L, evt_ud_ref);
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
  sint32 event_ref = lua_tointeger(L, -1);
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

  luaL_pcallx(L, 1, 0); //execute user's callback and pass Lua table
  return;
}

#ifdef WIFI_EVENT_MONITOR_DISCONNECT_REASON_LIST_ENABLE
LROT_BEGIN(wifi_event_monitor_reason, NULL, 0)
  LROT_NUMENTRY( UNSPECIFIED, REASON_UNSPECIFIED )
  LROT_NUMENTRY( AUTH_EXPIRE, REASON_AUTH_EXPIRE )
  LROT_NUMENTRY( AUTH_LEAVE, REASON_AUTH_LEAVE )
  LROT_NUMENTRY( ASSOC_EXPIRE, REASON_ASSOC_EXPIRE )
  LROT_NUMENTRY( ASSOC_TOOMANY, REASON_ASSOC_TOOMANY )
  LROT_NUMENTRY( NOT_AUTHED, REASON_NOT_AUTHED )
  LROT_NUMENTRY( NOT_ASSOCED, REASON_NOT_ASSOCED )
  LROT_NUMENTRY( ASSOC_LEAVE, REASON_ASSOC_LEAVE )
  LROT_NUMENTRY( ASSOC_NOT_AUTHED, REASON_ASSOC_NOT_AUTHED )
  LROT_NUMENTRY( DISASSOC_PWRCAP_BAD, REASON_DISASSOC_PWRCAP_BAD )
  LROT_NUMENTRY( DISASSOC_SUPCHAN_BAD, REASON_DISASSOC_SUPCHAN_BAD )
  LROT_NUMENTRY( IE_INVALID, REASON_IE_INVALID )
  LROT_NUMENTRY( MIC_FAILURE, REASON_MIC_FAILURE )
  LROT_NUMENTRY( 4WAY_HANDSHAKE_TIMEOUT, REASON_4WAY_HANDSHAKE_TIMEOUT )
  LROT_NUMENTRY( GROUP_KEY_UPDATE_TIMEOUT, REASON_GROUP_KEY_UPDATE_TIMEOUT )
  LROT_NUMENTRY( IE_IN_4WAY_DIFFERS, REASON_IE_IN_4WAY_DIFFERS )
  LROT_NUMENTRY( GROUP_CIPHER_INVALID, REASON_GROUP_CIPHER_INVALID )
  LROT_NUMENTRY( PAIRWISE_CIPHER_INVALID, REASON_PAIRWISE_CIPHER_INVALID )
  LROT_NUMENTRY( AKMP_INVALID, REASON_AKMP_INVALID )
  LROT_NUMENTRY( UNSUPP_RSN_IE_VERSION, REASON_UNSUPP_RSN_IE_VERSION )
  LROT_NUMENTRY( INVALID_RSN_IE_CAP, REASON_INVALID_RSN_IE_CAP )
  LROT_NUMENTRY( 802_1X_AUTH_FAILED, REASON_802_1X_AUTH_FAILED )
  LROT_NUMENTRY( CIPHER_SUITE_REJECTED, REASON_CIPHER_SUITE_REJECTED )
  LROT_NUMENTRY( BEACON_TIMEOUT, REASON_BEACON_TIMEOUT )
  LROT_NUMENTRY( NO_AP_FOUND, REASON_NO_AP_FOUND )
  LROT_NUMENTRY( AUTH_FAIL, REASON_AUTH_FAIL )
  LROT_NUMENTRY( ASSOC_FAIL, REASON_ASSOC_FAIL )
  LROT_NUMENTRY( HANDSHAKE_TIMEOUT, REASON_HANDSHAKE_TIMEOUT )
LROT_END(wifi_event_monitor_reason, NULL, 0)

#endif

LROT_BEGIN(wifi_event_monitor, NULL, 0)
  LROT_FUNCENTRY( register, wifi_event_monitor_register )
  LROT_FUNCENTRY( unregister, wifi_event_monitor_register )
  LROT_NUMENTRY( STA_CONNECTED, EVENT_STAMODE_CONNECTED )
  LROT_NUMENTRY( STA_DISCONNECTED, EVENT_STAMODE_DISCONNECTED )
  LROT_NUMENTRY( STA_AUTHMODE_CHANGE, EVENT_STAMODE_AUTHMODE_CHANGE )
  LROT_NUMENTRY( STA_GOT_IP, EVENT_STAMODE_GOT_IP )
  LROT_NUMENTRY( STA_DHCP_TIMEOUT, EVENT_STAMODE_DHCP_TIMEOUT )
  LROT_NUMENTRY( AP_STACONNECTED, EVENT_SOFTAPMODE_STACONNECTED )
  LROT_NUMENTRY( AP_STADISCONNECTED, EVENT_SOFTAPMODE_STADISCONNECTED )
  LROT_NUMENTRY( AP_PROBEREQRECVED, EVENT_SOFTAPMODE_PROBEREQRECVED )
  LROT_NUMENTRY( WIFI_MODE_CHANGED, EVENT_OPMODE_CHANGED )
  LROT_NUMENTRY( EVENT_MAX, EVENT_MAX )
#ifdef WIFI_EVENT_MONITOR_DISCONNECT_REASON_LIST_ENABLE
  LROT_TABENTRY( reason, wifi_event_monitor_reason )
#endif
LROT_END(wifi_event_monitor, NULL, 0)


void wifi_eventmon_init()
{
  wifi_event_monitor_task_id = task_get_id(wifi_event_monitor_process_event_queue);//get task id from task interface
  wifi_set_event_handler_cb(wifi_event_monitor_handle_event_cb);
  return;
}

#endif
#endif

