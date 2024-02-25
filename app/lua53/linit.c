/*
** $Id: linit.c,v 1.39.1.1 2017/04/19 17:20:42 roberto Exp $
** Initialization of libraries for lua.c and other clients
** See Copyright Notice in lua.h
*/


#define linit_c
#define LUA_LIB
#define LUA_CORE

/*
** NodeMCU uses RO segment based static ROTable declarations for library
** tables including the index of library tables itself (the ROM table).
** These tables are moved from RAM to flash ROM on the ESPs.
**
** In the case of ESP firmware builds, explicit control of the loader
** directives "linker magic" allows the marshalling of table entries for
** the master ROM and library initialisation vectors through linker-based
** PSECTs so that the corresponding tables can be bound during the link
** process rather than being statically declared here.  This avoids the
** need to reconfigure this linit.c file to reflect the subset of the total
** modules selected for a given build.  This same mechanism is used to
** include the lbaselib.c functions into the master ROM table.
**
** In contrast the host-based luac.cross builds must link on a range of
** platforms where we don't have control of PSECT placement.  However these
** only use a small fixed list of libraries, which can be defined in this
** linit.c.  This avoids the need for linker magic on host builds and
** simplifies building luac.cross across a range of host toolchains. One
** compilation in this case is that the lbaselib.c functions must be compiled
** into a separate external ROTable which is cascaded into the ROM resolution
** using its metatable __index hook.
*/

#include "lprefix.h"
#include <stddef.h>
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
#include "lstate.h"
#include "lnodemcu.h"

extern LROT_TABLE(strlib);
extern LROT_TABLE(tab_funcs);
extern LROT_TABLE(dblib);
extern LROT_TABLE(co_funcs);
extern LROT_TABLE(mathlib);
extern LROT_TABLE(utf8);

#define LROT_ROM_ENTRIES \
  LROT_TABENTRY( string, strlib ) \
  LROT_TABENTRY( table, tab_funcs ) \
  LROT_TABENTRY( debug, dblib) \
  LROT_TABENTRY( coroutine, co_funcs ) \
  LROT_TABENTRY( math, mathlib ) \
  LROT_TABENTRY( utf8, utf8 ) \
  LROT_TABENTRY( ROM, rotables )

#define LROT_LIB_ENTRIES \
  LROT_FUNCENTRY( _G, luaopen_base ) \
  LROT_FUNCENTRY( package, luaopen_package ) \
  LROT_FUNCENTRY( string, luaopen_string ) \
  LROT_FUNCENTRY( nodemcu, luaN_init )
 /*
  * Note that this nodemcu entry isn't a normal library initialisaiton but
  * instead is a hook to allow the loading of a new LFS.  This load process
  * needs base and string to be initialised but not the untrustworthy
  * modules and so is slotted in here.
  */

#if defined(LUA_CROSS_COMPILER)

/* _G __index -> rotables __index -> base_func */
#define LUAC_MODULE(map) \
  LUALIB_API LROT_TABLE(map);

#define LUAC_MODULE_INIT(map, initfunc) \
  LUAC_MODULE(map);\
  LUALIB_API int initfunc(lua_State *L);

LUAC_MODULE(thislib) // module struct
LUAC_MODULE(bit)
LUAC_MODULE(color_utils)
LUAC_MODULE_INIT(sjson, luaopen_sjson)
LUAC_MODULE(pipe)
LUAC_MODULE_INIT(pixbuf, luaopen_pixbuf)

LUAC_MODULE(rotables_meta);
LUAC_MODULE(base_func);
LROT_BEGIN(rotables_meta, NULL, LROT_MASK_INDEX)
  LROT_TABENTRY( __index, base_func)
LROT_END(rotables_meta, NULL, LROT_MASK_INDEX)

LROT_BEGIN(rotables, LROT_TABLEREF(rotables_meta), 0)
  LROT_TABENTRY( _G, base_func)
  LROT_ROM_ENTRIES
  // modules
  LROT_TABENTRY( struct, thislib )
  LROT_TABENTRY(bit, bit)
  LROT_TABENTRY(color_utils, color_utils)
  LROT_TABENTRY(sjson, sjson)
  LROT_TABENTRY(pipe, pipe)
  LROT_TABENTRY(pixbuf, pixbuf)
LROT_END(rotables, LROT_TABLEREF(rotables_meta), 0)

LROT_BEGIN(lua_libs, NULL, 0)
  LROT_LIB_ENTRIES
  LROT_FUNCENTRY( io, luaopen_io )
  LROT_FUNCENTRY( os, luaopen_os )
  // modules
  LROT_FUNCENTRY(struct, NULL)
  LROT_FUNCENTRY(bit, NULL)
  LROT_FUNCENTRY(color_utils, NULL)
  LROT_FUNCENTRY(sjson, luaopen_sjson)
  LROT_FUNCENTRY(pipe, NULL)
  LROT_FUNCENTRY(pixbuf, luaopen_pixbuf)
LROT_END(lua_libs, NULL, 0)

#else /* LUA_USE_ESP */

/* _G __index -> rotables __index (rotables includes base_func) */
extern const ROTable_entry lua_libs_base[];
extern const ROTable_entry lua_rotable_base[];

ROTable rotables_ROTable;   /* NOT const in this case */

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
  /* Now do lua opens */
  for ( ; p->key; p++) {
    if (ttislcf(&p->value) && fvalue(&p->value))
      luaL_requiref(L, p->key, fvalue(&p->value), 1);
  }
}
