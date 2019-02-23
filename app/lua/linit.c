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

#if !defined(LUA_CROSS_COMPILER) && !(MIN_OPT_LEVEL==2 && LUA_OPTIMIZE_MEMORY==2)
# error "NodeMCU modules must be built with LTR enabled (MIN_OPT_LEVEL=2 and LUA_OPTIMIZE_MEMORY=2)"
#endif

extern const luaR_entry strlib[], tab_funcs[],  dblib[],
                        co_funcs[], math_map[], syslib[];
extern const luaR_entry syslib[], io_funcs[];  // Only used on cross-compile builds

/*
 * The NodeMCU Lua initalisation has been adapted to use linker-based module
 * registration.  This uses a PSECT naming convention to allow the lib and rotab
 * entries to be collected by the linker into consoliated tables.  The linker
 * defines lua_libs_base and lua_rotable_base.
 *
 * This is not practical on Posix builds which use a standard loader declaration
 * so for cross compiler builds, separate ROTables are used for the base functions
 * and library ROTables, with the latter chained from the former using its __index
 * meta-method. In this case all library ROTables are defined here, avoiding the
 * need for linker magic is avoided on host builds.
 */

#if defined(LUA_CROSS_COMPILER)
#define LUA_ROTABLES lua_rotable_base
#define LUA_LIBS     lua_libs_base
#else /* declare Xtensa toolchain linker defined constants */
extern const luaL_Reg    lua_libs_base[];
extern const luaR_entry  lua_rotable_base[];
#define LUA_ROTABLES lua_rotable_core
#define LUA_LIBS     lua_libs_core
#endif

#ifdef _MSC_VER
//MSVC requires us to declare these sections before we refer to them
#pragma section(__ROSECNAME(A), read)
#pragma section(__ROSECNAME(zzzzzzzz), read)
#pragma section(__ROSECNAME(libs), read)
#pragma section(__ROSECNAME(rotable), read)

//These help us to find the beginning and ending of the RO data.  NOTE:  linker
//magic is used; the section names are lexically sorted, so 'a' and 'z' are
//important to keep the other sections lexically between these two dummy
//variables.  Check your mapfile output if you need to fiddle with this stuff.
const LOCK_IN_SECTION(A) char _ro_start[1] = {0};
const LOCK_IN_SECTION(zzzzzzzz) char _ro_end[1] = {0};
#endif
static const LOCK_IN_SECTION(libs) luaL_reg LUA_LIBS[] = {
  {"",              luaopen_base},
  {LUA_LOADLIBNAME, luaopen_package},
  {LUA_STRLIBNAME,  luaopen_string},
  {LUA_TABLIBNAME,  luaopen_table},
  {LUA_DBLIBNAME,   luaopen_debug}
#if defined(LUA_CROSS_COMPILER)
 ,{LUA_IOLIBNAME,   luaopen_io},
  {NULL,            NULL}
#endif
};

#define ENTRY(n,t)  {LSTRKEY(n), LRO_ROVAL(t)}

const LOCK_IN_SECTION(rotable) ROTable LUA_ROTABLES[] = {
  ENTRY("ROM",           LUA_ROTABLES),
  ENTRY(LUA_STRLIBNAME,  strlib),
  ENTRY(LUA_TABLIBNAME,  tab_funcs),
  ENTRY(LUA_DBLIBNAME,   dblib),
  ENTRY(LUA_COLIBNAME,   co_funcs),
  ENTRY(LUA_MATHLIBNAME, math_map)
#if defined(LUA_CROSS_COMPILER)
 ,ENTRY(LUA_OSLIBNAME,   syslib),
  LROT_END
#endif
  };

void luaL_openlibs (lua_State *L) {
  const luaL_Reg *lib = lua_libs_base;

  /* loop round and open libraries */
  for (; lib->name; lib++) {
    if (lib->func) {
      lua_pushcfunction(L, lib->func);
      lua_pushstring(L, lib->name);
      lua_call(L, 1, 0);
    }
  }
}

