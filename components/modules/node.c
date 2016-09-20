#include "module.h"
#include "lauxlib.h"
#include "esp_system.h"

// Lua: heap()
static int node_heap( lua_State* L )
{
  uint32_t sz = system_get_free_heap_size();
  lua_pushinteger(L, sz);
return 1;
}


static const LUA_REG_TYPE node_map[] =
{
  { LSTRKEY( "heap" ), LFUNCVAL( node_heap ) },
  { LNILKEY, LNILVAL }
};

NODEMCU_MODULE(NODE, "node", node_map, NULL);
