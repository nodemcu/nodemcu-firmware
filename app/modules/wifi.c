// Module for interfacing with WIFI

//#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
#include "platform.h"
#include "auxmods.h"
#include "lrotable.h"

#include "c_string.h"

#include "c_types.h"
#include "user_interface.h"
#include "smart.h"
#include "smartconfig.h"

static int wifi_smart_succeed = LUA_NOREF;

static void wifi_smart_succeed_cb(void *arg){
  NODE_DBG("wifi_smart_succeed_cb is called.\n");
  if( !arg )
    return;
#if 0
  struct station_config *sta_conf = arg;
  wifi_station_set_config(sta_conf);
  wifi_station_disconnect();
  wifi_station_connect();
  smartconfig_stop();
#endif
  if(wifi_smart_succeed == LUA_NOREF)
    return;
  lua_State* L = (lua_State *)arg;
  lua_rawgeti(L, LUA_REGISTRYINDEX, wifi_smart_succeed);
  lua_call(L, 0, 0);
}

static int wifi_scan_succeed = LUA_NOREF;
static lua_State* gL = NULL;
/**
  * @brief  Wifi ap scan over callback to display.
  * @param  arg: contain the aps information
  * @param  status: scan over status
  * @retval None
  */
static void wifi_scan_done(void *arg, STATUS status)
{
  uint8 ssid[33];
  char temp[128];
  if(wifi_scan_succeed == LUA_NOREF)
    return;
  if(arg == NULL)
    return;
  
  lua_rawgeti(gL, LUA_REGISTRYINDEX, wifi_scan_succeed);

  if (status == OK)
  {
    struct bss_info *bss_link = (struct bss_info *)arg;
    bss_link = bss_link->next.stqe_next;//ignore first
    lua_newtable( gL );

    while (bss_link != NULL)
    {
      c_memset(ssid, 0, 33);
      if (c_strlen(bss_link->ssid) <= 32)
      {
        c_memcpy(ssid, bss_link->ssid, c_strlen(bss_link->ssid));
      }
      else
      {
        c_memcpy(ssid, bss_link->ssid, 32);
      }
      c_sprintf(temp,"%d,%d,"MACSTR",%d", bss_link->authmode, bss_link->rssi,
                 MAC2STR(bss_link->bssid),bss_link->channel);

      lua_pushstring(gL, temp);
      lua_setfield( gL, -2, ssid );

      // NODE_DBG(temp);

      bss_link = bss_link->next.stqe_next;
    }
  }
  else
  {
    lua_pushnil(gL);
  }
  lua_call(gL, 1, 0);
}

// Lua: smart(channel, function succeed_cb)
// Lua: smart(type, function succeed_cb)
static int wifi_start_smart( lua_State* L )
{
  unsigned channel;
  int stack = 1;
  
  if ( lua_isnumber(L, stack) ){
    channel = lua_tointeger(L, stack);
    stack++;
  } else {
    channel = 6;
  }

  // luaL_checkanyfunction(L, stack);
  if (lua_type(L, stack) == LUA_TFUNCTION || lua_type(L, stack) == LUA_TLIGHTFUNCTION){
    lua_pushvalue(L, stack);  // copy argument (func) to the top of stack
    if(wifi_smart_succeed != LUA_NOREF)
      luaL_unref(L, LUA_REGISTRYINDEX, wifi_smart_succeed);
    wifi_smart_succeed = luaL_ref(L, LUA_REGISTRYINDEX);
  }

  if ( channel > 14 || channel < 1 )
    return luaL_error( L, "wrong arg range" );

  if(wifi_smart_succeed == LUA_NOREF){
    smart_begin(channel, NULL, NULL);
  }else{
    smart_begin(channel, (smart_succeed )wifi_smart_succeed_cb, L);
  }
  // smartconfig_start(0, wifi_smart_succeed_cb);
  return 0;  
}

// Lua: exit_smart()
static int wifi_exit_smart( lua_State* L )
{
  smart_end();
  // smartconfig_stop();
  if(wifi_smart_succeed != LUA_NOREF)
    luaL_unref(L, LUA_REGISTRYINDEX, wifi_smart_succeed);
  wifi_smart_succeed = LUA_NOREF;
  return 0;  
}

// Lua: realmode = setmode(mode)
static int wifi_setmode( lua_State* L )
{
  unsigned mode;
  
  mode = luaL_checkinteger( L, 1 );

  if ( mode != STATION_MODE && mode != SOFTAP_MODE && mode != STATIONAP_MODE )
    return luaL_error( L, "wrong arg type" );
  wifi_set_opmode( (uint8_t)mode);
  mode = (unsigned)wifi_get_opmode();
  lua_pushinteger( L, mode );
  return 1;  
}

