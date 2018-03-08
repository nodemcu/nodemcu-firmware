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
#if defined(LUA_CROSS_COMPILER)
BUILTIN_LIB(      start_list,  NULL,               NULL);
BUILTIN_LIB_INIT( start_list,  NULL,               NULL);
#endif
extern const luaR_entry strlib[], tab_funcs[],  dblib[], 
                        co_funcs[], math_map[], syslib[];

BUILTIN_LIB_INIT( BASE,      "",                luaopen_base);
BUILTIN_LIB_INIT( LOADLIB,   LUA_LOADLIBNAME,   luaopen_package);

BUILTIN_LIB(      STRING,    LUA_STRLIBNAME,    strlib);
BUILTIN_LIB_INIT( STRING,    LUA_STRLIBNAME,    luaopen_string);

BUILTIN_LIB(      TABLE,     LUA_TABLIBNAME,    tab_funcs);
BUILTIN_LIB_INIT( TABLE,     LUA_TABLIBNAME,    luaopen_table);

BUILTIN_LIB(      DBG,       LUA_DBLIBNAME,     dblib);
BUILTIN_LIB_INIT( DBG,       LUA_DBLIBNAME,     luaopen_debug);

BUILTIN_LIB(      CO,        LUA_COLIBNAME,     co_funcs);

BUILTIN_LIB(      MATH,      LUA_MATHLIBNAME,   math_map);

#if defined(LUA_CROSS_COMPILER)
extern const luaR_entry syslib[], iolib[];
BUILTIN_LIB(      OS,        LUA_OSLIBNAME,     syslib);
BUILTIN_LIB_INIT( IO,        LUA_IOLIBNAME,     luaopen_io);
BUILTIN_LIB(      end_list,  NULL,               NULL);
BUILTIN_LIB_INIT( end_list,  NULL,               NULL);
/*
 * These base addresses are internal to this module for cross compile builds
 * This also exploits feature of the GCC code generator that the variables are
 * emitted in either normal OR reverse order within PSECT.
 */
#define isascending(n) ((&(n ## _end_list)-&(n ## _start_list))>0)
static const luaL_Reg   *lua_libs;
const        luaR_table *lua_rotable;
#else 
/* These base addresses are Xtensa toolchain linker constants for Firmware builds */ 
extern const luaL_Reg    lua_libs_base[];
extern const luaR_table  lua_rotable_base[];
static const luaL_Reg   *lua_libs    = lua_libs_base;
const        luaR_table *lua_rotable = lua_rotable_base;
#endif

void luaL_openlibs (lua_State *L) {
#if defined(LUA_CROSS_COMPILER)
lua_libs    = (isascending(lua_lib) ? &lua_lib_start_list : &lua_lib_end_list) + 1;
lua_rotable = (isascending(lua_rotable) ? &lua_rotable_start_list : &lua_rotable_end_list) + 1;
#endif
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

