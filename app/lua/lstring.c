/*
** $Id: lstring.c,v 2.8.1.1 2007/12/27 13:02:25 roberto Exp $
** String table (keeps all strings handled by Lua)
** See Copyright Notice in lua.h
*/



#define lstring_c
#define LUA_CORE
#define LUAC_CROSS_FILE

#include "lua.h"
#include C_HEADER_STRING

#include "lmem.h"
#include "lobject.h"
#include "lstate.h"
#include "lstring.h"

#define LUAS_READONLY_STRING      1
#define LUAS_REGULAR_STRING       0

void luaS_resize (lua_State *L, int newsize) {
  stringtable *tb;
  int i;
  tb = &G(L)->strt;
  if (luaC_sweepstrgc(L) || newsize == tb->size || is_resizing_strings_gc(L))
    return;  /* cannot resize during GC traverse or doesn't need to be resized */
  set_resizing_strings_gc(L);
  if (newsize > tb->size) {
    luaM_reallocvector(L, tb->hash, tb->size, newsize, GCObject *);
    for (i=tb->size; i<newsize; i++) tb->hash[i] = NULL;
  }
  /* rehash */
  for (i=0; i<tb->size; i++) {
    GCObject *p = tb->hash[i];
    tb->hash[i] = NULL;
    while (p) {  /* for each node in the list */
      GCObject *next = p->gch.next;  /* save next */
      unsigned int h = gco2ts(p)->hash;
      int h1 = lmod(h, newsize);  /* new position */
      lua_assert(cast_int(h%newsize) == lmod(h, newsize));
      p->gch.next = tb->hash[h1];  /* chain it */
      tb->hash[h1] = p;
      p = next;
    }
  }
  if (newsize < tb->size)
    luaM_reallocvector(L, tb->hash, tb->size, newsize, GCObject *);
  tb->size = newsize;
  unset_resizing_strings_gc(L);
}

static TString *newlstr (lua_State *L, const char *str, size_t l,
                                       unsigned int h, int readonly) {
  TString *ts;
  stringtable *tb;
  if (l+1 > (MAX_SIZET - sizeof(TString))/sizeof(char))
    luaM_toobig(L);
  tb = &G(L)->strt;
  if ((tb->nuse + 1) > cast(lu_int32, tb->size) && tb->size <= MAX_INT/2)
    luaS_resize(L, tb->size*2);  /* too crowded */
  ts = cast(TString *, luaM_malloc(L, sizeof(TString) + (readonly ? sizeof(char**) : (l+1)*sizeof(char))));
  ts->tsv.len = l;
  ts->tsv.hash = h;
  ts->tsv.marked = luaC_white(G(L));
  ts->tsv.tt = LUA_TSTRING;
  if (!readonly) {
    c_memcpy(ts+1, str, l*sizeof(char));
    ((char *)(ts+1))[l] = '\0';  /* ending 0 */
  } else {
    *(char **)(ts+1) = (char *)str;
    l_setbit((ts)->tsv.marked, READONLYBIT);
  }
  h = lmod(h, tb->size);
  ts->tsv.next = tb->hash[h];  /* chain new entry */
  tb->hash[h] = obj2gco(ts);
  tb->nuse++;
  return ts;
}

#ifdef LUA_CROSS_COMPILER
#define IN_RO_AREA(p) (0)
#else /* xtensa tool chain for ESP */
extern char _irom0_text_start[];
extern char _irom0_text_end[];
#define IN_RO_AREA(p) ((p) >= _irom0_text_start && (p) <= _irom0_text_end)
#endif

int lua_is_ptr_in_ro_area(const char *p) {
  return IN_RO_AREA(p);
}

/*
 * The string algorithm has been modified to be LFS-friendly. The previous eLua
 * algo used the address of the string was in flash and the string was >4 bytes 
 * This creates miminal savings and prevents the use of LFS based strings
 */

LUAI_FUNC TString *luaS_newlstr (lua_State *L, const char *str, size_t l) {
  GCObject *o;
  unsigned int h = cast(unsigned int, l);  /* seed */
  size_t step = (l>>5)+1;  /* if string is too long, don't hash all its chars */
  size_t l1;
  for (l1=l; l1>=step; l1-=step)  /* compute hash */
    h = h ^ ((h<<5)+(h>>2)+cast(unsigned char, str[l1-1]));

  for (o = G(L)->strt.hash[lmod(h, G(L)->strt.size)];
       o != NULL;
       o = o->gch.next) {
    TString *ts = rawgco2ts(o);
    if (ts->tsv.len == l && (c_memcmp(str, getstr(ts), l) == 0)) {
      /* string may be dead */
      if (isdead(G(L), o)) changewhite(o);
      return ts;
    }
  }
#if defined(LUA_FLASH_STORE) && !defined(LUA_CROSS_COMPILER)
  /*
   * The RAM strt is searched first since RAM access is faster tham Flash access.
   * If a miss, then search the RO string table.
   */
  if (G(L)->ROstrt.hash) {
    for (o = G(L)->ROstrt.hash[lmod(h, G(L)->ROstrt.size)];
         o != NULL; 
         o = o->gch.next) {
      TString *ts = rawgco2ts(o);
      if (ts->tsv.len == l && (memcmp(str, getstr(ts), l) == 0)) {
        return ts;
      }
    }
  }
#endif
  /* New additions to the RAM strt are tagged as readonly if the string address
   * is in the CTEXT segment (target only, not luac.cross) */
  int readonly = (lua_is_ptr_in_ro_area(str) && l+1 > sizeof(char**) &&
                  l == c_strlen(str) ? LUAS_READONLY_STRING : LUAS_REGULAR_STRING);
  return newlstr(L, str, l, h, readonly);  /* not found */
}


Udata *luaS_newudata (lua_State *L, size_t s, Table *e) {
  Udata *u;
  if (s > MAX_SIZET - sizeof(Udata))
    luaM_toobig(L);
  u = cast(Udata *, luaM_malloc(L, s + sizeof(Udata)));
  u->uv.marked = luaC_white(G(L));  /* is not finalized */
  u->uv.tt = LUA_TUSERDATA;
  u->uv.len = s;
  u->uv.metatable = NULL;
  u->uv.env = e;
  /* chain it on udata list (after main thread) */
  u->uv.next = G(L)->mainthread->next;
  G(L)->mainthread->next = obj2gco(u);
  return u;
}

