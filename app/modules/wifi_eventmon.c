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

#ifdef WIFI_STATION_STATUS_MONITOR_ENABLE

//variables for wifi event monitor
static int wifi_station_status_cb_ref[6] = {[0 ... 6-1] = LUA_NOREF};
static os_timer_t wifi_sta_status_timer;
static uint8 prev_wifi_status=0;

// wifi.sta.eventMonStop()
void wifi_station_event_mon_stop(lua_State* L)
{
  os_timer_disarm(&wifi_sta_status_timer);
  if(lua_isstring(L,1))
  {
    int i;
    for (i=0; i<6; i++)
    {
      unregister_lua_cb(L, &wifi_station_status_cb_ref[i]);
    }
  }
  return;
}

static void wifi_status_cb(int arg)
{
  lua_State* L = lua_getstate();
  if (wifi_get_opmode() == SOFTAP_MODE)
  {
    os_timer_disarm(&wifi_sta_status_timer);
    return;
  }
  int wifi_status = wifi_station_get_connect_status();
  if (wifi_status != prev_wifi_status)
  {
    if(wifi_station_status_cb_ref[wifi_status] != LUA_NOREF)
    {
      lua_rawgeti(L, LUA_REGISTRYINDEX, wifi_station_status_cb_ref[wifi_status]);
      lua_pushnumber(L, prev_wifi_status);
      lua_call(L, 1, 0);
    }
  }
  prev_wifi_status = wifi_status;
}

// wifi.sta.eventMonReg()
int wifi_station_event_mon_reg(lua_State* L)
{
  uint8 id=(uint8)luaL_checknumber(L, 1);
  if ((id > 5)) // verify user specified a valid wifi status
  {
    return luaL_error( L, "valid wifi status:0-5" );
  }

  if (lua_type(L, 2) == LUA_TFUNCTION || lua_type(L, 2) == LUA_TLIGHTFUNCTION) //check if 2nd item on stack is a function
  {
    lua_pushvalue(L, 2); //push function to top of stack
    register_lua_cb(L, &wifi_station_status_cb_ref[id]);//pop function from top of the stack, register it in the LUA_REGISTRY, then assign returned lua_ref to wifi_station_status_cb_ref[id]
  }
  else
  {
    unregister_lua_cb(L, &wifi_station_status_cb_ref[id]); // unregister user's callback
  }
  return 0;
}


// wifi.sta.eventMonStart()
int wifi_station_event_mon_start(lua_State* L)
{
  if(wifi_get_opmode() == SOFTAP_MODE) //Verify ESP is in either Station mode or StationAP mode
  {
    return luaL_error( L, "Can't monitor in SOFTAP mode" );
  }
  if (wifi_station_status_cb_ref[0] == LUA_NOREF && wifi_station_status_cb_ref[1] == LUA_NOREF &&
      wifi_station_status_cb_ref[2] == LUA_NOREF && wifi_station_status_cb_ref[3] == LUA_NOREF &&
      wifi_station_status_cb_ref[4] == LUA_NOREF && wifi_station_status_cb_ref[5] == LUA_NOREF )
  { //verify user has registered callbacks
    return luaL_error( L, "No callbacks defined" );
  }
  uint32 ms = 150; //set default timer interval
  if(lua_isnumber(L, 1)) // check if user has specified a different timer interval
  {
    ms=luaL_checknumber(L, 1); // retrieve user-defined interval
  }

  os_timer_disarm(&wifi_sta_status_timer);
  os_timer_setfn(&wifi_sta_status_timer, (os_timer_func_t *)wifi_status_cb, NULL);
  os_timer_arm(&wifi_sta_status_timer, ms, 1);
  return 0;
}

#endif

#ifdef WIFI_SDK_EVENT_MONITOR_ENABLE

//variables for wifi event monitor
static task_handle_t wifi_event_monitor_task_id; //variable to hold task id for task handler(process_event_queue)

typedef struct evt_queue{
    System_Event_t *evt;
    struct evt_queue * next;
}evt_queue_t; //structure to hold pointers to event info and next item in queue

