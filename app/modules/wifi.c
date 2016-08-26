// Module for interfacing with WIFI

#include "module.h"
#include "lauxlib.h"
#include "platform.h"

#include "c_string.h"
#include "c_stdlib.h"
#include "ctype.h"

#include "c_types.h"
#include "user_interface.h"
#include "wifi_common.h"


#ifdef WIFI_SMART_ENABLE
#include "smart.h"
#include "smartconfig.h"

static int wifi_smart_succeed = LUA_NOREF;
#endif

static uint8 getap_output_format=0;

#define INVALID_MAC_STR "MAC:FF:FF:FF:FF:FF:FF"

//wifi.sleep variables
#define FPM_SLEEP_MAX_TIME 0xFFFFFFF
static bool FLAG_wifi_force_sleep_enabled=0;

#ifdef WIFI_SMART_ENABLE
static void wifi_smart_succeed_cb(sc_status status, void *pdata){
  NODE_DBG("wifi_smart_succeed_cb is called.\n");

  lua_State* L = lua_getstate();
  if (status == SC_STATUS_LINK_OVER)
  {
    smartconfig_stop();
    return;
  }

#if defined( NODE_SMART_OLDSTYLE )

  if (status != SC_STATUS_LINK || !pdata)
    return;
  if(wifi_smart_succeed == LUA_NOREF)
    return;

  lua_State* L = (lua_State *)arg;
  lua_rawgeti(L, LUA_REGISTRYINDEX, wifi_smart_succeed);
  lua_call(L, 0, 0);

#else

  if (status != SC_STATUS_LINK || !pdata)
    return;

  struct station_config *sta_conf = pdata;
  wifi_station_set_config(sta_conf);
  wifi_station_disconnect();
  wifi_station_connect();

  if(wifi_smart_succeed != LUA_NOREF)
  {
    lua_rawgeti(L, LUA_REGISTRYINDEX, wifi_smart_succeed);

    lua_pushstring(L, sta_conf->ssid);
    lua_pushstring(L, sta_conf->password);
    lua_call(L, 2, 0);

    unregister_lua_cb(L, &wifi_smart_succeed);
  }

#endif // defined( NODE_SMART_OLDSTYLE )
}
#endif // WIFI_SMART_ENABLE

static int wifi_scan_succeed = LUA_NOREF;
/**
  * @brief  Wifi ap scan over callback to display.
  * @param  arg: contain the aps information
  * @param  status: scan over status
  * @retval None
  */
static void wifi_scan_done(void *arg, STATUS status)
{
  lua_State* L = lua_getstate();
  uint8 ssid[33];
  char temp[sizeof("11:22:33:44:55:66")];
  if(wifi_scan_succeed == LUA_NOREF)
    return;
  if(arg == NULL)
    return;
  
  lua_rawgeti(L, LUA_REGISTRYINDEX, wifi_scan_succeed);

  if (status == OK)
  {
    struct bss_info *bss_link = (struct bss_info *)arg;
    lua_newtable( L );

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
      if(getap_output_format==1) //use new format(BSSID : SSID, RSSI, Authmode, Channel)
      {
    	c_sprintf(temp,MACSTR, MAC2STR(bss_link->bssid));
    	wifi_add_sprintf_field(L, temp, "%s,%d,%d,%d",
    			ssid, bss_link->rssi, bss_link->authmode, bss_link->channel);
        NODE_DBG(MACSTR" : %s\n",MAC2STR(bss_link->bssid) , temp);//00 00 00 00 00 00
      }
      else//use old format(SSID : Authmode, RSSI, BSSID, Channel)
      {
  	  wifi_add_sprintf_field(L, ssid, "%d,%d,"MACSTR",%d",
  			  bss_link->authmode, bss_link->rssi, MAC2STR(bss_link->bssid),bss_link->channel);
  	    NODE_DBG("%s : %s\n", ssid, temp);
      }

      bss_link = bss_link->next.stqe_next;
    }
  }
  else
  {
    lua_newtable( L );
  }
  lua_call(L, 1, 0);
    unregister_lua_cb(L, &wifi_scan_succeed);
}

#ifdef WIFI_SMART_ENABLE
// Lua: smart(channel, function succeed_cb)
// Lua: smart(type, function succeed_cb)
static int wifi_start_smart( lua_State* L )
{

#if defined( NODE_SMART_OLDSTYLE )

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

#else

  if(wifi_get_opmode() != STATION_MODE)
  {
    return luaL_error( L, "Smart link only in STATION mode" );
  }
  uint8_t smart_type = 0;
  int stack = 1;
  if ( lua_isnumber(L, stack) )
  {
    smart_type = lua_tointeger(L, stack);
    stack++;
  }

  if (lua_type(L, stack) == LUA_TFUNCTION || lua_type(L, stack) == LUA_TLIGHTFUNCTION)
  {
    lua_pushvalue(L, stack);  // copy argument (func) to the top of stack
    register_lua_cb(L, &wifi_smart_succeed);
  }

  if ( smart_type > 1 )
    return luaL_error( L, "wrong arg range" );

  smartconfig_set_type(smart_type);
  smartconfig_start(wifi_smart_succeed_cb);

#endif // defined( NODE_SMART_OLDSTYLE )

  return 0;  
}

// Lua: exit_smart()
static int wifi_exit_smart( lua_State* L )
{
#if defined( NODE_SMART_OLDSTYLE )
  smart_end();
#else
  smartconfig_stop();
#endif // defined( NODE_SMART_OLDSTYLE )

  unregister_lua_cb(L, &wifi_smart_succeed);
  return 0;  
}
#endif // WIFI_SMART_ENABLE

