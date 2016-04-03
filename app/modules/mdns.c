// Module for access to the nodemcu_mdns functions

#include "module.h"
#include "lauxlib.h"

#include "c_string.h"
#include "c_stdlib.h"

#include "c_types.h"
#include "mem.h"
#include "lwip/ip_addr.h"
#include "nodemcu_mdns.h"
#include "user_interface.h"

//
// mdns.close()
// 
static int mdns_close(lua_State *L)
{
  nodemcu_mdns_close();
  return 0;
}

//
// mdns.register(hostname [, { attributes} ])
//
static int mdns_register(lua_State *L)
{
  struct nodemcu_mdns_info info;

  memset(&info, 0, sizeof(info));
  
  info.host_name = luaL_checkstring(L, 1);
  info.service_name = "http";
  info.service_port = 80;
  info.host_desc = info.host_name;

  if (lua_gettop(L) >= 2) {
    luaL_checktype(L, 2, LUA_TTABLE);
    lua_pushnil(L); // first key
    int slot = 0;
    while (lua_next(L, 2) != 0 && slot < sizeof(info.txt_data) / sizeof(info.txt_data[0])) {
      luaL_checktype(L, -2, LUA_TSTRING);
      const char *key = luaL_checkstring(L, -2);

      if (c_strcmp(key, "port") == 0) {
	info.service_port = luaL_checknumber(L, -1);
      } else if (c_strcmp(key, "service") == 0) {
	info.service_name = luaL_checkstring(L, -1);
      } else if (c_strcmp(key, "description") == 0) {
	info.host_desc = luaL_checkstring(L, -1);
      } else {
	int len = c_strlen(key) + 1;
	const char *value = luaL_checkstring(L, -1);
	char *p = alloca(len + c_strlen(value) + 1);
	strcpy(p, key);
	strcat(p, "=");
	strcat(p, value);
	info.txt_data[slot++] = p;
      }
      lua_pop(L, 1);
    }
  }


  struct ip_info ipconfig;

  uint8_t mode = wifi_get_opmode();

  if (!wifi_get_ip_info((mode == 2) ? SOFTAP_IF : STATION_IF, &ipconfig) || !ipconfig.ip.addr) {
    return luaL_error(L, "No network connection");
  }

  // Close up the old session (if any). This cannot fail
  // so no chance of losing the memory in 'result'
  
  mdns_close(L);

  // Save the result as it appears that nodemcu_mdns_init needs
  // to have the data valid while it is running.

  if (!nodemcu_mdns_init(&info)) {
    mdns_close(L);
    return luaL_error(L, "Unable to start mDns daemon");
  }

  return 0;
}

// Module function map
static const LUA_REG_TYPE mdns_map[] = {
  { LSTRKEY("register"),  LFUNCVAL(mdns_register)  },
  { LSTRKEY("close"),     LFUNCVAL(mdns_close)     },
  { LNILKEY, LNILVAL }
};

NODEMCU_MODULE(MDNS, "mdns", mdns_map, NULL);