static evt_queue_t *wifi_event_queue_head; //pointer to beginning of queue
static evt_queue_t *wifi_event_queue_tail; //pointer to end of queue
static int wifi_event_cb_ref[EVENT_MAX+1] = { [0 ... EVENT_MAX] = LUA_NOREF}; //holds references to registered Lua callbacks

// wifi.eventmon.register()
static int wifi_event_monitor_register(lua_State* L)
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

static void wifi_event_monitor_handle_event_cb(System_Event_t *evt)
{
  EVENT_DBG("\n\twifi_event_monitor_handle_event_cb is called\n");

  if((wifi_event_cb_ref[evt->event] != LUA_NOREF) || ((wifi_event_cb_ref[EVENT_MAX] != LUA_NOREF) &&
      !(evt->event == EVENT_STAMODE_CONNECTED || evt->event == EVENT_STAMODE_DISCONNECTED ||
          evt->event == EVENT_STAMODE_AUTHMODE_CHANGE||evt->event==EVENT_STAMODE_GOT_IP ||
          evt->event == EVENT_STAMODE_DHCP_TIMEOUT||evt->event==EVENT_SOFTAPMODE_STACONNECTED ||
          evt->event == EVENT_SOFTAPMODE_STADISCONNECTED||evt->event==EVENT_SOFTAPMODE_PROBEREQRECVED)))
  {
    evt_queue_t *temp = (evt_queue_t*)c_malloc(sizeof(evt_queue_t)); //allocate memory for new queue item
    temp->evt = (System_Event_t*)c_malloc(sizeof(System_Event_t)); //allocate memory to hold event structure
    if(!temp || !temp->evt)
    {
      luaL_error(lua_getstate(), "wifi.eventmon malloc: out of memory");
      return;
    }
    c_memcpy(temp->evt, evt, sizeof(System_Event_t)); //copy event data to new struct

    if(wifi_event_queue_head == NULL && wifi_event_queue_tail == NULL)// if queue is empty add item to queue
    {
      wifi_event_queue_head = wifi_event_queue_tail = temp;
      EVENT_DBG("\n\tqueue empty, adding event and posting task\n");
      task_post_low(wifi_event_monitor_task_id, false);
    }
    else //if queue is not empty append item to end of queue
    {
      wifi_event_queue_tail->next=temp;
      wifi_event_queue_tail=temp;
      EVENT_DBG("\n\tqueue not empty, appending queue\n");
    }
  }
}