// Lua: realmode = setmode(mode)
static int wifi_setmode( lua_State* L )
{
  unsigned mode;
  
  mode = luaL_checkinteger( L, 1 );

  if ( mode != STATION_MODE && mode != SOFTAP_MODE && mode != STATIONAP_MODE && mode != NULL_MODE )
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
/**
  * wifi.getchannel()
  * Description:
  * 	Get current wifi Channel
  *
  * Syntax:
  * 	wifi.getchannel()
  * Parameters:
  * 	nil
  *
  * Returns:
  * 	Current wifi channel
  */

static int wifi_getchannel( lua_State* L )
{
  unsigned channel;
  channel = (unsigned)wifi_get_channel();
  lua_pushinteger( L, channel );
  return 1;
}

/**
  * wifi.setphymode()
  * Description:
  * 	Set wifi physical mode（802.11 b/g/n）
  * 	Note： SoftAP only supports 802.11 b/g.
  * Syntax:
  * 	wifi.setphymode(mode)
  * Parameters:
  * 	mode:
  * 		wifi.PHYMODE_B
  *	 		wifi.PHYMODE_G
  * 		wifi.PHYMODE_N
  * Returns:
  * 	Current physical mode after setup
  */

static int wifi_setphymode( lua_State* L )
{
  unsigned mode;

  mode = luaL_checkinteger( L, 1 );

  if ( mode != PHY_MODE_11B && mode != PHY_MODE_11G && mode != PHY_MODE_11N )
    return luaL_error( L, "wrong arg type" );
  wifi_set_phy_mode( (uint8_t)mode);
  mode = (unsigned)wifi_get_phy_mode();
  lua_pushinteger( L, mode );
  return 1;
}

/**
  * wifi.getphymode()
  * Description:
  * 	Get wifi physical mode（802.11 b/g/n）
  * Syntax:
  * 	wifi.getphymode()
  * Parameters:
  * 	nil
  * Returns:
  * 	Current physical mode.
  *
  */
static int wifi_getphymode( lua_State* L )
{
  unsigned mode;
  mode = (unsigned)wifi_get_phy_mode();
  lua_pushinteger( L, mode );
  return 1;
}

//wifi.sleep()
static int wifi_sleep(lua_State* L)
{
  uint8 desired_sleep_state = 2;
  sint8 wifi_fpm_do_sleep_return_value = 1;
  if(lua_isnumber(L, 1))
  {
	  if(luaL_checknumber(L, 1) == 0)
	  {
		  desired_sleep_state = 0;
	  }
	  else if(luaL_checknumber(L, 1) == 1)
	  {
		  desired_sleep_state = 1;
	  }
  }
  if (!FLAG_wifi_force_sleep_enabled && desired_sleep_state == 1 )
  {
	uint8 wifi_current_opmode = wifi_get_opmode();
	if (wifi_current_opmode == 1 || wifi_current_opmode == 3 )
	{
	  wifi_station_disconnect();
	}
	// set WiFi mode to null mode
	wifi_set_opmode(NULL_MODE);
	// set force sleep type
	wifi_fpm_set_sleep_type(MODEM_SLEEP_T);
	wifi_fpm_open();
	wifi_fpm_do_sleep_return_value = wifi_fpm_do_sleep(FPM_SLEEP_MAX_TIME);
	if (wifi_fpm_do_sleep_return_value == 0)
	{
	  FLAG_wifi_force_sleep_enabled = TRUE;
	}
	else
	{
		wifi_fpm_close();
		FLAG_wifi_force_sleep_enabled = FALSE;
	}

  }
  else if(FLAG_wifi_force_sleep_enabled && desired_sleep_state == 0)
  {
	FLAG_wifi_force_sleep_enabled = FALSE;
	// wake up to use WiFi again
	wifi_fpm_do_wakeup();
	wifi_fpm_close();
  }

  if (desired_sleep_state == 1 && FLAG_wifi_force_sleep_enabled == FALSE)
  {
	  lua_pushnil(L);
	  lua_pushnumber(L, wifi_fpm_do_sleep_return_value);
  }
  else
  {
	  lua_pushnumber(L, FLAG_wifi_force_sleep_enabled);
	  lua_pushnil(L);
  }
  return 2;
}

//wifi.nullmodesleep()
static int wifi_null_mode_auto_sleep(lua_State* L)
{
  if (!lua_isnone(L, 1))
  {
    bool auto_sleep_setting=lua_toboolean(L, 1);
    if (auto_sleep_setting!=(bool) get_fpm_auto_sleep_flag())
    {
      wifi_fpm_auto_sleep_set_in_null_mode((uint8)auto_sleep_setting);
      //if esp is already in NULL_MODE, auto sleep setting won't take effect until next wifi_set_opmode(NULL_MODE) call.
      if(wifi_get_opmode()==NULL_MODE)
      {
        wifi_set_opmode_current(NULL_MODE);
      }
    }
  }
  lua_pushboolean(L, (bool) get_fpm_auto_sleep_flag());
  return 1;
}

// Lua: mac = wifi.xx.getmac()
static int wifi_getmac( lua_State* L, uint8_t mode )
{
  char temp[64];
  uint8_t mac[6];
  wifi_get_macaddr(mode, mac);
  c_sprintf(temp, MACSTR, MAC2STR(mac));
  lua_pushstring( L, temp );
  return 1;  
}

// Lua: mac = wifi.xx.setmac()
static int wifi_setmac( lua_State* L, uint8_t mode )
{
  uint8_t mac[6];
  unsigned len = 0;
  const char *macaddr = luaL_checklstring( L, 1, &len );
  luaL_argcheck(L, len==17, 1, INVALID_MAC_STR);
  ets_str2macaddr(mac, macaddr);
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
  if(mode==SOFTAP_IF || ip!=0)
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

/**
  * wifi.sta.getconfig()
  * Description:
  * 	Get current Station configuration.
  * 	Note:  if bssid_set==1 STATION is configured to connect to specified BSSID
  * 		   if bssid_set==0 specified BSSID address is irrelevant.
  * Syntax:
  * 	ssid, pwd, bssid_set, bssid=wifi.sta.getconfig()
  * Parameters:
  * 	none
  * Returns:
  * 	SSID, Password, BSSID_set, BSSID
  */
static int wifi_station_getconfig( lua_State* L )
{
	struct station_config sta_conf;
	char bssid[17];
	wifi_station_get_config(&sta_conf);
	if(sta_conf.ssid==0)
	{
		lua_pushnil(L);
	    return 1;
	}
	else
	{
	    lua_pushstring( L, sta_conf.ssid );
	    lua_pushstring( L, sta_conf.password );
	    lua_pushinteger( L, sta_conf.bssid_set);
	    c_sprintf(bssid, MACSTR, MAC2STR(sta_conf.bssid));
	    lua_pushstring( L, bssid);
	    return 4;
	}
}

/**
  * wifi.sta.config()
  * Description:
  * 	Set current Station configuration.
  * 	Note: If there are multiple APs with the same ssid, you can connect to a specific one by entering it's MAC address into the "bssid" field.
  * Syntax:
  * 	wifi.sta.getconfig(ssid, password) --Set STATION configuration, Auto-connect by default, Connects to any BSSID
  * 	wifi.sta.getconfig(ssid, password, Auto_connect) --Set STATION configuration, Auto-connect(0 or 1), Connects to any BSSID
  * 	wifi.sta.getconfig(ssid, password, bssid) --Set STATION configuration, Auto-connect by default, Connects to specific BSSID
  * 	wifi.sta.getconfig(ssid, password, Auto_connect, bssid) --Set STATION configuration, Auto-connect(0 or 1), Connects to specific BSSID
  * Parameters:
  * 	ssid: string which is less than 32 bytes.
  * 	Password: string which is less than 64 bytes.
  * 	Auto_connect: 0 (disable Auto-connect) or 1 (to enable Auto-connect).
  * 	bssid: MAC address of Access Point you would like to connect to.
  * Returns:
  * 	Nothing.
  *
  *	Example:
  	  	--Connect to Access Point automatically when in range
  	  	wifi.sta.getconfig("myssid", "password")

  	  	--Connect to Access Point, User decides when to connect/disconnect to/from AP
  	  	wifi.sta.getconfig("myssid", "mypassword", 0)
  	  	wifi.sta.connect()
  	  	--do some wifi stuff
  	  	wifi.sta.disconnect()

  	  	--Connect to specific Access Point automatically when in range
  	  	wifi.sta.getconfig("myssid", "mypassword", "12:34:56:78:90:12")

  	  	--Connect to specific Access Point, User decides when to connect/disconnect to/from AP
  	  	wifi.sta.getconfig("myssid", "mypassword", 0)
  	  	wifi.sta.connect()
  	  	--do some wifi stuff
  	  	wifi.sta.disconnect()
  *
  */
static int wifi_station_config( lua_State* L )
{
  size_t sl, pl, ml;
  struct station_config sta_conf;
  int auto_connect=0;
  const char *ssid = luaL_checklstring( L, 1, &sl );
  if (sl>32 || ssid == NULL)
    return luaL_error( L, "ssid:<32" );
  const char *password = luaL_checklstring( L, 2, &pl );
  if ((pl!=0 && (pl<8 || pl>64)) || password == NULL)
    return luaL_error( L, "pwd:0,8~64" );

  if(lua_isnumber(L, 3))
  {
    auto_connect=luaL_checkinteger( L, 3 );;
    if ( auto_connect != 0 && auto_connect != 1)
    	return luaL_error( L, "wrong arg type" );
  }
  else if (lua_isstring(L, 3)&& !(lua_isnumber(L, 3)))
  {
	  lua_pushnil(L);
	  lua_insert(L, 3);
	  auto_connect=1;

  }
  else
  {
    if(lua_isnil(L, 3))
	   	return luaL_error( L, "wrong arg type" );
	auto_connect=1;
  }

  if(lua_isnumber(L, 4))
  {
    sta_conf.bssid_set = 0;
	c_memset(sta_conf.bssid, 0, 6);
  }
  else
  {
    if (lua_isstring(L, 4))
	{
	  const char *macaddr = luaL_checklstring( L, 4, &ml );
	  luaL_argcheck(L, ml==17, 1, INVALID_MAC_STR);
	  c_memset(sta_conf.bssid, 0, 6);
	  ets_str2macaddr(sta_conf.bssid, macaddr);
	  sta_conf.bssid_set = 1;
	}
	else
	{
	  sta_conf.bssid_set = 0;
	  c_memset(sta_conf.bssid, 0, 6);
	}
  }

  c_memset(sta_conf.ssid, 0, 32);
  c_memset(sta_conf.password, 0, 64);
  c_memcpy(sta_conf.ssid, ssid, sl);
  c_memcpy(sta_conf.password, password, pl);

  NODE_DBG(sta_conf.ssid);
  NODE_DBG(" %d\n", sl);
  NODE_DBG(sta_conf.password);
  NODE_DBG(" %d\n", pl);
  NODE_DBG(" %d\n", sta_conf.bssid_set);
  NODE_DBG( MACSTR, MAC2STR(sta_conf.bssid));
  NODE_DBG("\n");


  wifi_station_disconnect();
  wifi_station_set_config(&sta_conf);

  if(auto_connect==0)
  {
    wifi_station_set_auto_connect(false);

  }
  else
  {
    wifi_station_set_auto_connect(true);
    wifi_station_connect();
  }
//  station_check_connect(0);
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
  luaL_argcheck(L, ( a == 0 || a == 1 ), 1, "0 or 1");
  wifi_station_set_auto_connect(a);
  return 0;  
}

/**
  * wifi.sta.listap()
  * Description:
  * 	scan and get ap list as a lua table into callback function.
  * Syntax:
  * 	wifi.sta.getap(function(table))
  * 	wifi.sta.getap(format, function(table))
  * 	wifi.sta.getap(cfg, function(table))
  * 	wifi.sta.getap(cfg, format, function(table))
  * Parameters:
  * 	cfg: table that contains scan configuration
  * 	Format:Select output table format.
  * 		0 for the old format (SSID : Authmode, RSSI, BSSID, Channel) (Default)
  * 		1 for the new format (BSSID : SSID, RSSI, Authmode, Channel)
  * 	function(table): a callback function to receive ap table when scan is done
			this function receive a table, the key is the ssid,
			value is other info in format: authmode,rssi,bssid,channel
  * Returns:
  * 	nil
  *
  * Example:
  	  --original function left intact to preserve backward compatibility
  	  wifi.sta.getap(function(T) for k,v in pairs(T) do print(k..":"..v) end end)

  	  --if no scan configuration is desired cfg can be set to nil or previous example can be used
  	  wifi.sta.getap(nil, function(T) for k,v in pairs(T) do print(k..":"..v) end end)

  	  --scan configuration
  	  scan_cfg={}
	  scan_cfg.ssid="myssid"  			 --if set to nil, ssid is not filtered
	  scan_cfg.bssid="AA:AA:AA:AA:AA:AA" --if set to nil, MAC address is not filtered
	  scan_cfg.channel=0  				 --if set to nil, channel will default to 0(scans all channels), if set scan will be faster
	  scan_cfg.show_hidden=1			 --if set to nil, show_hidden will default to 0
  	  wifi.sta.getap(scan_cfg, function(T) for k,v in pairs(T) do print(k..":"..v) end end)

  */
static int wifi_station_listap( lua_State* L )
{
  if(wifi_get_opmode() == SOFTAP_MODE)
  {
    return luaL_error( L, "Can't list ap in SOFTAP mode" );
  }
  struct scan_config scan_cfg;
  getap_output_format=0;

  if (lua_type(L, 1)==LUA_TTABLE)
  {
	  char ssid[32];
	  char bssid[6];
	  uint8 channel=0;
	  uint8 show_hidden=0;
	  size_t len;

	  lua_getfield(L, 1, "ssid");
	  if (!lua_isnil(L, -1)){  /* found? */
	    if( lua_isstring(L, -1) )   // deal with the ssid string
	    {
	      const char *ssidstr = luaL_checklstring( L, -1, &len );
	      if(len>32)
	        return luaL_error( L, "ssid:<32" );
	      c_memset(ssid, 0, 32);
	      c_memcpy(ssid, ssidstr, len);
	      scan_cfg.ssid=ssid;
	      NODE_DBG(scan_cfg.ssid);
	      NODE_DBG("\n");
	    }
	    else
	      return luaL_error( L, "wrong arg type" );
	  }
	  else
		  scan_cfg.ssid=NULL;

	  lua_getfield(L, 1, "bssid");
	  if (!lua_isnil(L, -1)){  /* found? */
	    if( lua_isstring(L, -1) )   // deal with the ssid string
	    {
	      const char *macaddr = luaL_checklstring( L, -1, &len );
	      luaL_argcheck(L, len==17, 1, INVALID_MAC_STR);
	      c_memset(bssid, 0, 6);
	      ets_str2macaddr(bssid, macaddr);
	      scan_cfg.bssid=bssid;
	      NODE_DBG(MACSTR, MAC2STR(scan_cfg.bssid));
	      NODE_DBG("\n");

	    }
	    else
	      return luaL_error( L, "wrong arg type" );
	  }
	  else
		  scan_cfg.bssid=NULL;


	  lua_getfield(L, 1, "channel");
	  if (!lua_isnil(L, -1)){  /* found? */
	    if( lua_isnumber(L, -1) )   // deal with the ssid string
	    {
	      channel = luaL_checknumber( L, -1);
	      if(!(channel>=0 && channel<=13))
	        return luaL_error( L, "channel: 0 or 1-13" );
	      scan_cfg.channel=channel;
	      NODE_DBG("%d\n", scan_cfg.channel);
	    }
	    else
	      return luaL_error( L, "wrong arg type" );
	  }
	  else
		  scan_cfg.channel=0;

	  lua_getfield(L, 1, "show_hidden");
	  if (!lua_isnil(L, -1)){  /* found? */
	    if( lua_isnumber(L, -1) )   // deal with the ssid string
	    {
	      show_hidden = luaL_checknumber( L, -1);
	      if(show_hidden!=0 && show_hidden!=1)
	        return luaL_error( L, "show_hidden: 0 or 1" );
	      scan_cfg.show_hidden=show_hidden;
	      NODE_DBG("%d\n", scan_cfg.show_hidden);

	    }
	    else
	      return luaL_error( L, "wrong arg type" );
	  }
	  else
		  scan_cfg.show_hidden=0;
	  if (lua_type(L, 2) == LUA_TFUNCTION || lua_type(L, 2) == LUA_TLIGHTFUNCTION)
	  {
		  lua_pushnil(L);
		  lua_insert(L, 2);
	  }
	  lua_pop(L, -4);
  }
  else  if (lua_type(L, 1) == LUA_TNUMBER)
  {
	  lua_pushnil(L);
	  lua_insert(L, 1);
  }
  else  if (lua_type(L, 1) == LUA_TFUNCTION || lua_type(L, 1) == LUA_TLIGHTFUNCTION)
  {
	  lua_pushnil(L);
	  lua_insert(L, 1);
	  lua_pushnil(L);
	  lua_insert(L, 1);
  }
  else if(lua_isnil(L, 1))
  {
	  if (lua_type(L, 2) == LUA_TFUNCTION || lua_type(L, 2) == LUA_TLIGHTFUNCTION)
	  {
		  lua_pushnil(L);
		  lua_insert(L, 2);
	  }
  }
  else
  {
	  return luaL_error( L, "wrong arg type" );
  }


  if (lua_type(L, 2) == LUA_TNUMBER) //this section changes the output format
    {
	  getap_output_format=luaL_checkinteger( L, 2 );
	  if ( getap_output_format != 0 && getap_output_format != 1)
		  return luaL_error( L, "wrong arg type" );
    }
  NODE_DBG("Use alternate output format: %d\n", getap_output_format);
  if (lua_type(L, 3) == LUA_TFUNCTION || lua_type(L, 3) == LUA_TLIGHTFUNCTION)
  {
    lua_pushvalue(L, 3);  // copy argument (func) to the top of stack
    register_lua_cb(L, &wifi_scan_succeed);

    if (lua_type(L, 1)==LUA_TTABLE)
    {
    	wifi_station_scan(&scan_cfg,wifi_scan_done);
    }
    else
    {
    	wifi_station_scan(NULL,wifi_scan_done);
    }
  }
  else
  {
	unregister_lua_cb(L, &wifi_scan_succeed);
  }
}

// Lua: wifi.sta.gethostname()
static int wifi_sta_gethostname( lua_State* L )
{
  char* hostname = wifi_station_get_hostname();
  lua_pushstring(L, hostname);
  return 1;
}

static bool wifi_sta_sethostname(const char *hostname, size_t len)
{
  //this function follows RFC 952 & RFC 1123 host name standards.
  //the hostname must be 32 chars or less and first and last char must be alphanumeric
  if (!isalnum(hostname[0]) || !isalnum(hostname[len-1]) || len > 32)
  {
    return false;
  }

  for (int i=1; i<len; i++)
  {
    //characters in the middle of the host name can be alphanumeric or a hyphen(-) only
	if (!(isalnum(hostname[i]) || hostname[i]=='-'))
    {
    	return false;
    }
  }
  return wifi_station_set_hostname((char*)hostname);
}

static int wifi_sta_sethostname_lua( lua_State* L )
{
  size_t len;
  const char *hostname = luaL_checklstring(L, 1, &len);
  luaL_argcheck(L, wifi_sta_sethostname(hostname, len), 1, "Invalid hostname");
  return 0;
}

// Lua: wifi.sta.status()
static int wifi_station_status( lua_State* L )
{
  uint8_t status = wifi_station_get_connect_status();
  lua_pushinteger( L, status );
  return 1; 
}

// Lua: wifi.sta.getrssi()
static int wifi_station_getrssi( lua_State* L ){
  sint8 rssival=wifi_station_get_rssi();
  NODE_DBG("\n\tRSSI is %i\n", rssival);
  if (rssival<10)
  {
    lua_pushinteger(L, rssival);
  }
  else
  {
    lua_pushnil(L);
  }
	return 1;
}

//Lua: wifi.ap.deauth()
static int wifi_ap_deauth( lua_State* L )
{
  uint8_t mac[6];
  unsigned len = 0;
  if(lua_isstring(L, 1))
  {
    const char *macaddr = luaL_checklstring( L, 1, &len );
    luaL_argcheck(L, len==17, 1, INVALID_MAC_STR);
    ets_str2macaddr(mac, macaddr);
  }
  else
  {
	  c_memset(&mac, 0xFF, sizeof(mac));
  }
  lua_pushboolean(L,wifi_softap_deauth(mac));
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

// Lua: wifi.ap.getconfig()
static int wifi_ap_getconfig( lua_State* L )
{
  struct softap_config config;
  wifi_softap_get_config(&config);
  lua_pushstring( L, config.ssid );
  if(config.authmode == AUTH_OPEN)
    lua_pushnil(L);
  else
    lua_pushstring( L, config.password );
  return 2;
}

// Lua: wifi.ap.config(table)
static int wifi_ap_config( lua_State* L )
{
  if (!lua_istable(L, 1))
    return luaL_error( L, "wrong arg type" );

  struct softap_config config;
  wifi_softap_get_config(&config);

  size_t len;

  lua_getfield(L, 1, "ssid");
  if (!lua_isnil(L, -1)){  /* found? */
    if( lua_isstring(L, -1) )   // deal with the ssid string
    {
      const char *ssid = luaL_checklstring( L, -1, &len );
      if(len<1 || len>32 || ssid == NULL)
        return luaL_error( L, "ssid:1~32" );
      c_memset(config.ssid, 0, 32);
      c_memcpy(config.ssid, ssid, len);
      NODE_DBG(config.ssid);
      NODE_DBG("\n");
      config.ssid_len = len;
      config.ssid_hidden = 0;
    } 
    else
      return luaL_error( L, "wrong arg type" );
  }
  else
    return luaL_error( L, "ssid required" );

  lua_getfield(L, 1, "pwd");
  if (!lua_isnil(L, -1)){  /* found? */
    if( lua_isstring(L, -1) )   // deal with the password string
    {
      const char *pwd = luaL_checklstring( L, -1, &len );
      if(len<8 || len>64 || pwd == NULL)
        return luaL_error( L, "pwd:8~64" );
      c_memset(config.password, 0, 64);
      c_memcpy(config.password, pwd, len);
      NODE_DBG(config.password);
      NODE_DBG("\n");
      config.authmode = AUTH_WPA_WPA2_PSK;
    }
    else
      return luaL_error( L, "wrong arg type" );
  }
  else{
    config.authmode = AUTH_OPEN;
  }

  lua_getfield(L, 1, "auth");
  if (!lua_isnil(L, -1))
  {
    config.authmode = (uint8_t)luaL_checkinteger(L, -1);
    NODE_DBG("%d\n", config.authmode);
  }
  else
  {
    // keep whatever value resulted from "pwd" logic above
  }

  lua_getfield(L, 1, "channel");
  if (!lua_isnil(L, -1))
  {
    unsigned channel = luaL_checkinteger(L, -1);
    if (channel < 1 || channel > 13)
      return luaL_error( L, "channel:1~13" );

    config.channel = (uint8_t)channel;
    NODE_DBG("%d\n", config.channel);
  }
  else
  {
    config.channel = 6;
  }

  lua_getfield(L, 1, "hidden");
  if (!lua_isnil(L, -1))
  {
    config.ssid_hidden = (uint8_t)luaL_checkinteger(L, -1);
    NODE_DBG("%d\n", config.ssid_hidden);
    NODE_DBG("\n");
  }
  else
  {
    config.ssid_hidden = 0;
  }

  lua_getfield(L, 1, "max");
  if (!lua_isnil(L, -1))
  {
    unsigned max = luaL_checkinteger(L, -1);
    if (max < 1 || max > 4)
      return luaL_error( L, "max:1~4" );

    config.max_connection = (uint8_t)max;
    NODE_DBG("%d\n", config.max_connection);
  }
  else
  {
    config.max_connection = 4;
  }

  lua_getfield(L, 1, "beacon");
  if (!lua_isnil(L, -1))
  {
    unsigned beacon = luaL_checkinteger(L, -1);
    if (beacon < 100 || beacon > 60000)
      return luaL_error( L, "beacon:100~60000" );

    config.beacon_interval = (uint16_t)beacon;
    NODE_DBG("%d\n", config.beacon_interval);
  }
  else
  {
    config.beacon_interval = 100;
  }

  wifi_softap_set_config(&config);
  // system_restart();
  return 0;
}

// Lua: table = wifi.ap.getclient()
static int wifi_ap_listclient( lua_State* L )
{
  if (wifi_get_opmode() == STATION_MODE)
  {
    return luaL_error( L, "Can't list client in STATION_MODE mode" );
  }

  char temp[64];

  lua_newtable(L);

  struct station_info * station = wifi_softap_get_station_info();
  struct station_info * next_station;
  while (station != NULL)
  {
    c_sprintf(temp, MACSTR, MAC2STR(station->bssid));
    wifi_add_sprintf_field(L, temp, IPSTR, IP2STR(&station->ip));
    next_station = STAILQ_NEXT(station, next);
    c_free(station);
    station = next_station;
  }

  return 1;
}

// Lua: ip = wifi.ap.dhcp.config()
static int wifi_ap_dhcp_config( lua_State* L )
{
  if (!lua_istable(L, 1))
    return luaL_error( L, "wrong arg type" );

  struct dhcps_lease lease;
  uint32_t ip;

  ip = parse_key(L, "start");
  if (ip == 0)
    return luaL_error( L, "wrong arg type" );

  lease.start_ip.addr = ip;
  NODE_DBG(IPSTR, IP2STR(&lease.start_ip));
  NODE_DBG("\n");

  // use configured max_connection to determine end
  struct softap_config config;
  wifi_softap_get_config(&config);
  lease.end_ip = lease.start_ip;
  ip4_addr4(&lease.end_ip) += config.max_connection - 1;

  char temp[64];
  c_sprintf(temp, IPSTR, IP2STR(&lease.start_ip));
  lua_pushstring(L, temp);
  c_sprintf(temp, IPSTR, IP2STR(&lease.end_ip));
  lua_pushstring(L, temp);

  // note: DHCP max range = 101 from start_ip to end_ip
  wifi_softap_dhcps_stop();
  wifi_softap_set_dhcps_lease(&lease);
  wifi_softap_dhcps_start();

  return 2;
}

// Lua: wifi.ap.dhcp.start()
static int wifi_ap_dhcp_start( lua_State* L )
{
  lua_pushboolean(L, wifi_softap_dhcps_start());
  return 1;
}

// Lua: wifi.ap.dhcp.stop()
static int wifi_ap_dhcp_stop( lua_State* L )
{
  lua_pushboolean(L, wifi_softap_dhcps_stop());
  return 1;
}

// Module function map
static const LUA_REG_TYPE wifi_station_map[] = {
  { LSTRKEY( "getconfig" ),     LFUNCVAL( wifi_station_getconfig ) },
  { LSTRKEY( "config" ),        LFUNCVAL( wifi_station_config ) },
  { LSTRKEY( "connect" ),       LFUNCVAL( wifi_station_connect4lua ) },
  { LSTRKEY( "disconnect" ),    LFUNCVAL( wifi_station_disconnect4lua ) },
  { LSTRKEY( "autoconnect" ),   LFUNCVAL( wifi_station_setauto ) },
  { LSTRKEY( "getip" ),         LFUNCVAL( wifi_station_getip ) },
  { LSTRKEY( "setip" ),         LFUNCVAL( wifi_station_setip ) },
  { LSTRKEY( "getbroadcast" ),  LFUNCVAL( wifi_station_getbroadcast) },
  { LSTRKEY( "getmac" ),        LFUNCVAL( wifi_station_getmac ) },
  { LSTRKEY( "setmac" ),        LFUNCVAL( wifi_station_setmac ) },
  { LSTRKEY( "getap" ),         LFUNCVAL( wifi_station_listap ) },
  { LSTRKEY( "sethostname" ),   LFUNCVAL( wifi_sta_sethostname_lua ) },
  { LSTRKEY( "gethostname" ),   LFUNCVAL( wifi_sta_gethostname ) },
  { LSTRKEY( "getrssi" ),       LFUNCVAL( wifi_station_getrssi ) },
  { LSTRKEY( "status" ),        LFUNCVAL( wifi_station_status ) },
#if defined(WIFI_STATION_STATUS_MONITOR_ENABLE)
  { LSTRKEY( "eventMonReg" ),   LFUNCVAL( wifi_station_event_mon_reg ) }, //declared in wifi_eventmon.c
  { LSTRKEY( "eventMonStart" ), LFUNCVAL( wifi_station_event_mon_start ) }, //declared in wifi_eventmon.c
  { LSTRKEY( "eventMonStop" ),  LFUNCVAL( wifi_station_event_mon_stop ) }, //declared in wifi_eventmon.c
#endif
  { LNILKEY, LNILVAL }
};

static const LUA_REG_TYPE wifi_ap_dhcp_map[] = {
  { LSTRKEY( "config" ),  LFUNCVAL( wifi_ap_dhcp_config ) },
  { LSTRKEY( "start" ),   LFUNCVAL( wifi_ap_dhcp_start ) },
  { LSTRKEY( "stop" ),    LFUNCVAL( wifi_ap_dhcp_stop ) },
  { LNILKEY, LNILVAL }
};

static const LUA_REG_TYPE wifi_ap_map[] = {
  { LSTRKEY( "config" ),       LFUNCVAL( wifi_ap_config ) },
  { LSTRKEY( "deauth" ),       LFUNCVAL( wifi_ap_deauth ) },
  { LSTRKEY( "getip" ),        LFUNCVAL( wifi_ap_getip ) },
  { LSTRKEY( "setip" ),        LFUNCVAL( wifi_ap_setip ) },
  { LSTRKEY( "getbroadcast" ), LFUNCVAL( wifi_ap_getbroadcast) },
  { LSTRKEY( "getmac" ),       LFUNCVAL( wifi_ap_getmac ) },
  { LSTRKEY( "setmac" ),       LFUNCVAL( wifi_ap_setmac ) },
  { LSTRKEY( "getclient" ),    LFUNCVAL( wifi_ap_listclient ) },
  { LSTRKEY( "getconfig" ),    LFUNCVAL( wifi_ap_getconfig ) },
  { LSTRKEY( "dhcp" ),         LROVAL( wifi_ap_dhcp_map ) },
//{ LSTRKEY( "__metatable" ),  LROVAL( wifi_ap_map ) },
  { LNILKEY, LNILVAL }
};

static const LUA_REG_TYPE wifi_map[] =  {
  { LSTRKEY( "setmode" ),        LFUNCVAL( wifi_setmode ) },
  { LSTRKEY( "getmode" ),        LFUNCVAL( wifi_getmode ) },
  { LSTRKEY( "getchannel" ),     LFUNCVAL( wifi_getchannel ) },
  { LSTRKEY( "setphymode" ),     LFUNCVAL( wifi_setphymode ) },
  { LSTRKEY( "getphymode" ),     LFUNCVAL( wifi_getphymode ) },
  { LSTRKEY( "sleep" ),          LFUNCVAL( wifi_sleep ) },
  { LSTRKEY( "nullmodesleep" ),  LFUNCVAL( wifi_null_mode_auto_sleep ) },
#ifdef WIFI_SMART_ENABLE 
  { LSTRKEY( "startsmart" ),     LFUNCVAL( wifi_start_smart ) },
  { LSTRKEY( "stopsmart" ),      LFUNCVAL( wifi_exit_smart ) },
#endif
  { LSTRKEY( "sleeptype" ),      LFUNCVAL( wifi_sleeptype ) },

  { LSTRKEY( "sta" ),            LROVAL( wifi_station_map ) },
  { LSTRKEY( "ap" ),             LROVAL( wifi_ap_map ) },
#if defined(WIFI_SDK_EVENT_MONITOR_ENABLE)
  { LSTRKEY( "eventmon" ),       LROVAL( wifi_event_monitor_map ) }, //declared in wifi_eventmon.c
#endif
  { LSTRKEY( "NULLMODE" ),       LNUMVAL( NULL_MODE ) },
  { LSTRKEY( "STATION" ),        LNUMVAL( STATION_MODE ) },
  { LSTRKEY( "SOFTAP" ),         LNUMVAL( SOFTAP_MODE ) },
  { LSTRKEY( "STATIONAP" ),      LNUMVAL( STATIONAP_MODE ) },

  { LSTRKEY( "PHYMODE_B" ),      LNUMVAL( PHY_MODE_11B ) },
  { LSTRKEY( "PHYMODE_G" ),      LNUMVAL( PHY_MODE_11G ) },
  { LSTRKEY( "PHYMODE_N" ),      LNUMVAL( PHY_MODE_11N ) },

  { LSTRKEY( "NONE_SLEEP" ),     LNUMVAL( NONE_SLEEP_T ) },
  { LSTRKEY( "LIGHT_SLEEP" ),    LNUMVAL( LIGHT_SLEEP_T ) },
  { LSTRKEY( "MODEM_SLEEP" ),    LNUMVAL( MODEM_SLEEP_T ) },

  { LSTRKEY( "OPEN" ),           LNUMVAL( AUTH_OPEN ) },
//{ LSTRKEY( "WEP" ),            LNUMVAL( AUTH_WEP ) },
  { LSTRKEY( "WPA_PSK" ),        LNUMVAL( AUTH_WPA_PSK ) },
  { LSTRKEY( "WPA2_PSK" ),       LNUMVAL( AUTH_WPA2_PSK ) },
  { LSTRKEY( "WPA_WPA2_PSK" ),   LNUMVAL( AUTH_WPA_WPA2_PSK ) },

  { LSTRKEY( "STA_IDLE" ),       LNUMVAL( STATION_IDLE ) },
  { LSTRKEY( "STA_CONNECTING" ), LNUMVAL( STATION_CONNECTING ) },
  { LSTRKEY( "STA_WRONGPWD" ),   LNUMVAL( STATION_WRONG_PASSWORD ) },
  { LSTRKEY( "STA_APNOTFOUND" ), LNUMVAL( STATION_NO_AP_FOUND ) },
  { LSTRKEY( "STA_FAIL" ),       LNUMVAL( STATION_CONNECT_FAIL ) },
  { LSTRKEY( "STA_GOTIP" ),      LNUMVAL( STATION_GOT_IP ) },

  { LSTRKEY( "__metatable" ),    LROVAL( wifi_map ) },
  { LNILKEY, LNILVAL }
};

void wifi_change_default_host_name(void)
{
  uint8 opmode_temp=wifi_get_opmode();
  wifi_set_opmode_current(STATION_MODE);
#ifndef WIFI_STA_HOSTNAME
  char temp[32];
  uint8_t mac[6];
  wifi_get_macaddr(STATION_IF, mac);
  c_sprintf(temp, "NODE-%X%X%X", (mac)[3], (mac)[4], (mac)[5]);
  wifi_sta_sethostname((const char*)temp, strlen(temp));

#elif defined(WIFI_STA_HOSTNAME) && !defined(WIFI_STA_HOSTNAME_APPEND_MAC)
  if(!wifi_sta_sethostname(WIFI_STA_HOSTNAME, strlen(WIFI_STA_HOSTNAME)))
  {
    char temp[32];
    uint8_t mac[6];
    wifi_get_macaddr(STATION_IF, mac);
    c_sprintf(temp, "NODE-%X%X%X", (mac)[3], (mac)[4], (mac)[5]);
    wifi_sta_sethostname((const char*)temp, strlen(temp));
  }

#elif defined(WIFI_STA_HOSTNAME) && defined(WIFI_STA_HOSTNAME_APPEND_MAC)
  char temp[32];
  uint8_t mac[6];
  wifi_get_macaddr(STATION_IF, mac);
  c_sprintf(temp, "%s%X%X%X", WIFI_STA_HOSTNAME, (mac)[3], (mac)[4], (mac)[5]);
  if(!wifi_sta_sethostname(temp, strlen(temp)))
  {
    c_sprintf(temp, "NODE-%X%X%X", (mac)[3], (mac)[4], (mac)[5]);
    wifi_sta_sethostname((const char*)temp, strlen(temp));
  }
#endif
  if(opmode_temp!=wifi_get_opmode())
  {
    wifi_set_opmode_current(opmode_temp);
  }
}

int luaopen_wifi( lua_State *L )
{
  wifi_fpm_auto_sleep_set_in_null_mode(1);
  //if esp is already in NULL_MODE, auto sleep setting won't take effect until next wifi_set_opmode(NULL_MODE) call.
  if(wifi_get_opmode()==NULL_MODE)
  {
    wifi_set_opmode_current(NULL_MODE);
  }
#if defined(WIFI_SDK_EVENT_MONITOR_ENABLE)
  wifi_eventmon_init();
#endif
 return 0;
}

NODEMCU_MODULE(WIFI, "wifi", wifi_map, luaopen_wifi);
