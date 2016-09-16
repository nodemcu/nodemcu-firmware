/*
** $Id: linit.c,v 1.14.1.1 2007/12/27 13:02:25 roberto Exp $
** Initialization of libraries for lua.c
** See Copyright Notice in lua.h
*/


#define linit_c
#define LUA_LIB
#define LUAC_CROSS_FILE

#include "lua.h"

#include "lualib.h"
#include "lauxlib.h"
#include "luaconf.h"
#include "module.h"


BUILTIN_LIB_INIT( BASE,      "",                 luaopen_base);
BUILTIN_LIB_INIT( LOADLIB,   LUA_LOADLIBNAME,    luaopen_package);

#if defined(LUA_USE_BUILTIN_IO)
BUILTIN_LIB_INIT( IO,        LUA_IOLIBNAME,      luaopen_io);
#endif

#if defined (LUA_USE_BUILTIN_STRING)
extern const luaR_entry strlib[];
BUILTIN_LIB_INIT( STRING,    LUA_STRLIBNAME,     luaopen_string);
BUILTIN_LIB(      STRING,    LUA_STRLIBNAME,     strlib);
#endif

#if defined(LUA_USE_BUILTIN_TABLE)
extern const luaR_entry tab_funcs[];
BUILTIN_LIB_INIT( TABLE,     LUA_TABLIBNAME,     luaopen_table);
BUILTIN_LIB(      TABLE,     LUA_TABLIBNAME,     tab_funcs);
#endif

#if defined(LUA_USE_BUILTIN_DEBUG) || defined(LUA_USE_BUILTIN_DEBUG_MINIMAL)
extern const luaR_entry dblib[];
BUILTIN_LIB_INIT( DBG,       LUA_DBLIBNAME,      luaopen_debug);
BUILTIN_LIB(      DBG,       LUA_DBLIBNAME,      dblib);
#endif

#if defined(LUA_USE_BUILTIN_OS)
extern const luaR_entry syslib[];
BUILTIN_LIB(      OS,        LUA_OSLIBNAME,     syslib);
#endif

#if defined(LUA_USE_BUILTIN_COROUTINE)
extern const luaR_entry co_funcs[];
BUILTIN_LIB(      CO,        LUA_COLIBNAME,     co_funcs);
#endif

#if defined(LUA_USE_BUILTIN_MATH)
extern const luaR_entry math_map[];
BUILTIN_LIB(      MATH,      LUA_MATHLIBNAME,   math_map);
#endif

#ifdef LUA_CROSS_COMPILER
const luaL_Reg lua_libs[] = {{NULL, NULL}};
const luaR_table lua_rotable[] = {{NULL, NULL}};
#else 
extern const luaL_Reg lua_libs[];
#endif

void luaL_openlibs (lua_State *L) {
  const luaL_Reg *lib = lua_libs;
  for (; lib->name; lib++) {
    if (lib->func)
    {
      lua_pushcfunction(L, lib->func);
      lua_pushstring(L, lib->name);
      lua_call(L, 1, 0);
    }
  }
}

