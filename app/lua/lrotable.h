/* Read-only tables for Lua */

#ifndef lrotable_h
#define lrotable_h

#include "lua.h"
#include "luaconf.h"
#include "lobject.h"
#include "llimits.h"

/* Macros one can use to define rotable entries */
#define LRO_FUNCVAL(v)  {{.p = v}, LUA_TLIGHTFUNCTION}
#define LRO_LUDATA(v)   {{.p = v}, LUA_TLIGHTUSERDATA}
#define LRO_NUMVAL(v)   {{.n = v}, LUA_TNUMBER}
#define LRO_ROVAL(v)    {{.p = (void*)v}, LUA_TROTABLE}
#define LRO_NILVAL      {{.p = NULL}, LUA_TNIL}
#ifdef LUA_CROSS_COMPILER
#define LRO_STRKEY(k)   {LUA_TSTRING, {.strkey = k}}
#else
#define LRO_STRKEY(k)   {LUA_TSTRING, {.strkey = (STORE_ATTR char *) k}}
#endif
#define LRO_NUMKEY(k)   {LUA_TNUMBER, {.numkey = k}}
#define LRO_NILKEY      {LUA_TNIL, {.strkey=NULL}}

/* Maximum length of a rotable name and of a string key*/
#define LUA_MAX_ROTABLE_NAME      32

/* Type of a numeric key in a rotable */
typedef int luaR_numkey;

/* The next structure defines the type of a key */
typedef struct {
  int type;
  union {
    const char*   strkey;
    luaR_numkey   numkey;
  } id;
} luaR_key;

/* An entry in the read only table */
typedef struct luaR_entry {
  const luaR_key key;
  const TValue value;
} luaR_entry;

/*
 * The current ROTable implmentation is a vector of luaR_entry terminated by a
 * nil record.  The convention is to use ROtable * to refer to the entire vector
 * as a logical ROTable.
 */
typedef const struct luaR_entry ROTable;

const TValue* luaR_findentry(ROTable *tab, TString *key, unsigned *ppos);
const TValue* luaR_findentryN(ROTable *tab, luaR_numkey numkey, unsigned *ppos);
void luaR_next(lua_State *L, ROTable *tab, TValue *key, TValue *val);
void* luaR_getmeta(ROTable *tab);
int luaR_isrotable(void *p);

/*
 * Set inRO check depending on platform. Note that this implementation needs
 * to work on both the host (luac.cross) and ESP targets.  The luac.cross
 * VM is used for the -e option, and is primarily used to be able to debug
 * VM changes on the more developer-friendly hot gdb environment.
 */
#if defined(LUA_CROSS_COMPILER)

#if defined(_MSC_VER)
//msvc build uses these dummy vars to locate the beginning and ending addresses of the RO data
extern const char _ro_start[], _ro_end[];
#define IN_RODATA_AREA(p) (((const char*)(p)) >= _ro_start && ((const char *)(p)) <= _ro_end)
#else /* one of the POSIX variants */
#if defined(__CYGWIN__)
#define _RODATA_END __end__
#elif defined(__MINGW32__)
#define _RODATA_END end
#else
#define _RODATA_END _edata
#endif
extern const char _RODATA_END[];
#define IN_RODATA_AREA(p) (((const char *)(p)) < _RODATA_END)
#endif /* defined(_MSC_VER) */

#else  /* xtensa tool chain for ESP target */

extern const char _irom0_text_start[];
extern const char _irom0_text_end[];
#define IN_RODATA_AREA(p) (((const char *)(p)) >= _irom0_text_start && ((const char *)(p)) <= _irom0_text_end)

#endif /* defined(LUA_CROSS_COMPILER) */

/* Return 1 if the given pointer is a rotable */
#define luaR_isrotable(p) IN_RODATA_AREA(p)

#endif
