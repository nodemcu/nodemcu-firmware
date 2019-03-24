/* Read-only tables for Lua */
#define LUAC_CROSS_FILE

#include "lua.h"
#include C_HEADER_STRING
#include "lrotable.h"
#include "lauxlib.h"
#include "lstring.h"
#include "lobject.h"
#include "lapi.h"

#ifdef _MSC_VER
#define ALIGNED_STRING (__declspec( align( 4 ) ) char*)
#else
#define ALIGNED_STRING (__attribute__((aligned(4))) char *)
#endif
#define LA_LINES 16
#define LA_SLOTS 4
//#define COLLECT_STATS

/*
 * All keyed ROtable access passes through luaR_findentry().  ROTables
 * are simply a list of <key><TValue value> pairs.  The existing algo
 * did a linear scan of this vector of pairs looking for a match.
 *
 * A N×M lookaside cache has been added, with a simple hash on the key's
 * TString addr and the ROTable addr to identify one of N lines.  Each
 * line has M slots which are scanned. This is all done in RAM and is
 * perhaps 20x faster than the corresponding random Flash accesses which
 * will cause flash faults.
 *
 * If a match is found and the table addresses match, then this entry is
 * probed first. In practice the hit-rate here is over 99% so the code
 * rarely fails back to doing the linear scan in ROM.
 *
 * Note that this hash does a couple of prime multiples and a modulus 2^X
 * with is all evaluated in H/W, and adequately randomizes the lookup.
 */
#define HASH(a,b) (519*((size_t)(a)>>4) + 17*((size_t)(b)>>4))

static struct {
  unsigned hash;
  unsigned addr:24;
  unsigned ndx:8;
} cache[LA_LINES][LA_SLOTS];

#ifdef COLLECT_STATS
unsigned cache_stats[3];
#define COUNT(i) cache_stats[i]++
#else
#define COUNT(i)
#endif

static int lookup_cache(unsigned hash, ROTable *rotable) {
  int i = (hash>>2) & (LA_LINES-1), j;

  for (j = 0; j<LA_SLOTS; j++) {
    if (cache[i][j].hash == hash &&
        ((size_t)rotable & 0xffffffu) == cache[i][j].addr) {
      COUNT(0);
      return cache[i][j].ndx;
    }
  }
  COUNT(1);
  return -1;
}

static void update_cache(unsigned hash, ROTable *rotable, unsigned ndx) {
  int i = (hash)>>2 & (LA_LINES-1), j;
  COUNT(2);
  if (ndx>0xffu)
    return;
  for (j = LA_SLOTS-1; j>0; j--)
    cache[i][j] = cache[i][j-1];
  cache[i][0].hash = hash;
  cache[i][0].addr = (size_t) rotable;
  cache[i][0].ndx  = ndx;
}
/*
 * Find a string key entry in a rotable and return it.  Note that this internally
 * uses a null key to denote a metatable search.
 */
const TValue* luaR_findentry(ROTable *rotable, TString *key, unsigned *ppos) {
  const luaR_entry *pentry = rotable;
  const char *strkey = key ? getstr(key) : ALIGNED_STRING "__metatable" ;
  size_t      hash   = HASH(rotable, key);
  unsigned    i      = 0;
  int         j      = lookup_cache(hash, rotable);
  unsigned    l      = key ? key->tsv.len : sizeof("__metatable")-1;

  if (pentry) {
    if (j >= 0){
      if ((pentry[j].key.type == LUA_TSTRING) &&
        !c_strcmp(pentry[j].key.id.strkey, strkey)) {
        if (ppos)
          *ppos = j;
        return &pentry[j].value;
      }
    }
    /*
     * The invariants for 1st word comparison are deferred to here since they
     * aren't needed if there is a cache hit. Note that the termination null
     * is included so a "on\0" has a mask of 0xFFFFFF and "a\0" has 0xFFFF.
     */
    unsigned name4, mask4 = l > 2 ? (~0u) : (~0u)>>((3-l)*8);
    c_memcpy(&name4, strkey, sizeof(name4));

    for(;pentry->key.type != LUA_TNIL; i++, pentry++) {
      if ((pentry->key.type == LUA_TSTRING) &&
          ((*(unsigned *)pentry->key.id.strkey ^ name4) & mask4) == 0 &&
          !c_strcmp(pentry->key.id.strkey, strkey)) {
        if (ppos)
          *ppos = i;
        if (j==-1) {
          update_cache(hash, rotable, pentry - rotable);
        } else if (j != (pentry - rotable)) {
          j = 0;
        }
        return &pentry->value;
      }
    }
  }
  return luaO_nilobject;
}

const TValue* luaR_findentryN(ROTable *rotable, luaR_numkey numkey, unsigned *ppos) {
  unsigned i = 0;
  const luaR_entry *pentry = rotable;
  if (pentry) {
    for ( ;pentry->key.type != LUA_TNIL; i++, pentry++) {
      if (pentry->key.type == LUA_TNUMBER && (luaR_numkey) pentry->key.id.numkey == numkey) {
        if (ppos)
          *ppos = i;
        return &pentry->value;
      }
    }
  }
  return NULL;
}


/* Find the metatable of a given table */
void* luaR_getmeta(ROTable *rotable) {
  const TValue *res = luaR_findentry(rotable, NULL, NULL);
  return res && ttisrotable(res) ? rvalue(res) : NULL;
}

static void luaR_next_helper(lua_State *L, ROTable *pentries, int pos,
                             TValue *key, TValue *val) {
  setnilvalue(key);
  setnilvalue(val);
  if (pentries[pos].key.type != LUA_TNIL) {
    /* Found an entry */
    if (pentries[pos].key.type == LUA_TSTRING)
      setsvalue(L, key, luaS_new(L, pentries[pos].key.id.strkey))
    else
      setnvalue(key, (lua_Number)pentries[pos].key.id.numkey)
   setobj2s(L, val, &pentries[pos].value);
  }
}


/* next (used for iteration) */
void luaR_next(lua_State *L, ROTable *rotable, TValue *key, TValue *val) {
  unsigned keypos;

  /* Special case: if key is nil, return the first element of the rotable */
  if (ttisnil(key))
    luaR_next_helper(L, rotable, 0, key, val);
  else if (ttisstring(key) || ttisnumber(key)) {
    /* Find the previous key again */
    if (ttisstring(key)) {
      luaR_findentry(rotable, rawtsvalue(key), &keypos);
    } else {
      luaR_findentryN(rotable, (luaR_numkey)nvalue(key), &keypos);
    }
    /* Advance to next key */
    keypos ++;
    luaR_next_helper(L, rotable, keypos, key, val);
  }
}
