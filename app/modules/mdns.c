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

typedef struct wrapper {
  struct nodemcu_mdns_info mdns_info;
  char data;
} wrapper_t;

static wrapper_t *info;

typedef enum phase {
  PHASE_CALCULATE_LENGTH,
  PHASE_COPY_DATA
} phase_t;

static char *advance_over_string(char *s)
{
  while (*s++) {
  }
  // s now points after the null
  return s;
}

//
// mdns.close()
// 
static int mdns_close(lua_State *L)
{
  if (info) {
    nodemcu_mdns_close();
    c_free(info);
    info = NULL;
  }
  return 0;
}

// 
// this handles all the arguments. Two passes are necessary -- 
// one to calculate the size of the block, and the other to 
// copy the data. It is vitally important that these two
// passes are kept in step.
//
static wrapper_t *process_args(lua_State *L, phase_t phase, size_t *sizep)
{
  wrapper_t *result = NULL;
  char *p = NULL;
  bool has_desc = !lua_isnumber(L, 3);
  int arg_no = 1;

  if (phase == PHASE_COPY_DATA) {
    result = (wrapper_t *) c_zalloc(sizeof(wrapper_t) + *sizep);
    if (!result) {
      return NULL;
    }
    p = &result->data;
  }

  if (phase == PHASE_CALCULATE_LENGTH) {
    luaL_checktype(L, arg_no, LUA_TSTRING);
    *sizep += c_strlen(luaL_checkstring(L, arg_no++)) + 1;
    luaL_checktype(L, arg_no, LUA_TSTRING);
    *sizep += c_strlen(luaL_checkstring(L, arg_no++)) + 1;
    if (has_desc) {
      luaL_checktype(L, arg_no, LUA_TSTRING);
      *sizep += c_strlen(luaL_checkstring(L, arg_no++)) + 1;
    }
    (void) luaL_checkinteger(L, arg_no++);
  } else {
    c_strcpy(p, luaL_checkstring(L, arg_no++)); 
    result->mdns_info.host_name = p;
    result->mdns_info.host_desc = p;
    p = advance_over_string(p);

    if (has_desc) {
      c_strcpy(p, luaL_checkstring(L, arg_no++));
      result->mdns_info.host_desc = p;
      p = advance_over_string(p);
    }

    c_strcpy(p, luaL_checkstring(L, arg_no++));
    result->mdns_info.service_name = p;
    p = advance_over_string(p);

    result->mdns_info.server_port = luaL_checkinteger(L, arg_no++);
  }

  if (lua_gettop(L) >= arg_no) {
    luaL_checktype(L, arg_no, LUA_TTABLE);
    lua_pushnil(L); // first key
    int slot = 0;
    while (lua_next(L, arg_no) != 0 && slot < sizeof(result->mdns_info.txt_data) / sizeof(result->mdns_info.txt_data[0])) {
      if (phase == PHASE_CALCULATE_LENGTH) {
        luaL_checktype(L, -2, LUA_TSTRING);
        *sizep += c_strlen(luaL_checkstring(L, -2)) + 1;
        *sizep += c_strlen(luaL_checkstring(L, -1)) + 1;
      } else {
        // put in the key
	c_strcpy(p, luaL_checkstring(L, -2));
        result->mdns_info.txt_data[slot++] = p;
	p = advance_over_string(p);

	// now smash in the value
	const char *value = luaL_checkstring(L, -1);
	p[-1] = '=';
	c_strcpy(p, value);
	p = advance_over_string(p);
      }
      lua_pop(L, 1);
    }
  }

  return result;
}

//
// mdns.register(hostname, [host desc, ] servicename, port [, attributes])
//
static int mdns_register(lua_State *L)
{
  size_t len = 0;

  (void) process_args(L, PHASE_CALCULATE_LENGTH, &len);

  struct ip_info ipconfig;

  uint8_t mode = wifi_get_opmode();

  if (!wifi_get_ip_info((mode == 2) ? SOFTAP_IF : STATION_IF, &ipconfig) || !ipconfig.ip.addr) {
    return luaL_error(L, "No network connection");
  }

  wrapper_t *result = process_args(L, PHASE_COPY_DATA, &len);

  if (!result) {
    return luaL_error( L, "failed to allocate info block" );
  }

  // Close up the old session (if any). This cannot fail
  // so no chance of losing the memory in 'result'
  
  mdns_close(L);

  // Save the result as it appears that nodemcu_mdns_init needs
  // to have the data valid while it is running.

  info = result;

  nodemcu_mdns_init(&(info->mdns_info));

  return 0;
}

// Module function map
static const LUA_REG_TYPE mdns_map[] = {
  { LSTRKEY("register"),  LFUNCVAL(mdns_register)  },
  { LSTRKEY("close"),     LFUNCVAL(mdns_close)     },
  { LNILKEY, LNILVAL }
};

NODEMCU_MODULE(MDNS, "mdns", mdns_map, NULL);
