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
#include "lstate.h"
#include <assert.h>
#include <stdalign.h>

LROT_EXTERN(strlib);
LROT_EXTERN(tab_funcs);
LROT_EXTERN(dblib);
LROT_EXTERN(co_funcs);
LROT_EXTERN(math);
#if defined(LUA_CROSS_COMPILER)
LROT_EXTERN(oslib);
//LROT_EXTERN(iolib);
#endif
/*
 * The NodeMCU Lua initalisation has been adapted to use linker-based module
 * registration.  This uses a PSECT naming convention to allow the ROTable and
 * initialisation function entries to be collected by the linker into two
 * consoliated ROTables.  This significantly simplifies adding new modules and
 * configuring builds with a small subset of the total modules.
 *
 * This linker-based approach is not practical for cross compiler builds which
 * must link on a range of platforms, and where we don't have control of PSECT
 * placement.   However unlike the target builds, the  luac.cross builds only
 * used a small and fixed list of libraries and so in this case the table can
 * be defined in this source file, so in this case all library ROTables are
 * defined here, avoiding the need for linker magic is avoided on host builds.
 *
 * Note that a separate ROTable is defined in lbaselib.c for the base functions
 * and there is a metatable index cascade from _G to this base function table to
 * the master rotables table.  In the target build, the linker marshals the
 * table, hence the LROT_BREAK() macros which don't 0 terminate the lists.
 */

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

LROT_PUBLIC_BEGIN(LOCK_IN_SECTION(rotable) lua_rotables)
#ifdef CONFIG_LUA_BUILTIN_STRING
  LROT_TABENTRY( string, strlib )
#endif
#ifdef CONFIG_LUA_BUILTIN_TABLE
  LROT_TABENTRY( table, tab_funcs )
#endif
#ifdef CONFIG_LUA_BUILTIN_COROUTINE
  LROT_TABENTRY( coroutine, co_funcs )
#endif
#ifdef CONFIG_LUA_BUILTIN_DEBUG
  LROT_TABENTRY( debug, dblib)
#endif
#ifdef CONFIG_LUA_BUILTIN_MATH
  LROT_TABENTRY( math, math )
#endif
  LROT_TABENTRY( ROM, lua_rotables )
#ifdef LUA_CROSS_COMPILER
  LROT_TABENTRY( os, oslib )
  //LROT_TABENTRY( io, iolib )
LROT_END(lua_rotables, NULL, 0)
#else
LROT_BREAK(lua_rotables)
#endif

LROT_PUBLIC_BEGIN(LOCK_IN_SECTION(libs) lua_libs)
  LROT_FUNCENTRY( _, luaopen_base )
  LROT_FUNCENTRY( package, luaopen_package )
#ifdef CONFIG_LUA_BUILTIN_STRING
  LROT_FUNCENTRY( string, luaopen_string )
#endif
#ifdef CONFIG_LUA_BUILTIN_TABLE
  LROT_FUNCENTRY( table, luaopen_table )
#endif
#ifdef CONFIG_LUA_BUILTIN_DEBUG
  LROT_FUNCENTRY( debug, luaopen_debug )
#endif
#ifndef LUA_CROSS_COMPILER
LROT_BREAK(lua_rotables)
#else
  LROT_FUNCENTRY( io, luaopen_io )
LROT_END( lua_libs, NULL, 0)
#endif

#ifndef LUA_CROSS_COMPILER
extern void luaL_dbgbreak(void);
#endif

void luaL_openlibs (lua_State *L) {

  lua_pushrotable(L, LROT_TABLEREF(lua_libs));
  lua_pushnil(L);  /* first key */
  /* loop round and open libraries */
#ifndef LUA_CROSS_COMPILER
// luaL_dbgbreak();  // This is a test point for debugging library start ups
#endif
  while (lua_next(L, -2) != 0) {
    if (lua_islightfunction(L,-1) &&
        fvalue(L->top-1)) { // only process function entries
      lua_pushvalue(L, -2);
      lua_call(L, 1, 0);  // call luaopen_XXX(libname)
    } else {
      lua_pop(L, 1);
    }
  }
  lua_pop(L, 1);  //cleanup stack
}

#ifndef LUA_CROSS_COMPILER
# ifdef LUA_NUMBER_INTEGRAL
#  define COMPARE <=
# else
#  define COMPARE ==
# endif
_Static_assert(_Alignof(luaR_entry) COMPARE 8, "Unexpected alignment of module registration - update the linker script snippets to match!");
_Static_assert(sizeof(luaR_entry) COMPARE 24, "Unexpect size of array member - update the linker script snippets to match!");
#endif
