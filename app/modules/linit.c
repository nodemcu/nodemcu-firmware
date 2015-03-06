/*
** $Id: linit.c,v 1.14.1.1 2007/12/27 13:02:25 roberto Exp $
** Initialization of libraries for lua.c
** See Copyright Notice in lua.h
*/


#define linit_c
#define LUA_LIB

#include "lua.h"

#include "lualib.h"
#include "lauxlib.h"
#include "lrotable.h"
#include "luaconf.h"

#include "user_modules.h"

#if defined(LUA_USE_MODULES)
#include "modules.h"
#endif

#if defined(LUA_MODULES_ROM)
#undef _ROM
#define _ROM( name, openf, table ) extern int openf(lua_State *);
LUA_MODULES_ROM
#endif

static const luaL_Reg lualibs[] = {
  {"", luaopen_base},
  {LUA_LOADLIBNAME, luaopen_package},
#if defined(LUA_USE_BUILTIN_IO)
  {LUA_IOLIBNAME, luaopen_io},
#endif

#if defined(LUA_USE_BUILTIN_STRING)
  {LUA_STRLIBNAME, luaopen_string},
#endif

#if LUA_OPTIMIZE_MEMORY == 0
  #if defined(LUA_USE_BUILTIN_MATH)
  {LUA_MATHLIBNAME, luaopen_math},
  #endif

  #if defined(LUA_USE_BUILTIN_TABLE)
  {LUA_TABLIBNAME, luaopen_table},  
  #endif

  #if defined(LUA_USE_BUILTIN_DEBUG)
  {LUA_DBLIBNAME, luaopen_debug},
  #endif 
#endif
#if defined(LUA_MODULES_ROM)
#undef _ROM
#define _ROM( name, openf, table ) { name, openf },
  LUA_MODULES_ROM
#endif
  {NULL, NULL}
};

#if defined(LUA_USE_BUILTIN_STRING)
extern const luaR_entry strlib[];
#endif

#if defined(LUA_USE_BUILTIN_OS)
extern const luaR_entry syslib[];
#endif

#if defined(LUA_USE_BUILTIN_TABLE)
extern const luaR_entry tab_funcs[];
#endif

#if defined(LUA_USE_BUILTIN_DEBUG)
extern const luaR_entry dblib[];
#endif

#if defined(LUA_USE_BUILTIN_COROUTINE)
extern const luaR_entry co_funcs[];
#endif

#if defined(LUA_USE_BUILTIN_MATH)
extern const luaR_entry math_map[];
#endif

#if defined(LUA_MODULES_ROM) && LUA_OPTIMIZE_MEMORY == 2
#undef _ROM
#define _ROM( name, openf, table ) extern const luaR_entry table[];
LUA_MODULES_ROM
#endif
const luaR_table lua_rotable[] = 
{
#if LUA_OPTIMIZE_MEMORY > 0
  #if defined(LUA_USE_BUILTIN_STRING)
  {LUA_STRLIBNAME, strlib},
  #endif

  #if defined(LUA_USE_BUILTIN_TABLE)
  {LUA_TABLIBNAME, tab_funcs},
  #endif

  #if defined(LUA_USE_BUILTIN_DEBUG)
  {LUA_DBLIBNAME, dblib},
  #endif

  #if defined(LUA_USE_BUILTIN_COROUTINE)
  {LUA_COLIBNAME, co_funcs},
  #endif

  #if defined(LUA_USE_BUILTIN_MATH)
  {LUA_MATHLIBNAME, math_map},
  #endif

  #if defined(LUA_USE_BUILTIN_OS)
  {LUA_OSLIBNAME, syslib},
  #endif

#if defined(LUA_MODULES_ROM) && LUA_OPTIMIZE_MEMORY == 2
#undef _ROM
#define _ROM( name, openf, table ) { name, table },
  LUA_MODULES_ROM
#endif
#endif
  {NULL, NULL}
};

LUALIB_API void luaL_openlibs (lua_State *L) {
  const luaL_Reg *lib = lualibs;
  for (; lib->func; lib++) {
    lua_pushcfunction(L, lib->func);
    lua_pushstring(L, lib->name);
    lua_call(L, 1, 0);
  }
}

