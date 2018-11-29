#include "lauxlib.h"
#include "common.h"

// Like luaL_argerror but for options table
int opt_error(lua_State *L, const char* name, const char *extramsg) {
  lua_Debug ar;
  if (!lua_getstack(L, 0, &ar))  /* no stack frame? */
    return luaL_error(L, "bad option %s (%s)", name, extramsg);
  lua_getinfo(L, "n", &ar);
  if (ar.name == NULL)
    ar.name = "?";
  return luaL_error(L, "bad option %s to " LUA_QS " (%s)",
                        name, ar.name, extramsg);
}

// Similar to luaL_argcheck() but using the options table rather than a raw index
bool opt_get(lua_State *L, const char *name, int required_type)
{
  if (!lua_istable(L, -1)) {
    return false;
  }

  lua_getfield(L, -1, name);
  int type = lua_type(L, -1);
  if (type == LUA_TNIL) {
    // Option not present
    lua_pop(L, 1);
    return false;
  }
  if (type != required_type) {
    const char* msg = lua_pushfstring(L, "%s expected, got %s",
                                      lua_typename(L, required_type),
                                      lua_typename(L, type));
    opt_error(L, name, msg);
  }
  return true;
}

int opt_checkint(lua_State *L, const char *name, int default_val)
{
  if (opt_get(L, name, LUA_TNUMBER)) {
    int result = lua_tointeger(L, -1);
    lua_pop(L, 1);
    return result;
  } else {
    return default_val;
  }
}

int opt_checkint_range(lua_State *L, const char *name, int default_val, int min_val, int max_val)
{
  int result = default_val;

  if (opt_get(L, name, LUA_TNUMBER)) {
    result = lua_tointeger(L, -1);
    lua_pop(L, 1);
  }
  if (!(result >= min_val && result <= max_val)) {
    const char* msg = lua_pushfstring(L, "must be in range %d-%d", min_val, max_val);
    opt_error(L, name, msg);
  }
  return result;
}

bool opt_checkbool(lua_State *L, const char *name, bool default_val)
{
  if (opt_get(L, name, LUA_TBOOLEAN)) {
    int result = lua_toboolean(L, -1);
    lua_pop(L, 1);
    return !!result;
  } else {
    return default_val;
  }
}

const char *opt_checklstring(lua_State *L, const char *name, const char *default_val, size_t *l)
{
  if (opt_get(L, name, LUA_TSTRING)) {
    const char *result = lua_tolstring(L, -1, l);
    lua_pop(L, 1);
    return result;
  } else {
    return default_val;
  }
}
