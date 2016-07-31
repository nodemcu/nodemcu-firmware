#include "cjson_mem.h"
#include "../lua/lauxlib.h"
#include <c_stdlib.h>

static const char errfmt[] = "cjson %salloc: out of mem (%d bytes)";

void *cjson_mem_malloc (uint32_t sz)
{
  void *p = (void*)c_malloc (sz);
  lua_State *L = lua_getstate();
  if (!p)
    luaL_error (L, errfmt, "m", sz);
  return p;
}


void *cjson_mem_realloc (void *o, uint32_t sz)
{
  void *p = (void*)c_realloc (o, sz);
  lua_State *L = lua_getstate();
  if (!p)
    luaL_error (L, errfmt, "re", sz);
  return p;
}
