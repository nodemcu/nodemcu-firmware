/* Read-only tables for Lua */
#define LUAC_CROSS_FILE

#include "lua.h"
#include <string.h>
#include "lrotable.h"
#include "lauxlib.h"
#include "lstring.h"
#include "lobject.h"
#include "ltm.h"
#include "lapi.h"

#ifdef _MSC_VER
#define ALIGNED_STRING (__declspec( align( 4 ) ) char*)
#else
#define ALIGNED_STRING (__attribute__((aligned(4))) char *)
#endif

#define LA_LINES 32
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
#define HASH(a,b) (unsigned)((((519*(size_t)(a)))>>4) + ((b) ? (b)->tsv.hash: 0))

typedef struct {
  unsigned hash;
  unsigned addr:24;
  unsigned ndx:8;
} cache_line_t;

static cache_line_t cache [LA_LINES][LA_SLOTS];

/*
 * Find a string key entry in a rotable and return it.  Note that this internally
 * uses a null key to denote a metatable search.
 */
const TValue* luaR_findentry(ROTable *rotable, TString *key, unsigned *ppos) {
  const luaR_entry *pentry = rotable;
  const char *strkey = getstr(key);
  unsigned   hash    = HASH(rotable, key);
  cache_line_t *cl = &cache[(hash>>2) & (LA_LINES-1)][0];
  int i, j;
  int l = key->tsv.len;

  if (!pentry)
    return luaO_nilobject;

  /* scan the ROTable lookaside cache and return if hit found */
  for (i=0; i<LA_SLOTS; i++) {
    if (cl[i].hash == hash && ((size_t)rotable & 0xffffffu) == cl[i].addr) {
      int j = cl[i].ndx;
      if (strcmp(pentry[j].key, strkey) == 0) {
        if (ppos)
          *ppos = j;
        return &pentry[j].value;
      }
    }
  }

  lu_int32 name4 = *(lu_int32 *) strkey;
  memcpy(&name4, strkey, sizeof(name4));
 /*
  * A lot of search misses are metavalues, but tables typically only have at
  * most a couple of them, so these are always put at the front of the table
  * in ascending order and the metavalue scan short circuits using a straight
  * strcmp()
  */
  if (*(char*)&name4  == '_') {
    for(j=-1; pentry->key; pentry++) {
      j = strcmp(pentry->key, strkey);
      if (j>=0)
        break;
    }
  } else {
 /*
  * Ordinary (non-meta) keys can be unsorted.  This is for legacy
  * compatiblity, plus misses are pretty rare in this case.
  */   
    lu_int32 mask4 = l > 2 ? (~0u) : (~0u)>>((3-l)*8);
    for(j=1; pentry->key ; pentry++) {
      if (((*(lu_int32 *)pentry->key ^ name4) & mask4) != 0)
        continue;
      j = strcmp(pentry->key, strkey);
      if (j==0)
        break;
    }
  }
  if (j)
    return luaO_nilobject;
  if (ppos)
    *ppos = pentry - rotable;
  /* In the case of a hit, update the lookaside cache */
  for (j = LA_SLOTS-1; j>0; j--)
    cl[j] = cl[j-1];
  cl->hash = hash;
  cl->addr = (size_t) rotable;
  cl->ndx  = pentry - rotable;
  return &pentry->value;
}

/* Find the metatable of a given table */
void* luaR_getmeta(ROTable *rotable) {
  static TString *TS__metatable = NULL;
  if (TS__metatable == NULL) {
    TS__metatable = G(lua_getstate())->tmname[TM_METATABLE];
  }
  const TValue *res = luaR_findentry(rotable, TS__metatable, NULL);
  return res && ttisrotable(res) ? rvalue(res) : NULL;
}

static void luaR_next_helper(lua_State *L, ROTable *pentries, int pos,
                             TValue *key, TValue *val) {
  if (pentries[pos].key) {
    /* Found an entry */
    setsvalue(L, key, luaS_new(L, pentries[pos].key));
    setobj2s(L, val, &pentries[pos].value);
  } else {
    setnilvalue(key);
    setnilvalue(val);
  }
}


/* next (used for iteration) */
void luaR_next(lua_State *L, ROTable *rotable, TValue *key, TValue *val) {
  unsigned keypos;

  /* Special case: if key is nil, return the first element of the rotable */
  if (ttisnil(key))
    luaR_next_helper(L, rotable, 0, key, val);
  else if (ttisstring(key)) {
    /* Find the previous key again */
    if (ttisstring(key)) {
      luaR_findentry(rotable, rawtsvalue(key), &keypos);
    }
    /* Advance to next key */
    keypos ++;
    luaR_next_helper(L, rotable, keypos, key, val);
  }
}
