/*
** $Id: linit.c,v 1.14.1.1 2007/12/27 13:02:25 roberto Exp $
** Initialization of libraries for lua.c
** See Copyright Notice in lua.h
*/


#define linit_c
#define LUA_LIB
#define LUA_CORE

/*
** NodeMCU uses RO segment based static ROTable declarations for all library
** tables, including the index of library tables itself (the ROM table). These
** tables can be moved from RAM to flash ROM on the ESPs.
**
** On the ESPs targets, we can marshal the table entries through linker-based
** PSECTs to enable the library initiation tables to be bound during the link
** process rather than being statically declared here. This simplifies the
** addition of new modules and configuring builds with a subset of the total
** modules available.
**
** Such a linker-based approach is not practical for cross compiler builds that
** must link on a range of platforms, and where we don't have control of PSECT
** placement.  However unlike the target builds, the luac.cross builds only
** use a small and fixed list of libraries and so in this case all of libraries
** are defined here, avoiding the need for linker magic on host builds.
**
** Note that a separate ROTable is defined in lbaselib.c on luac.cross builds
** for the base functions. (These use linker based entries on target builds)
** and there is a metatable index cascade from _G to this base function table
** to the master rotables table.  In the target build, the linker marshals the
** table, hence the LROT_BREAK() macros which don't 0 terminate the lists and
** skip generating the ROtable header.
 */
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
#include "lstate.h"
#include "lnodemcu.h"
#if LUA_VERSION_NUM == 501
#include "lflash.h"
#endif

extern LROT_TABLE(strlib);
extern LROT_TABLE(tab_funcs);
extern LROT_TABLE(dblib);
extern LROT_TABLE(co_funcs);
extern LROT_TABLE(mathlib);
extern LROT_TABLE(utf8);

#if defined(LUA_CROSS_COMPILER)

/* _G __index -> rotables __index -> base_func */
extern LROT_TABLE(rotables_meta);
extern LROT_TABLE(base_func);
#if LUA_VERSION_NUM == 501
extern LROT_TABLE(oslib);
extern LROT_TABLE(iolib);
#endif

LROT_BEGIN(rotables_meta, NULL, LROT_MASK_INDEX)
  LROT_TABENTRY( __index, base_func)
LROT_END(rotables_meta, NULL, LROT_MASK_INDEX)

LROT_BEGIN(rotables, LROT_TABLEREF(rotables_meta), 0)
  LROT_TABENTRY( string, strlib )
  LROT_TABENTRY( table, tab_funcs )
  LROT_TABENTRY( coroutine, co_funcs )
  LROT_TABENTRY( debug, dblib)
  LROT_TABENTRY( math, mathlib )
#if LUA_VERSION_NUM >= 503
  LROT_TABENTRY( utf8, utf8 )
#endif
  LROT_TABENTRY( ROM, rotables )
#if LUA_VERSION_NUM == 501
  LROT_TABENTRY( os, oslib )
  LROT_TABENTRY( io, iolib )
#endif
LROT_END(rotables, LROT_TABLEREF(rotables_meta), 0)

LROT_BEGIN(lua_libs, NULL, 0)
  LROT_FUNCENTRY( _G, luaopen_base )
  LROT_FUNCENTRY( package, luaopen_package )
  LROT_FUNCENTRY( string, luaopen_string )
#if LUA_VERSION_NUM >= 503
  LROT_FUNCENTRY( nodemcu, luaN_init )
#endif
  LROT_FUNCENTRY( io, luaopen_io )
  LROT_FUNCENTRY( is, luaopen_os )
LROT_END(lua_libs, NULL, 0)

#else

/* These symbols get created by the linker fragment mapping:nodemcu
 * at the head of the lua libs init table and the rotables, respectively.
 */
extern const ROTable_entry _lua_libs_map_start[];
extern const ROTable_entry _lua_rotables_map_start[];

ROTable rotables_ROTable;   /* NOT const in this case */

LROT_ENTRIES_IN_SECTION(rotables, rotable)
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
  LROT_TABENTRY( math, mathlib )
#endif
#ifdef CONFIG_LUA_BUILTIN_UTF8
  LROT_TABENTRY( utf8, utf8 )
#endif
  LROT_TABENTRY( ROM, rotables )
LROT_BREAK(lua_rotables_part)


LROT_ENTRIES_IN_SECTION(lua_libs, libs)
  LROT_FUNCENTRY( _G, luaopen_base )
  LROT_FUNCENTRY( package, luaopen_package )
#ifdef CONFIG_LUA_BUILTIN_STRING
  LROT_FUNCENTRY( string, luaopen_string )
#endif
#ifdef CONFIG_LUA_BUILTIN_IO
  LROT_FUNCENTRY( io, luaopen_io )
#endif
  LROT_FUNCENTRY( nodemcu, luaN_init )
LROT_BREAK(lua_libs)

#endif

/* Historically the linker script took care of zero terminating both the
 * lua_libs and lua_rotable arrays, but the IDF currently does not
 * support injecting LONG(0) entries. To compensate, we have explicit
 * end markers here which the linker fragment then place appropriately
 * to terminate aforementioned arrays.
 */
const ROTable_entry LOCK_IN_SECTION(libs_end_marker) libs_end_marker= { 0, };
const ROTable_entry LOCK_IN_SECTION(rotable_end_marker) rotable_end_marker = { 0, };

void luaL_openlibs (lua_State *L) {
#ifdef LUA_CROSS_COMPILER
  const ROTable_entry *p = LROT_TABLEREF(lua_libs)->entry;
#else
  const ROTable_entry *p = _lua_libs_map_start;
  lua_createrotable(L, LROT_TABLEREF(rotables), _lua_rotables_map_start, NULL);
#endif
  for ( ; p->key; p++) {
#if LUA_VERSION_NUM == 501
    if (ttislightfunction(&p->value) && fvalue(&p->value)) {
      lua_pushcfunction(L, fvalue(&p->value));
      lua_pushstring(L, p->key);
      lua_call(L, 1, 0);  // call luaopen_XXX(libname)
    }
#else
    if (ttislcf(&p->value) && fvalue(&p->value))
      luaL_requiref(L, p->key, fvalue(&p->value), 1);
#endif
  }
}


#ifndef LUA_CROSS_COMPILER
_Static_assert(_Alignof(ROTable_entry) <= 8, "Unexpected alignment of module registration - update the linker script snippets to match!");
#endif
