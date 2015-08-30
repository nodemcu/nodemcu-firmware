#include "cjson_mem.h"
#include "../lua/lauxlib.h"
#include <c_stdlib.h>

static lua_State *gL;
static const char errfmt[] = "cjson %salloc: out of mem (%d bytes)";

void cjson_mem_setlua (lua_State *L)
{
  gL = L;
}

void *cjson_mem_malloc (uint32_t sz)
{
  void *p = (void*)c_malloc (sz);
  if (!p && gL)
    luaL_error (gL, errfmt, "m", sz);
  return p;
}


void *cjson_mem_realloc (void *o, uint32_t sz)
{
  void *p = (void*)c_realloc (o, sz);
  if (!p && gL)
    luaL_error (gL, errfmt, "re", sz);
  return p;
}