// Lua: realmode = getmode()
static int wifi_getmode( lua_State* L )
{
  unsigned mode;
  mode = (unsigned)wifi_get_opmode();
  lua_pushinteger( L, mode );
  return 1;  
}


// Lua: mac = wifi.xx.getmac()
static int wifi_getmac( lua_State* L, uint8_t mode )
{
  char temp[64];
  uint8_t mac[6];
  wifi_get_macaddr(mode, mac);
  c_sprintf(temp, "%02X-%02X-%02X-%02X-%02X-%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5] );
  lua_pushstring( L, temp );
  return 1;  
}

// Lua: mac = wifi.xx.setmac()
static int wifi_setmac( lua_State* L, uint8_t mode )
{
  unsigned len = 0;
  const char *mac = luaL_checklstring( L, 1, &len );
  if(len!=6)
    return luaL_error( L, "wrong arg type" );

  lua_pushboolean(L,wifi_set_macaddr(mode, (uint8 *)mac));
  return 1;
}

// Lua: ip = wifi.xx.getip()
static int wifi_getip( lua_State* L, uint8_t mode )
{
  struct ip_info pTempIp;
  char temp[64];
  wifi_get_ip_info(mode, &pTempIp);
  if(pTempIp.ip.addr==0){
    lua_pushnil(L);
    return 1;  
  } else {
    c_sprintf(temp, "%d.%d.%d.%d", IP2STR(&pTempIp.ip) );
    lua_pushstring( L, temp );
    c_sprintf(temp, "%d.%d.%d.%d", IP2STR(&pTempIp.netmask) );
    lua_pushstring( L, temp );
    c_sprintf(temp, "%d.%d.%d.%d", IP2STR(&pTempIp.gw) );
    lua_pushstring( L, temp );
    return 3;
  }
}

// Lua: broadcast = wifi.xx.getbroadcast()
static int wifi_getbroadcast( lua_State* L, uint8_t mode )
{
  struct ip_info pTempIp;
  char temp[64];
  wifi_get_ip_info(mode, &pTempIp);
  if(pTempIp.ip.addr==0){
    lua_pushnil(L);
    return 1;  
  } else {

    struct ip_addr broadcast_address;

    uint32 subnet_mask32 = pTempIp.netmask.addr & pTempIp.ip.addr;
    uint32 broadcast_address32 = ~pTempIp.netmask.addr | subnet_mask32;
    broadcast_address.addr = broadcast_address32;

    c_sprintf(temp, "%d.%d.%d.%d", IP2STR(&broadcast_address) );
    lua_pushstring( L, temp );

    return 1;
  }
}


static uint32_t parse_key(lua_State* L, const char * key){
  lua_getfield(L, 1, key);
  if( lua_isstring(L, -1) )   // deal with the ip/netmask/gw string
  {
    const char *ip = luaL_checkstring( L, -1 );
    return ipaddr_addr(ip);
  }
  lua_pop(L, 1);
  return 0;
}

// Lua: ip = wifi.xx.setip()
static int wifi_setip( lua_State* L, uint8_t mode )
{
  struct ip_info pTempIp;
  wifi_get_ip_info(mode, &pTempIp);

  if (!lua_istable(L, 1))
    return luaL_error( L, "wrong arg type" );
  uint32_t ip = parse_key(L, "ip");
  if(ip!=0) 
    pTempIp.ip.addr = ip;

  ip = parse_key(L, "netmask");
  if(ip!=0) 
    pTempIp.netmask.addr = ip;

  ip = parse_key(L, "gateway");
  if(ip!=0) 
    pTempIp.gw.addr = ip;
  
  if(STATION_IF == mode)
  {
    wifi_station_dhcpc_stop();
    lua_pushboolean(L,wifi_set_ip_info(mode, &pTempIp));
  }
  else
  {
    wifi_softap_dhcps_stop();
    lua_pushboolean(L,wifi_set_ip_info(mode, &pTempIp));
    wifi_softap_dhcps_start();
  }

  return 1;  
}

// Lua: realtype = sleeptype(type)
static int wifi_sleeptype( lua_State* L )
{
  unsigned type;
  
  if ( lua_isnumber(L, 1) ){
    type = lua_tointeger(L, 1);
    if ( type != NONE_SLEEP_T && type != LIGHT_SLEEP_T && type != MODEM_SLEEP_T )
      return luaL_error( L, "wrong arg type" );
    if(!wifi_set_sleep_type(type)){
      lua_pushnil(L);
      return 1;
    }
  }

  type = wifi_get_sleep_type();
  lua_pushinteger( L, type );
  return 1;  
}

// Lua: wifi.sta.getmac()
static int wifi_station_getmac( lua_State* L ){
  return wifi_getmac(L, STATION_IF);
}

// Lua: wifi.sta.setmac()
static int wifi_station_setmac( lua_State* L ){
  return wifi_setmac(L, STATION_IF);
}

// Lua: wifi.sta.getip()
static int wifi_station_getip( lua_State* L ){
  return wifi_getip(L, STATION_IF);
}

// Lua: wifi.sta.setip()
static int wifi_station_setip( lua_State* L ){
  return wifi_setip(L, STATION_IF);
}

// Lua: wifi.sta.getbroadcast()
static int wifi_station_getbroadcast( lua_State* L ){
  return wifi_getbroadcast(L, STATION_IF);
}

// Lua: wifi.sta.config(ssid, password)
static int wifi_station_config( lua_State* L )
{
  size_t sl, pl;
  struct station_config sta_conf;
  int i;
  const char *ssid = luaL_checklstring( L, 1, &sl );
  if (sl>32 || ssid == NULL)
    return luaL_error( L, "ssid:<32" );
  const char *password = luaL_checklstring( L, 2, &pl );
  if (pl>64 || password == NULL)
    return luaL_error( L, "pwd:<64" );

  c_memset(sta_conf.ssid, 0, 32);
  c_memset(sta_conf.password, 0, 64);
  c_memset(sta_conf.bssid, 0, 6);
  c_memcpy(sta_conf.ssid, ssid, sl);
  c_memcpy(sta_conf.password, password, pl);
  sta_conf.bssid_set = 0;

  NODE_DBG(sta_conf.ssid);
  NODE_DBG(" %d\n", sl);
  NODE_DBG(sta_conf.password);
  NODE_DBG(" %d\n", pl);

  wifi_station_set_config(&sta_conf);
  wifi_station_set_auto_connect(true);
  wifi_station_disconnect();
  wifi_station_connect();
  // station_check_connect(0);
  return 0;  
}

// Lua: wifi.sta.connect()
static int wifi_station_connect4lua( lua_State* L )
{
  wifi_station_connect();
  return 0;  
}

// Lua: wifi.sta.disconnect()
static int wifi_station_disconnect4lua( lua_State* L )
{
  wifi_station_disconnect();
  return 0;  
}

// Lua: wifi.sta.auto(true/false)
static int wifi_station_setauto( lua_State* L )
{
  unsigned a;
  
  a = luaL_checkinteger( L, 1 );

  if ( a != 0 && a != 1 )
    return luaL_error( L, "wrong arg type" );
  wifi_station_set_auto_connect(a);
  if(a){
    // station_check_connect(0);
  }
  return 0;  
}

static int wifi_station_listap( lua_State* L )
{
  if(wifi_get_opmode() == SOFTAP_MODE)
  {
    return luaL_error( L, "Can't list ap in SOFTAP mode" );
  }
  gL = L;
  // luaL_checkanyfunction(L, 1);
  if (lua_type(L, 1) == LUA_TFUNCTION || lua_type(L, 1) == LUA_TLIGHTFUNCTION){
    lua_pushvalue(L, 1);  // copy argument (func) to the top of stack
    if(wifi_scan_succeed != LUA_NOREF)
      luaL_unref(L, LUA_REGISTRYINDEX, wifi_scan_succeed);
    wifi_scan_succeed = luaL_ref(L, LUA_REGISTRYINDEX);
    wifi_station_scan(NULL,wifi_scan_done);
  } else {
    if(wifi_scan_succeed != LUA_NOREF)
      luaL_unref(L, LUA_REGISTRYINDEX, wifi_scan_succeed);
    wifi_scan_succeed = LUA_NOREF;
  }
}

// Lua: wifi.sta.status()
static int wifi_station_status( lua_State* L )
{
  uint8_t status = wifi_station_get_connect_status();
  lua_pushinteger( L, status );
  return 1; 
}

// Lua: wifi.ap.getmac()
static int wifi_ap_getmac( lua_State* L ){
  return wifi_getmac(L, SOFTAP_IF);
}

// Lua: wifi.ap.setmac()
static int wifi_ap_setmac( lua_State* L ){
  return wifi_setmac(L, SOFTAP_IF);
}

// Lua: wifi.ap.getip()
static int wifi_ap_getip( lua_State* L ){
  return wifi_getip(L, SOFTAP_IF);
}

// Lua: wifi.ap.setip()
static int wifi_ap_setip( lua_State* L ){
  return wifi_setip(L, SOFTAP_IF);
}

// Lua: wifi.ap.getbroadcast()
static int wifi_ap_getbroadcast( lua_State* L ){
  return wifi_getbroadcast(L, SOFTAP_IF);
}

// Lua: wifi.ap.config(table)
static int wifi_ap_config( lua_State* L )
{
  struct softap_config config;
  size_t len;
  wifi_softap_get_config(&config);
  if (!lua_istable(L, 1))
    return luaL_error( L, "wrong arg type" );

  lua_getfield(L, 1, "ssid");
  if (!lua_isnil(L, -1)){  /* found? */
    if( lua_isstring(L, -1) )   // deal with the ssid string
    {
      const char *ssid = luaL_checklstring( L, -1, &len );
      if(len>32)
        return luaL_error( L, "ssid:<32" );
      c_memset(config.ssid, 0, 32);
      c_memcpy(config.ssid, ssid, len);
      config.ssid_len = len;
      config.ssid_hidden = 0;
      NODE_DBG(config.ssid);
      NODE_DBG("\n");
    } 
    else
      return luaL_error( L, "wrong arg type" );
  }
  else
    return luaL_error( L, "wrong arg type" );

  lua_getfield(L, 1, "pwd");
  if (!lua_isnil(L, -1)){  /* found? */
    if( lua_isstring(L, -1) )   // deal with the password string
    {
      const char *pwd = luaL_checklstring( L, -1, &len );
      if(len>64)
        return luaL_error( L, "pwd:<64" );
      c_memset(config.password, 0, 64);
      c_memcpy(config.password, pwd, len);
      config.authmode = AUTH_WPA_WPA2_PSK;
      NODE_DBG(config.password);
      NODE_DBG("\n");
    }
    else
      return luaL_error( L, "wrong arg type" );
  }
  else{
    config.authmode = AUTH_OPEN;
  }

  config.max_connection = 4;

  wifi_softap_set_config(&config);
  // system_restart();
  return 0;  
}

// Module function map
#define MIN_OPT_LEVEL 2
#include "lrodefs.h"
static const LUA_REG_TYPE wifi_station_map[] =
{
  { LSTRKEY( "config" ), LFUNCVAL ( wifi_station_config ) },
  { LSTRKEY( "connect" ), LFUNCVAL ( wifi_station_connect4lua ) },
  { LSTRKEY( "disconnect" ), LFUNCVAL ( wifi_station_disconnect4lua ) },
  { LSTRKEY( "autoconnect" ), LFUNCVAL ( wifi_station_setauto ) },
  { LSTRKEY( "getip" ), LFUNCVAL ( wifi_station_getip ) },
  { LSTRKEY( "setip" ), LFUNCVAL ( wifi_station_setip ) },
  { LSTRKEY( "getbroadcast" ), LFUNCVAL ( wifi_station_getbroadcast) },
  { LSTRKEY( "getmac" ), LFUNCVAL ( wifi_station_getmac ) },
  { LSTRKEY( "setmac" ), LFUNCVAL ( wifi_station_setmac ) },
  { LSTRKEY( "getap" ), LFUNCVAL ( wifi_station_listap ) },
  { LSTRKEY( "status" ), LFUNCVAL ( wifi_station_status ) },
  { LNILKEY, LNILVAL }
};

static const LUA_REG_TYPE wifi_ap_map[] =
{
  { LSTRKEY( "config" ), LFUNCVAL( wifi_ap_config ) },
  { LSTRKEY( "getip" ), LFUNCVAL ( wifi_ap_getip ) },
  { LSTRKEY( "setip" ), LFUNCVAL ( wifi_ap_setip ) },
  { LSTRKEY( "getbroadcast" ), LFUNCVAL ( wifi_ap_getbroadcast) },
  { LSTRKEY( "getmac" ), LFUNCVAL ( wifi_ap_getmac ) },
  { LSTRKEY( "setmac" ), LFUNCVAL ( wifi_ap_setmac ) },
  { LNILKEY, LNILVAL }
};

const LUA_REG_TYPE wifi_map[] = 
{
  { LSTRKEY( "setmode" ), LFUNCVAL( wifi_setmode ) },
  { LSTRKEY( "getmode" ), LFUNCVAL( wifi_getmode ) },
  { LSTRKEY( "startsmart" ), LFUNCVAL( wifi_start_smart ) },
  { LSTRKEY( "stopsmart" ), LFUNCVAL( wifi_exit_smart ) },
  { LSTRKEY( "sleeptype" ), LFUNCVAL( wifi_sleeptype ) },
#if LUA_OPTIMIZE_MEMORY > 0
  { LSTRKEY( "sta" ), LROVAL( wifi_station_map ) },
  { LSTRKEY( "ap" ), LROVAL( wifi_ap_map ) },

  // { LSTRKEY( "NULLMODE" ), LNUMVAL( NULL_MODE ) },
  { LSTRKEY( "STATION" ), LNUMVAL( STATION_MODE ) },
  { LSTRKEY( "SOFTAP" ), LNUMVAL( SOFTAP_MODE ) },
  { LSTRKEY( "STATIONAP" ), LNUMVAL( STATIONAP_MODE ) },

  { LSTRKEY( "NONE_SLEEP" ), LNUMVAL( NONE_SLEEP_T ) },
  { LSTRKEY( "LIGHT_SLEEP" ), LNUMVAL( LIGHT_SLEEP_T ) },
  { LSTRKEY( "MODEM_SLEEP" ), LNUMVAL( MODEM_SLEEP_T ) },

  // { LSTRKEY( "STA_IDLE" ), LNUMVAL( STATION_IDLE ) },
  // { LSTRKEY( "STA_CONNECTING" ), LNUMVAL( STATION_CONNECTING ) },
  // { LSTRKEY( "STA_WRONGPWD" ), LNUMVAL( STATION_WRONG_PASSWORD ) },
  // { LSTRKEY( "STA_APNOTFOUND" ), LNUMVAL( STATION_NO_AP_FOUND ) },
  // { LSTRKEY( "STA_FAIL" ), LNUMVAL( STATION_CONNECT_FAIL ) },
  // { LSTRKEY( "STA_GOTIP" ), LNUMVAL( STATION_GOT_IP ) },

  { LSTRKEY( "__metatable" ), LROVAL( wifi_map ) },
#endif
  { LNILKEY, LNILVAL }
};

LUALIB_API int luaopen_wifi( lua_State *L )
{
#if LUA_OPTIMIZE_MEMORY > 0
  return 0;
#else // #if LUA_OPTIMIZE_MEMORY > 0
  luaL_register( L, AUXLIB_WIFI, wifi_map );

  // Set it as its own metatable
  lua_pushvalue( L, -1 );
  lua_setmetatable( L, -2 );

  // Module constants  
  // MOD_REG_NUMBER( L, "NULLMODE", NULL_MODE );
  MOD_REG_NUMBER( L, "STATION", STATION_MODE );
  MOD_REG_NUMBER( L, "SOFTAP", SOFTAP_MODE );  
  MOD_REG_NUMBER( L, "STATIONAP", STATIONAP_MODE );  

  MOD_REG_NUMBER( L, "NONE_SLEEP", NONE_SLEEP_T );
  MOD_REG_NUMBER( L, "LIGHT_SLEEP", LIGHT_SLEEP_T );  
  MOD_REG_NUMBER( L, "MODEM_SLEEP", MODEM_SLEEP_T );  

  // MOD_REG_NUMBER( L, "STA_IDLE", STATION_IDLE );
  // MOD_REG_NUMBER( L, "STA_CONNECTING", STATION_CONNECTING );  
  // MOD_REG_NUMBER( L, "STA_WRONGPWD", STATION_WRONG_PASSWORD );  
  // MOD_REG_NUMBER( L, "STA_APNOTFOUND", STATION_NO_AP_FOUND );
  // MOD_REG_NUMBER( L, "STA_FAIL", STATION_CONNECT_FAIL );  
  // MOD_REG_NUMBER( L, "STA_GOTIP", STATION_GOT_IP );  

  // Setup the new tables (station and ap) inside wifi
  lua_newtable( L );
  luaL_register( L, NULL, wifi_station_map );
  lua_setfield( L, -2, "sta" );

  lua_newtable( L );
  luaL_register( L, NULL, wifi_ap_map );
  lua_setfield( L, -2, "ap" );

  return 1;
#endif // #if LUA_OPTIMIZE_MEMORY > 0  
}
