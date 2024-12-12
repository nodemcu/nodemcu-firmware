#include "module.h"
#include "lauxlib.h"
#include "ip_fmt.h"

#include "esp_err.h"
#include "mdns.h"

// Table key names
static const char *HOSTNAME = "hostname";
static const char *INSTANCE_NAME = "instance_name";
static const char *SERVICES = "services";
static const char *SERVICE_TYPE = "service_type";
static const char *PROTO = "protocol";
static const char *PORT = "port";
static const char *TXT = "txt";
static const char *SUBTYPE = "subtype";
static const char *QUERY_TYPE = "query_type";
static const char *NAME = "name";
static const char *TIMEOUT = "timeout";
static const char *MAX_RESULTS = "max_results";
static const char *ADDRESSES = "addresses";

#define DEFAULT_TIMEOUT_MS 2000
#define DEFAULT_MAX_RESULTS 10

static bool started;

static bool valid_query_type(int t)
{
  switch(t)
  {
    case MDNS_TYPE_A:
    case MDNS_TYPE_PTR:
    case MDNS_TYPE_TXT:
    case MDNS_TYPE_AAAA:
    case MDNS_TYPE_SRV:
    //case MDNS_TYPE_OPT:
    //case MDNS_TYPE_NSEC:
    case MDNS_TYPE_ANY: return true;
    default: return false;
  }
}


static int lmdns_start(lua_State *L)
{
  luaL_checktable(L, 1);
  lua_settop(L, 1);

  if (started)
    return luaL_error(L, "already started");

  bool inited = false;
  esp_err_t err = mdns_init();
  if (err != ESP_OK)
    goto mdns_err;
  inited = true;

  // Hostname
  lua_getfield(L, 1, HOSTNAME);
  const char *hostname = luaL_optstring(L, -1, NULL);
  if (hostname)
  {
    err = mdns_hostname_set(hostname);
    if (err != ESP_OK)
      goto mdns_err;
  }
  lua_pop(L, 1);

  // Instance name
  lua_getfield(L, 1, INSTANCE_NAME);
  const char *instname = luaL_optstring(L, -1, NULL);
  if (instname)
  {
    err = mdns_instance_name_set(instname);
    if (err != ESP_OK)
      goto mdns_err;
  }
  lua_pop(L, 1);

  // Services
  lua_getfield(L, 1, SERVICES);
  unsigned i = 1;
  if (!lua_isnoneornil(L, 2)) // array of service entries
  {
    luaL_checktable(L, 2);
    for (i = 1; true; ++i)
    {
      lua_rawgeti(L, 2, i);
      if (!lua_istable(L, 3))
        break;

      lua_getfield(L, 3, SERVICE_TYPE);
      const char *svctype = luaL_checkstring(L, -1);

      lua_getfield(L, 3, PROTO);
      const char *proto = luaL_checkstring(L, -1);

      lua_getfield(L, 3, PORT);
      int port = luaL_checkint(L, -1);

      lua_getfield(L, 3, INSTANCE_NAME);
      const char *instname2 = luaL_optstring(L, -1, NULL);

      // Note: we add txt entries iteratively to avoid having to size and
      // allocate a buffer to hold them all.
      err = mdns_service_add(instname2, svctype, proto, port, NULL, 0);
      if (err != ESP_OK)
        goto mdns_err;

      lua_pop(L, 4); // svctype, proto, port, instname2

      lua_getfield(L, 3, TXT);
      if (lua_istable(L, 4))
      {
        lua_pushnil(L); // 5 is now table key
        while(lua_next(L, 4)) // replaces 5 with actual key
        {
          // copy key, value so we can safely tostring() them
          lua_pushvalue(L, 5);
          lua_pushvalue(L, 6);

          const char *key = luaL_checkstring(L, -2);
          const char *val = luaL_checkstring(L, -1);

          err = mdns_service_txt_item_set_for_host(
            instname2, svctype, proto, hostname, key, val);
          if (err != ESP_OK)
            goto mdns_err;

          lua_pop(L, 3); // value, key, value
        }
      }
      lua_pop(L, 1); // txt table

      // Subtype
      lua_getfield(L, 1, SUBTYPE);
      const char *subtype = luaL_optstring(L, -1, NULL);
      if (subtype)
      {
        err = mdns_service_subtype_add_for_host(
          instname2, svctype, proto, hostname, subtype);
        if (err != ESP_OK)
          goto mdns_err;
      }
      lua_pop(L, 1); // subtype

      lua_pop(L, 1); // services[i] table
    }
  }
  lua_pop(L, 1); // services array

  started = true;

  // Return number of services we added
  lua_pushinteger(L, i - 1);
  return 1;

mdns_err:
  if (inited)
  {
    mdns_service_remove_all();
    mdns_free();
  }
  return luaL_error(L, "mdns error: %s", esp_err_to_name(err));
}


