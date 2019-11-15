/*
** $Id: lstring.c,v 2.56.1.1 2017/04/19 17:20:42 roberto Exp $
** String table (keeps all strings handled by Lua)
** See Copyright Notice in lua.h
*/

#define lstring_c
#define LUA_CORE

#include "lprefix.h"


#include <string.h>

#include "lua.h"

#include "ldebug.h"
#include "ldo.h"
#include "lmem.h"
#include "lobject.h"
#include "lstate.h"
#include "lstring.h"


#define MEMERRMSG       "not enough memory"

/*
** Lua will use at most ~(2^LUAI_HASHLIMIT) bytes from a string to
** compute its hash
*/
#if !defined(LUAI_HASHLIMIT)
#define LUAI_HASHLIMIT		5
#endif


/*
** equality for long strings
*/
int luaS_eqlngstr (TString *a, TString *b) {
  size_t len = a->u.lnglen;
  lua_assert(gettt(a) == LUA_TLNGSTR && gettt(b) == LUA_TLNGSTR);
  return (a == b) ||  /* same instance or... */
    ((len == b->u.lnglen) &&  /* equal length and ... */
     (memcmp(getstr(a), getstr(b), len) == 0));  /* equal contents */
}


unsigned int luaS_hash (const char *str, size_t l, unsigned int seed) {
  unsigned int h = seed ^ cast(unsigned int, l);
  size_t step = (l >> LUAI_HASHLIMIT) + 1;
  for (; l >= step; l -= step)
    h ^= ((h<<5) + (h>>2) + cast_byte(str[l - 1]));
  return h;
}


unsigned int luaS_hashlongstr (TString *ts) {
  lua_assert(ts->tt == LUA_TLNGSTR);
  if (getextra(ts) == 0) {  /* no hash? */
    ts->hash = luaS_hash(getstr(ts), ts->u.lnglen, ts->hash);
    ts->extra = 1;  /* now it has its hash */
  }
  return ts->hash;
}


/*
** resizes the string table
*/
void luaS_resize (lua_State *L, int newsize) {
  int i;
//***FIX*** rentrancy guard during GC
  stringtable *tb = &G(L)->strt;
  if (newsize > tb->size) {  /* grow table if needed */
    luaM_reallocvector(L, tb->hash, tb->size, newsize, TString *);
    for (i = tb->size; i < newsize; i++)
      tb->hash[i] = NULL;
  }
  for (i = 0; i < tb->size; i++) {  /* rehash */
    TString *p = tb->hash[i];
    tb->hash[i] = NULL;
    while (p) {  /* for each node in the list */
      TString *hnext = p->u.hnext;  /* save next */
      unsigned int h = lmod(p->hash, newsize);  /* new position */
      p->u.hnext = tb->hash[h];  /* chain it */
      tb->hash[h] = p;
      p = hnext;
    }
  }
  if (newsize < tb->size) {  /* shrink table if needed */
    /* vanishing slice should be empty */
    lua_assert(tb->hash[newsize] == NULL && tb->hash[tb->size - 1] == NULL);
    luaM_reallocvector(L, tb->hash, tb->size, newsize, TString *);
  }
  tb->size = newsize;
}


#define STRING_ENTRY(e) (cast(KeyCache,((size_t)(e)) & 1));
/*
** Initialize the string table and the key cache
*/
void luaS_init (lua_State *L) {
  global_State *g = G(L);
  int i, j;
  luaS_resize(L, MINSTRTABSIZE);  /* initial size of string table */
  /* pre-create memory-error message */
  g->memerrmsg = luaS_newliteral(L, MEMERRMSG);
  luaC_fix(L, obj2gco(g->memerrmsg));  /* it should never be collected */

  /* Initialise the global cache to dummy string entries */
  for (i = 0; i < KEYCACHE_N; i++) {
    KeyCache *p = g->cache[i];
    for (j = 0;j < KEYCACHE_M; j++)
      p[j] = STRING_ENTRY(g->memerrmsg);
  }
}



/*
** creates a new string object
*/
static TString *createstrobj (lua_State *L, size_t l, int tag, unsigned int h) {
  TString *ts;
  GCObject *o;
  size_t totalsize;  /* total size of TString object */
  totalsize = sizelstring(l);
  o = luaC_newobj(L, tag, totalsize);
  ts = gco2ts(o);
  ts->hash = h;
  ts->extra = 0;
  getstr(ts)[l] = '\0';  /* ending 0 */
  return ts;
}


TString *luaS_createlngstrobj (lua_State *L, size_t l) {
  TString *ts = createstrobj(L, l, LUA_TLNGSTR, G(L)->seed);
  ts->u.lnglen = l;
  return ts;
}


void luaS_remove (lua_State *L, TString *ts) {
  stringtable *tb = &G(L)->strt;
  TString **p = &tb->hash[lmod(ts->hash, tb->size)];
  while (*p != ts)  /* find previous element */
    p = &(*p)->u.hnext;
  *p = (*p)->u.hnext;  /* remove element from its list */
  tb->nuse--;
}


