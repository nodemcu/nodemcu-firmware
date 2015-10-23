/*
 * mdns.c
 *	Description: lua mDNS interface exposing the mDNS API of the SDK
 *  Created on: Aug 21, 2015
 *  Author: Michael Lucas (Aeprox @github)
 *

The MIT License (MIT)

Copyright (c) 2014 zeroday nodemcu.com

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

 */

#include "lua.h"
#include "lauxlib.h"
//#include "ets_sys.h"
//#include "osapi.h"
#include "../../sdk/esp_iot_sdk_v1.4.0/include/mem.h"
#include "user_interface.h"
#include "espconn.h"


/******************************************************************************
 * FunctionName : espconn_mdns_init
 * Description  : register a device with mdns
 * Parameters   : ipAddr -- the ip address of device
 * 				  hostname -- the hostname of device
 * Returns      : none
*******************************************************************************/
static int ICACHE_FLASH_ATTR lmdns_init(lua_State* L) {
	struct mdns_info *info = (struct mdns_info *)os_zalloc(sizeof(struct
	mdns_info));

	// fetch mdnsinfo from lua state
	const char* host_name = luaL_checkstring(L, 1);
	const char* server_name = luaL_checkstring(L,2);
	uint16 server_port = luaL_checkinteger(L,3);
	unsigned long ipAddr = luaL_checkinteger(L,4);
	//char* txt_data[10] = luaL_checkanytable(L,5); // need to parse table

	espconn_mdns_init(info);
	return 0;
}

/******************************************************************************
 * FunctionName : mdns_close
 * Description  : close mdns socket
 * Parameters   : void
 * Returns      : none
*******************************************************************************/
static int ICACHE_FLASH_ATTR lmdns_close(lua_State* L) {
	espconn_mdns_close();
	return 0;
}

/******************************************************************************
 * FunctionName : mdns_server_register
 * Description  : register a server and join a multicast group
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
static int ICACHE_FLASH_ATTR lmdns_server_register(lua_State* L) {

	espconn_mdns_server_register();
	return 0;
}

/******************************************************************************
 * FunctionName : mdns_server_unregister
 * Description  : unregister server and leave multicast group
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
static int ICACHE_FLASH_ATTR lmdns_server_unregister(lua_State* L) {
	espconn_mdns_server_unregister();
	return 0;
}

/******************************************************************************
 * FunctionName : mdns_get_servername
 * Description  : get server name
 * Parameters   : none
 * Returns      : server name
*******************************************************************************/
static int ICACHE_FLASH_ATTR lmdns_get_servername(lua_State* L) {
	char* servername = espconn_mdns_get_servername();
	lua_pushstring(L,servername);
	return 1;
}

/******************************************************************************
 * FunctionName : mdns_get_servername
 * Description  : set server name
 * Parameters   : server name
 * Returns      : none
*******************************************************************************/
static int ICACHE_FLASH_ATTR lmdns_set_servername(lua_State* L) {
	const char* servername = luaL_checkstring(L,1);
	espconn_mdns_set_servername(servername);
	return 0;
}

/******************************************************************************
 * FunctionName : mdns_set_hostname
 * Description  : set host name
 * Parameters   : host name
 * Returns      : none
*******************************************************************************/
static int ICACHE_FLASH_ATTR lmdns_set_hostname(lua_State* L) {
	const char* hostname = luaL_checkstring(L,1);
	espconn_mdns_set_hostname(strdup(hostname));
	return 0;
}
/******************************************************************************
 * FunctionName : mdns_get_hostname
 * Description  : get host name
 * Parameters   : void
 * Returns      : hostname
*******************************************************************************/
static int ICACHE_FLASH_ATTR lmdns_get_hostname(lua_State* L) {
	char* hostname = espconn_mdns_get_hostname();
	lua_pushstring(L, hostname);
	return 1;
}
/******************************************************************************
 * FunctionName : mdns_disable
 * Returns      : none
*******************************************************************************/
static int ICACHE_FLASH_ATTR lmdns_disable(lua_State* L) {
	espconn_mdns_disable();
	return 0;
}
/******************************************************************************
 * FunctionName : mdns_enable
 * Description  : enable mdns
 * Parameters   : void
 * Returns      : none
*******************************************************************************/
static int ICACHE_FLASH_ATTR lmdns_enable(lua_State* L) {
	espconn_mdns_enable();
	return 0;
}


#define MIN_OPT_LEVEL 2
#include "lrodefs.h"
const LUA_REG_TYPE mdns_map[] =
{
	{	LSTRKEY( "init" ), LFUNCVAL( lmdns_init )},
	{	LSTRKEY( "enable" ), LFUNCVAL( lmdns_enable)},
	{	LSTRKEY( "disable" ), LFUNCVAL( lmdns_disable)},
	{	LSTRKEY( "close" ), LFUNCVAL( lmdns_close)},
	{	LSTRKEY( "get_hostname" ), LFUNCVAL( lmdns_get_hostname)},
	{	LSTRKEY( "set_hostname" ), LFUNCVAL( lmdns_set_hostname)},
	{	LSTRKEY( "get_servername" ), LFUNCVAL( lmdns_get_servername)},
	{	LSTRKEY( "set_servername" ), LFUNCVAL( lmdns_set_servername)},
	{	LSTRKEY( "server_register" ), LFUNCVAL( lmdns_server_register)},
	{	LSTRKEY( "server_unregister" ), LFUNCVAL( lmdns_server_unregister)},

	{	LNILKEY, LNILVAL}
};

LUALIB_API int luaopen_mdns(lua_State *L) {
	LREGISTER(L, "mdns", mdns_map);
	return 1;
}
