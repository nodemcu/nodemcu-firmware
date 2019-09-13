/*
 * NodeMCU extensions to Lua for readonly Flash memory support
 */
#ifndef lnodemcu_h
#define lnodemcu_h

#include "lua.h"
#include "lobject.h"
#include "llimits.h"
#include "ltm.h"

#ifdef LUA_USE_HOST
#define LRO_STRKEY(k)   k
#define LOCK_IN_SECTION(s)
#else
#define LRO_STRKEY(k)   ((STORE_ATTR char *) k)
#define LOCK_IN_SECTION(s) __attribute__((used,unused,section(".lua_" #s)))
#endif

/* Macros used to declare rotable entries */

#define LRO_FUNCVAL(v)       {{.f = v}, LUA_TLCF}
#define LRO_LUDATA(v)        {{.p = (void *) v}, LUA_TLIGHTUSERDATA}
#define LRO_NILVAL           {{.p = NULL}, LUA_TNIL}
#define LRO_NUMVAL(v)        {{.i = v}, LUA_TNUMINT}
#define LRO_INTVAL(v)        LRO_NUMVAL(v)
#define LRO_FLOATVAL(v)      {{.n = v}, LUA_TNUMFLT}
#define LRO_ROVAL(v)         {{.gc = cast(GCObject *, &(v ## _ROTable))}, LUA_TTBLROF}

#define LROT_MARKED          0 //<<<<<<<<<<*** TBD *** >>>>>>>>>>>

#define LROT_FUNCENTRY(n,f)  {LRO_STRKEY(#n), LRO_FUNCVAL(f)},
#define LROT_LUDENTRY(n,x)   {LRO_STRKEY(#n), LRO_LUDATA(x)},
#define LROT_NUMENTRY(n,x)   {LRO_STRKEY(#n), LRO_NUMVAL(x)},
#define LROT_INTENTRY(n,x)   LROT_NUMENTRY(n,x)
#define LROT_FLOATENTRY(n,x) {LRO_STRKEY(#n), LRO_FLOATVAL(x)},
#define LROT_TABENTRY(n,t)   {LRO_STRKEY(#n), LRO_ROVAL(t)},

#define LROT_TABLE(rt)       const ROTable rt ## _ROTable
#define LROT_ENTRYREF(rt)    (rt ##_entries)
#define LROT_TABLEREF(rt)    (&rt ##_ROTable)
#define LROT_BEGIN(rt,mt,f)  LROT_TABLE(rt); \
  static const ROTable_entry rt ## _entries[] = {
#define LROT_ENTRIES_IN_SECTION(rt,s) \
  static const ROTable_entry LOCK_IN_SECTION(s) rt ## _entries[] = {
#define LROT_END(rt,mt,f)    {NULL, LRO_NILVAL} }; \
  const ROTable rt ## _ROTable = { \
    (GCObject *)1,LUA_TTBLROF, LROT_MARKED, \
    cast(lu_byte, ~(f)), (sizeof(rt ## _entries)/sizeof(ROTable_entry)) - 1, \
    cast(Table *, mt), cast(ROTable_entry *, rt ## _entries) };
#define LROT_BREAK(rt)       };

#define LROT_MASK(m)         cast(lu_byte, 1<<TM_ ## m)

/*
 * These are statically coded can be any combination of the fast index tags
 * listed in ltm.h: EQ, GC, INDEX, LEN, MODE, NEWINDEX or combined by anding
 * GC+INDEX is the only common combination used, hence the combinaton macro
 */
#define LROT_MASK_EQ         LROT_MASK(EQ)
#define LROT_MASK_GC         LROT_MASK(GC)
#define LROT_MASK_INDEX      LROT_MASK(INDEX)
#define LROT_MASK_LEN        LROT_MASK(LEN)
#define LROT_MASK_MODE       LROT_MASK(MODE)
#define LROT_MASK_NEWINDEX   LROT_MASK(NEWINDEX)
#define LROT_MASK_GC_INDEX   (LROT_MASK_GC | LROT_MASK_INDEX)

/* Maximum length of a rotable name and of a string key*/
#define LUA_MAX_ROTABLE_NAME 32

#if 0
#include "lobject.h"
#include "lstate.h"
#include "lzio.h"

#ifdef LUA_NUMBER_INTEGRAL
# define FLASH_SIG_B1 0x02
#else
# define FLASH_SIG_B1 0x00
#endif
#define FLASH_FORMAT_VERSION (1 << 8)
#define FLASH_FORMAT_MASK    0xF00
#ifdef LUA_PACK_TVALUES
#ifdef LUA_NUMBER_INTEGRAL
#error "LUA_PACK_TVALUES is only valid for Floating point builds"
#endif
# define FLASH_SIG_B2 0x04
#else
# define FLASH_SIG_B2 0x00
#endif
# define FLASH_SIG_B2_MASK 0x04
#define FLASH_SIG_ABSOLUTE    0x01
#define FLASH_SIG_IN_PROGRESS 0x08
#define FLASH_SIG  (0xfafaa050 | FLASH_FORMAT_VERSION |FLASH_SIG_B2 | FLASH_SIG_B1)

typedef lu_int32 FlashAddr;
typedef struct {
  lu_int32  flash_sig;      /* a stabdard fingerprint identifying an LFS image */
  lu_int32  flash_size;     /* Size of LFS image */
  FlashAddr mainProto;      /* address of main Proto in Proto hierarchy */
  FlashAddr pROhash;        /* address of ROstrt hash */
  lu_int32  nROuse;         /* number of elements in ROstrt */
  int       nROsize;        /* size of ROstrt */
  lu_int32  fill1;          /* reserved */
  lu_int32  fill2;          /* reserved */
} FlashHeader;

LUAI_FUNC void luaN_init (lua_State *L);
LUAI_FUNC int  luaN_flashSetup (lua_State *L);
#endif
#endif