/*
** checks whether short string exists and reuses it or creates a new one
*/
static TString *internshrstr (lua_State *L, const char *str, size_t l) {
  TString *ts;
  global_State *g = G(L);
  unsigned int h = luaS_hash(str, l, g->seed);
  TString **list = &g->strt.hash[lmod(h, g->strt.size)];
  lua_assert(str != NULL);  /* otherwise 'memcmp'/'memcpy' are undefined */
  for (ts = *list; ts != NULL; ts = ts->u.hnext) {
    if (l == getshrlen(ts) &&
        (memcmp(str, getstr(ts), l * sizeof(char)) == 0)) {
      /* found! */
      if (isdead(g, ts))  /* dead (but not collected yet)? */
        changewhite(ts);  /* resurrect it */
      return ts;
    }
  }
  /*
   * The RAM strt is searched first since RAM access is faster than flash
   * access.  If a miss, then search the RO string table.
   */
  if (g->ROstrt.hash) {
    for (ts = g->ROstrt.hash[lmod(h, g->ROstrt.size)];
         ts != NULL;
         ts = ts->u.hnext)     {
      if (l == getshrlen(ts) &&
          memcmp(str, getstr(ts), l * sizeof(char)) == 0) {
      /* found in ROstrt! */
        return ts;
      }
    }
  }
  if (g->strt.nuse >= g->strt.size && g->strt.size <= MAX_INT/2) {
    luaS_resize(L, g->strt.size * 2);
    list = &g->strt.hash[lmod(h, g->strt.size)];  /* recompute with new size */
  }
  ts = createstrobj(L, l, LUA_TSHRSTR, h);
  memcpy(getstr(ts), str, l * sizeof(char));
  ts->shrlen = cast_byte(l);
  ts->u.hnext = *list;
  *list = ts;
  g->strt.nuse++;
  return ts;
}


/*
** new string (with explicit length)
*/
TString *luaS_newlstr (lua_State *L, const char *str, size_t l) {
  if (l <= LUAI_MAXSHORTLEN)  /* short string? */
    return internshrstr(L, str, l);
  else {
    TString *ts;
    if (l >= (MAX_SIZE - sizeof(TString))/sizeof(char))
      luaM_toobig(L);
    ts = luaS_createlngstrobj(L, l);
    memcpy(getstr(ts), str, l * sizeof(char));
    return ts;
  }
}


/*
** Create or reuse a zero-terminated string, If the null terminated
** length > sizeof (unisigned) then first check the cache (using the
** string address as a key).  The cache can contain only zero-
** terminated strings, so it is safe to use 'strcmp' to check hits.
**
** Note that the cache contains both TStrings and Tables entries but
** both of these addresses word are always aligned, so the address is
** a mulitple of size_t.  The lowbit of the address in the cache is
** overwritten with a boolean to tag TString entries
*/

#define IS_STRING_ENTRY(e) (e & 1)
#define TSTRING(e) cast(TString *, ((size_t) e) & (~1u))

TString *luaS_new (lua_State *L, const char *str) {
  unsigned int i = point2uint(str) % KEYCACHE_N;  /* hash */
  int j;
  TString *ps;
  KeyCache *p = G(L)->cache[i];

  for (j = 0; j < KEYCACHE_M; j++) {
    ps = TSTRING(p[j]);
    /* string cache entries always point to a valid TString */
    if (IS_STRING_ENTRY(p[j]) && strcmp(str, getstr(ps)) == 0)  /* hit? */
      return ps;  /* that is it */
  }
  /* normal route, move out last element inserting new string at fist slot */
  for (j = KEYCACHE_M - 1; j > 0; j--) {
    p[j] = p[j-1];
  }
  ps = luaS_newlstr(L, str, strlen(str));
  p[0] = STRING_ENTRY(ps);
  return ps;
}

/*
** Clear API cache of dirty string entries.
*/
void luaS_clearcache (global_State *g) {
  int i, j, k;
  TString *ps;
  for (i = 0; i < KEYCACHE_N; i++) {
    KeyCache *p = g->cache[i];
    for (j = 0, k = 0; j < KEYCACHE_M; j++) {
      ps = TSTRING(p[j]);
      if (!IS_STRING_ENTRY(p[j]) || !iswhite(cast(GCObject *,ps))) { /* keep entry? */
        if (k < j)
          p[k] = p[j];  /* shift down element */
        k++;
      }
    }
    for (;k < KEYCACHE_M; k++)
      p[k] = STRING_ENTRY(g->memerrmsg);
  }
}

Udata *luaS_newudata (lua_State *L, size_t s) {
  Udata *u;
  GCObject *o;
  if (s > MAX_SIZE - sizeof(Udata))
    luaM_toobig(L);
  o = luaC_newobj(L, LUA_TUSERDATA, sizeludata(s));
  u = gco2u(o);
  u->len = s;
  u->metatable = NULL;
  setuservalue(L, u, luaO_nilobject);
  return u;
}

