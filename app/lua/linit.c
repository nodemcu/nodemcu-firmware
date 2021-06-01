/*
** $Id: linit.c,v 1.14.1.1 2007/12/27 13:02:25 roberto Exp $
** Initialization of libraries for lua.c
** See Copyright Notice in lua.h
*/


#define linit_c
#define LUA_LIB

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

extern LROT_TABLE(strlib);
extern LROT_TABLE(tab_funcs);
extern LROT_TABLE(dblib);
extern LROT_TABLE(co_funcs);
extern LROT_TABLE(math);

#define LROT_ROM_ENTRIES \
  LROT_TABENTRY( string, strlib ) \
  LROT_TABENTRY( table, tab_funcs ) \
  LROT_TABENTRY( debug, dblib) \
  LROT_TABENTRY( coroutine, co_funcs ) \
  LROT_TABENTRY( math, math ) \
  LROT_TABENTRY( ROM, rotables )

#define LROT_LIB_ENTRIES \
  LROT_FUNCENTRY( _G, luaopen_base ) /* This MUST be called first */ \
  LROT_FUNCENTRY( package, luaopen_package ) \
  LROT_FUNCENTRY( string, luaopen_string ) \
  LROT_FUNCENTRY( debug, luaopen_debug )

#if defined(LUA_CROSS_COMPILER)

#define LUAC_MODULE(map) \
  LUALIB_API LROT_TABLE(map);

#define LUAC_MODULE_INIT(map, initfunc) \
  LUAC_MODULE(map);\
  LUALIB_API int initfunc(lua_State *L);

#ifndef __MINGW32__
LUAC_MODULE(thislib) // module struct
LUAC_MODULE(bit)
LUAC_MODULE(color_utils)
LUAC_MODULE_INIT(sjson, luaopen_sjson)
LUAC_MODULE(pipe)
#ifndef _MSC_VER 
LUAC_MODULE_INIT(pixbuf, luaopen_pixbuf)
#endif
#endif

LUAC_MODULE(base_func);
LROT_BEGIN(rotables_meta, NULL, LROT_MASK_INDEX)
  LROT_TABENTRY( __index, base_func)
LROT_END(rotables_meta, NULL, LROT_MASK_INDEX)

LUAC_MODULE(oslib);
LUAC_MODULE(iolib);
LROT_BEGIN(rotables, LROT_TABLEREF(rotables_meta), 0)
  LROT_ROM_ENTRIES
  LROT_TABENTRY( os, oslib )
  LROT_TABENTRY( io, iolib )
#ifndef __MINGW32__
  // modules
  LROT_TABENTRY( struct, thislib )
  LROT_TABENTRY(bit, bit)
  LROT_TABENTRY(color_utils, color_utils)
  LROT_TABENTRY(sjson, sjson)
  LROT_TABENTRY(pipe, pipe)
#ifndef _MSC_VER 
  LROT_TABENTRY(pixbuf, pixbuf)
#endif
#endif
LROT_END(rotables, LROT_TABLEREF(rotables_meta), 0)

LROT_BEGIN(lua_libs, NULL, 0)
  LROT_LIB_ENTRIES
  LROT_FUNCENTRY( io, luaopen_io )
#ifndef __MINGW32__
  // modules
  LROT_FUNCENTRY(struct, NULL)
  LROT_FUNCENTRY(bit, NULL)
  LROT_FUNCENTRY(color_utils, NULL)
  LROT_FUNCENTRY(sjson, luaopen_sjson)
  LROT_FUNCENTRY(pipe, NULL)
#ifndef _MSC_VER 
  LROT_FUNCENTRY(pixbuf, luaopen_pixbuf)
#endif
#endif
LROT_END(lua_libs, NULL, 0)

#else

extern const ROTable_entry lua_libs_base[];
extern const ROTable_entry lua_rotable_base[];
ROTable rotables_ROTable;

LROT_ENTRIES_IN_SECTION(rotables, rotable)
  LROT_ROM_ENTRIES
LROT_BREAK(rotables)

LROT_ENTRIES_IN_SECTION(lua_libs, libs)
  LROT_LIB_ENTRIES
LROT_BREAK(lua_libs)

#endif


void luaL_openlibs (lua_State *L) {
#ifdef LUA_CROSS_COMPILER
  const ROTable_entry *p = LROT_TABLEREF(lua_libs)->entry;
#else
  const ROTable_entry *p = lua_libs_base;
  lua_createrotable(L, LROT_TABLEREF(rotables), lua_rotable_base, NULL);
#endif
  while (p->key) {
    if (ttislightfunction(&p->value) && fvalue(&p->value)) {
      lua_pushcfunction(L, fvalue(&p->value));
      lua_pushstring(L, p->key);
      lua_call(L, 1, 0);  // call luaopen_XXX(libname)
    }
    p++;
  }
}