static void wifi_event_monitor_process_event_queue(task_param_t param, uint8 priority)
{
  lua_State* L = lua_getstate();
  evt_queue_t *temp = wifi_event_queue_head; //copy event_queue_head pointer to temporary pointer
  System_Event_t *evt = temp->evt; //copy event data pointer to temporary pointer

  EVENT_DBG("\t\tevent %u\n", evt->event);

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
      EVENT_DBG("\n\tSTAMODE_CONNECTED\n");
      wifi_add_sprintf_field(L, "SSID", (char*)evt->event_info.connected.ssid);
      wifi_add_sprintf_field(L, "BSSID", MACSTR, MAC2STR(evt->event_info.connected.bssid));
      wifi_add_int_field(L, "channel", evt->event_info.connected.channel);
      EVENT_DBG("\tConnected to SSID %s, Channel %d\n",
          evt->event_info.connected.ssid,
          evt->event_info.connected.channel);
      break;

    case EVENT_STAMODE_DISCONNECTED:
      EVENT_DBG("\n\tSTAMODE_DISCONNECTED\n");
      wifi_add_sprintf_field(L, "SSID", (char*)evt->event_info.disconnected.ssid);
      wifi_add_int_field(L, "reason", evt->event_info.disconnected.reason);
      wifi_add_sprintf_field(L, "BSSID", MACSTR, MAC2STR(evt->event_info.disconnected.bssid));
      EVENT_DBG("\tDisconnect from SSID %s, reason %d\n",
          evt->event_info.disconnected.ssid,
          evt->event_info.disconnected.reason);
      break;

    case EVENT_STAMODE_AUTHMODE_CHANGE:
      EVENT_DBG("\n\tSTAMODE_AUTHMODE_CHANGE\n");
      wifi_add_int_field(L, "old_auth_mode", evt->event_info.auth_change.old_mode);
      wifi_add_int_field(L, "new_auth_mode", evt->event_info.auth_change.new_mode);
      EVENT_DBG("\tAuthmode: %u -> %u\n",
          evt->event_info.auth_change.old_mode,
          evt->event_info.auth_change.new_mode);
      break;

    case EVENT_STAMODE_GOT_IP:
      EVENT_DBG("\n\tGOT_IP\n");
      wifi_add_sprintf_field(L, "IP", IPSTR, IP2STR(&evt->event_info.got_ip.ip));
      wifi_add_sprintf_field(L, "netmask", IPSTR, IP2STR(&evt->event_info.got_ip.mask));
      wifi_add_sprintf_field(L, "gateway", IPSTR, IP2STR(&evt->event_info.got_ip.gw));
      EVENT_DBG("\tIP:" IPSTR ",Mask:" IPSTR ",GW:" IPSTR "\n",
          IP2STR(&evt->event_info.got_ip.ip),
          IP2STR(&evt->event_info.got_ip.mask),
          IP2STR(&evt->event_info.got_ip.gw));
      break;

    case EVENT_STAMODE_DHCP_TIMEOUT:
      EVENT_DBG("\n\tSTAMODE_DHCP_TIMEOUT\n");
      break;

    case EVENT_SOFTAPMODE_STACONNECTED:
      EVENT_DBG("\n\tSOFTAPMODE_STACONNECTED\n");
      wifi_add_sprintf_field(L, "MAC", MACSTR, MAC2STR(evt->event_info.sta_connected.mac));
      wifi_add_int_field(L, "AID", evt->event_info.sta_connected.aid);
      EVENT_DBG("\tStation: " MACSTR "join, AID = %d\n",
          MAC2STR(evt->event_info.sta_connected.mac),
          evt->event_info.sta_connected.aid);
      break;

    case EVENT_SOFTAPMODE_STADISCONNECTED:
      EVENT_DBG("\n\tSOFTAPMODE_STADISCONNECTED\n");
      wifi_add_sprintf_field(L, "MAC", MACSTR, MAC2STR(evt->event_info.sta_disconnected.mac));
      wifi_add_int_field(L, "AID", evt->event_info.sta_disconnected.aid);
      EVENT_DBG("\tstation: " MACSTR "leave, AID = %d\n",
          MAC2STR(evt->event_info.sta_disconnected.mac),
          evt->event_info.sta_disconnected.aid);
      break;

    case EVENT_SOFTAPMODE_PROBEREQRECVED:
      EVENT_DBG("\n\tSOFTAPMODE_PROBEREQRECVED\n");
      wifi_add_sprintf_field(L, "MAC", MACSTR, MAC2STR(evt->event_info.ap_probereqrecved.mac));
      wifi_add_int_field(L, "RSSI", evt->event_info.ap_probereqrecved.rssi);
      EVENT_DBG("Station PROBEREQ: " MACSTR " RSSI = %d\n",
          MAC2STR(evt->event_info.ap_probereqrecved.mac),
          evt->event_info.ap_probereqrecved.rssi);
      break;

    default://if event is not implemented, push table with userdata containing event data
      EVENT_DBG("\n\tswitch/case default\n");
      wifi_add_sprintf_field(L, "info", "event %u not implemented", evt->event);
      break;
  }

  lua_call(L, 1, 0); //execute user's callback and pass Lua table

  if (wifi_event_queue_head == wifi_event_queue_tail) //if queue is empty..
  {
    wifi_event_queue_head = wifi_event_queue_tail = NULL; //set queue pointers to NULL
    EVENT_DBG("\n\tQueue empty\n");
  }
  else //if queue is not empty...
  {
    wifi_event_queue_head = wifi_event_queue_head->next; //append item to end of queue
    EVENT_DBG("\n\tmore in queue, posting task...\n");
    task_post_low(wifi_event_monitor_task_id, false); //post task to process next item in queue
  }

  c_free(evt); //free memory used by event structure
  c_free(temp); //free memory used by queue structure
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