static int lmdns_stop(lua_State *L)
{
  if (started)
  {
    mdns_service_remove_all();
    started = false;
  }
  mdns_free();
  return 0;
}


static int lmdns_query(lua_State *L)
{
  luaL_checktable(L, 1);
  lua_settop(L, 1);

  lua_getfield(L, 1, NAME);
  const char *name = luaL_optstring(L, -1, NULL);

  lua_getfield(L, 1, SERVICE_TYPE);
  const char *svctype = luaL_optstring(L, -1, NULL);

  lua_getfield(L, 1, PROTO);
  const char *proto = luaL_optstring(L, -1, NULL);

  lua_getfield(L, 1, QUERY_TYPE);
  int qtype = luaL_checkint(L, -1);
  if (!valid_query_type(qtype))
    return luaL_error(L, "unknown mDNS query type");

  lua_getfield(L, 1, TIMEOUT);
  int timeout = luaL_optinteger(L, -1, DEFAULT_TIMEOUT_MS);

  lua_getfield(L, 1, MAX_RESULTS);
  int max_results = luaL_optinteger(L, -1, DEFAULT_MAX_RESULTS);

  mdns_result_t *res = NULL;
  esp_err_t err =
    mdns_query(name, svctype, proto, qtype, timeout, max_results, &res);
  if (err != ESP_OK)
    return luaL_error(L, "mdns error: %s", esp_err_to_name(err));

  lua_settop(L, 0);
  lua_createtable(L, max_results, 0); // results array at idx 1

  for (int n = 1; res; ++n, res = res->next)
  {
    // Reserve 5 slots, for SRV result host/port/instance/service_type/proto
    lua_createtable(L, 0, 5); // result entry table at idx 2

    if (res->instance_name)
    {
      lua_pushstring(L, res->instance_name);
      lua_setfield(L, 2, INSTANCE_NAME);
    }
    if (res->service_type)
    {
      lua_pushstring(L, res->service_type);
      lua_setfield(L, 2, SERVICE_TYPE);
    }
    if (res->proto)
    {
      lua_pushstring(L, res->proto);
      lua_setfield(L, 2, PROTO);
    }
    if (res->hostname)
    {
      lua_pushstring(L, res->hostname);
      lua_setfield(L, 2, HOSTNAME);
    }
    if (res->port)
    {
      lua_pushinteger(L, res->port);
      lua_setfield(L, 2, PORT);
    }
    if (res->txt)
    {
      lua_createtable(L, 0, res->txt_count); // txt table at idx 3
      for (int i = 0; i < res->txt_count; ++i)
      {
        lua_pushstring(L, res->txt[i].key);
        if (res->txt[i].value)
          lua_pushlstring(L, res->txt[i].value, res->txt_value_len[i]);
        else
          lua_pushliteral(L, "");
        lua_settable(L, 3);
      }
      lua_setfield(L, 2, TXT);
    }
    if (res->addr)
    {
      lua_createtable(L, 1, 0); // address array table at idx 3
      int i = 1;
      for (mdns_ip_addr_t *a = res->addr; a; ++i, a = a->next)
      {
        char buf[IP_STR_SZ];
        ipstr_esp(buf, &a->addr);
        lua_pushstring(L, buf);
        lua_rawseti(L, 3, i);
      }
      lua_setfield(L, 2, ADDRESSES);
    }

    lua_rawseti(L, 1, n); // insert into array of results
  }

  mdns_query_results_free(res);
  return 1;
}


LROT_BEGIN(mdns, NULL, 0)
  LROT_FUNCENTRY( start,     lmdns_start    )
  LROT_FUNCENTRY( query,     lmdns_query    )
  LROT_FUNCENTRY( stop,      lmdns_stop     )

  LROT_NUMENTRY( TYPE_A,     MDNS_TYPE_A    )
  LROT_NUMENTRY( TYPE_PTR,   MDNS_TYPE_PTR  )
  LROT_NUMENTRY( TYPE_TXT,   MDNS_TYPE_TXT  )
  LROT_NUMENTRY( TYPE_AAAA,  MDNS_TYPE_AAAA )
  LROT_NUMENTRY( TYPE_SRV,   MDNS_TYPE_SRV  )
  //LROT_NUMENTRY( TYPE_OPT,   MDNS_TYPE_OPT  )
  //LROT_NUMENTRY( TYPE_NSEC,  MDNS_TYPE_NSEC )
  LROT_NUMENTRY( TYPE_ANY,   MDNS_TYPE_ANY  )
LROT_END(mdns, NULL, 0)

NODEMCU_MODULE(MDNS, "mdns", mdns, NULL);
