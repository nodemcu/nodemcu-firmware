/*
** $Id: ldump.c,v 2.37.1.1 2017/04/19 17:20:42 roberto Exp $
** save precompiled Lua chunks
** See Copyright Notice in lua.h
*/

#define ldump_c
#define LUA_CORE

#include "lprefix.h"


#include <stddef.h>

#include "lua.h"
#include "lapi.h"
#include "lauxlib.h"
#include "llex.h"
#include "lgc.h"
#include "lobject.h"
#include "lstate.h"
#include "lstring.h"
#include "ltable.h"
#include "lundump.h"


typedef struct {
  lua_State *L;
  lua_Writer writer;
  void *data;
  int strip;
  int status;
#ifdef LUA_USE_HOST
  int useStrRefs;
  Table *stringIndex;
  int sTScnt;
  int lTScnt;
  int nFixed;
#endif
} DumpState;

/*
** To ensure that dump files are loadable onto the ESP architectures:
** 1.  Integers are in the range -2^31 .. 2^31-1  (sint32_t)
** 2.  Floats are the IEEE 4 or 8 byte format, with a 4 byte default.
**
** The file formats are also different to standard because of two add
** additional goals:
** 3.  The file must be serially loadable into a programmable flash
**     memory through a file-write like API call.
** 4.  Compactness of dump files is a key design goal.
*/

#define DumpVector(v,n,D)	DumpBlock(v,(n)*sizeof((v)[0]),D)

#define DumpLiteral(s,D)	DumpBlock(s, sizeof(s) - sizeof(char), D)


static void DumpBlock (const void *b, size_t size, DumpState *D) {
  if (D->status == 0 && size > 0) {
    lua_unlock(D->L);
    D->status = (*D->writer)(D->L, b, size, D->data);
    lua_lock(D->L);
  }
}


#define DumpVar(x,D)		DumpVector(&x,1,D)


static void DumpByte (lu_byte x, DumpState *D) {
  DumpVar(x, D);
}

/*
** Dump (unsigned) int 0..MAXINT using multibyte encoding (MBE). DumpInt
** is used for context dependent counts and sizes; no type information
** is embedded.
*/
static void DumpInt (lua_Integer x, DumpState *D) {
  lu_byte buf[sizeof(lua_Integer) + 2];
  lu_byte *b = buf + sizeof(buf) - 1;
  lua_assert(x>=0);
  *b-- = x & 0x7f;  x >>= 7;
  while(x) { *b-- = 0x80 + (x & 0x7f); x >>= 7; }
  b++;
  lua_assert (b >= buf);
  DumpVector(b, (buf - b) + sizeof(buf), D);
}


static void DumpNumber (lua_Number x, DumpState *D) {
  DumpByte(LUAU_TNUMFLT, D);
  DumpVar(x, D);
}


/*
** DumpIntTT is MBE and embeds a type encoding for string length and integers.
** It also handles negative integers by forcing the type to LUAU_TNUMNINT.
** 0TTTNNNN or 1TTTNNNN (1NNNNNNN)* 0NNNNNNN
*/
static void DumpIntTT (lu_byte tt, lua_Integer y, DumpState *D) {
  int x = y < 0 ? -(y + 1) : y;
  lu_byte buf[sizeof(lua_Integer) + 3];
  lu_byte *b = buf + sizeof(buf) - 1;
  *b-- = x & 0x7f;  x >>= 7;
  while(x) { *b-- = 0x80 + (x & 0x7f); x >>= 7; }
  b++;
  if (*b & cast(lu_byte, LUAU_TMASK) )/* Need an extra byte for the type bits? */
    *--b = 0x80;
  *b |= (y >= 0) ? tt: LUAU_TNUMNINT;
  lua_assert (b >= buf);
  DumpVector(b, (buf - b) + sizeof(buf), D);
}
#define DumpInteger(i, D)  DumpIntTT(LUAU_TNUMPINT, i, D);


/*
** Strings are stored in LFS uniquely, any string references use this index.
** The table at D->stringIndex is used to lookup this unique index.
*/
static void DumpString (const TString *s, DumpState *D) {
  if (s == NULL) {
    DumpByte(LUAU_TSSTRING + 0, D);
  } else {
    lu_byte tt = (gettt(s) == LUA_TSHRSTR) ? LUAU_TSSTRING : LUAU_TLSTRING;
    size_t l = tsslen(s);
    const char *str = getstr(s);
#ifdef LUA_USE_HOST
    if (D->useStrRefs) {
      const TValue *o = luaH_getstr(D->stringIndex, cast(TString *,s));
      DumpIntTT(tt, ivalue(o), D);
      return;
    }
#endif
    DumpIntTT(tt, l + 1, D);   /* include trailing '\0' */
    DumpVector(str, l, D);  /* no need to save '\0' */
  }
}


static void DumpCode (const Proto *f, DumpState *D) {
  DumpInt(f->sizecode, D);
  DumpVector(f->code, f->sizecode, D);
}


static void DumpFunction(const Proto *f, TString *psource, DumpState *D);

static void DumpConstants (const Proto *f, DumpState *D) {
  int i;
  int n = f->sizek;
  DumpInt(n, D);
  for (i = 0; i < n; i++) {
    const TValue *o = &f->k[i];
    switch (ttype(o)) {
      case LUA_TNIL:
        DumpByte(LUAU_TNIL, D);
        break;
      case LUA_TBOOLEAN:
        DumpByte(LUAU_TBOOLEAN + bvalue(o), D);
        break;
      case LUA_TNUMFLT :
        DumpNumber(fltvalue(o), D);
        break;
      case LUA_TNUMINT:
        DumpInteger(ivalue(o), D);
        break;
      case LUA_TSHRSTR:
      case LUA_TLNGSTR:
        DumpString(tsvalue(o), D);
        break;
      default:
        lua_assert(0);
    }
  }
}


static void DumpProtos (const Proto *f, DumpState *D) {
  int i;
  int n = f->sizep;
  DumpInt(n, D);
  for (i = 0; i < n; i++)
    DumpFunction(f->p[i], f->source, D);
}


static void DumpUpvalues (const Proto *f, DumpState *D) {
  int i, n = f->sizeupvalues, nostrip = (D->strip == 0);
  DumpByte(nostrip, D);
  DumpInt(n, D);
  for (i = 0; i < n; i++) {
    if (nostrip)
      DumpString(f->upvalues[i].name, D);
    DumpByte(f->upvalues[i].instack, D);
    DumpByte(f->upvalues[i].idx, D);
  }
}


static void DumpDebug (const Proto *f, DumpState *D) {
  int i, keepli = (D->strip <= 1), keeplv = (D->strip == 0);
  int n = keepli ? f->sizelineinfo : 0;
  DumpInt(n, D);
  DumpVector(f->lineinfo, n, D);
  n = keeplv ? f->sizelocvars : 0;
  DumpInt(n, D);
  for (i = 0; i < n; i++) {
    DumpString(f->locvars[i].varname, D);
    DumpInt(f->locvars[i].startpc, D);
    DumpInt(f->locvars[i].endpc, D);
  }
}


static void DumpFunction (const Proto *f, TString *psource, DumpState *D) {
  if (f->source == psource)
    DumpString(NULL, D);   /* same source as its parent */
  else
    DumpString(f->source, D);
  DumpInt(f->linedefined, D);
  DumpInt(f->lastlinedefined, D);
  DumpByte(getnumparams(f), D);
  DumpByte(getis_vararg(f), D);
  DumpByte(getmaxstacksize(f), D);
  DumpProtos(f, D);
  DumpCode(f, D);
  DumpConstants(f, D);
  DumpUpvalues(f, D);
  DumpDebug(f, D);
}


static void DumpHeader (DumpState *D, int format) {
  DumpLiteral(LUA_SIGNATURE, D);
  DumpByte(LUAC_VERSION, D);
  DumpByte(format, D);
  DumpLiteral(LUAC_DATA, D);
  DumpByte(sizeof(int), D);
  DumpByte(sizeof(Instruction), D);
  DumpByte(sizeof(lua_Integer), D);
  DumpByte(sizeof(lua_Number), D);
/* Note that we multi-byte encoded integers so need to check size_t or endian */
  DumpNumber(LUAC_NUM, D);
}


/*
** Dump Lua function as precompiled chunk
*/
int luaU_dump (lua_State *L, const Proto *f, lua_Writer w, void *data,
               int strip) {
  DumpState D = {0};
  D.L = L;
  D.writer = w;
  D.data = data;
  D.strip = strip;
  DumpHeader(&D, LUAC_FORMAT);
  DumpByte(f->sizeupvalues, &D);
  DumpFunction(f, NULL, &D);
  return D.status;
}

/*============================================================================**
**
** NodeMCU extensions for LFS support and dumping.  Note that to keep lua_lock
** pairing for testing, this dump/unload functionality works within a locked
** window and therefore has to use the core luaH, ..., APIs rather than the
** public Lua and lauxlib APIs.
**
**============================================================================*/
#ifdef LUA_USE_HOST

/*
** Add a TS found in the Proto Load to the table at the ToS. Note that this is
** a unified table of {string = index} for both short and long TStrings.
*/
static void addTS (TString *ts, DumpState *D) {
  lua_State *L = D->L;
  if (ttisnil(luaH_getstr(D->stringIndex, ts))) {
    TValue k, v, *slot;
    gettt(ts)<=LUA_TSHRSTR ? D->sTScnt++ : D->lTScnt++;
    setsvalue(L, &k, ts);
    setivalue(&v, D->sTScnt + D->lTScnt);
    slot = luaH_set(L, D->stringIndex, &k);
    setobj2t(L, slot, &v);
    luaC_barrierback(L, D->stringIndex, &v);
  }
}


/*
** Add the fixed TS that are created by the luaX and LuaT initialisation
** and fixed so not collectable. This are always loaded into LFS to save
** RAM and can be implicitly referenced in any Proto.
*/
static void addFixedStrings (DumpState *D) {
  int i;
  const char *p;
  for (i = 0; (p = luaX_getstr(i, 0))!=NULL; i++)
    addTS(luaS_new(D->L, p), D);
  addTS(G(D->L)->memerrmsg, D);
  addTS(luaS_new(D->L, LUA_ENV), D);
  for (i = 0; (p = luaT_getstr(i))!=NULL; i++)
    addTS(luaS_new(D->L, p), D);
  lua_assert(D->lTScnt == 0); /* all of these fixed strings should be short */
  D->nFixed = D->sTScnt; /* book mark for later skipping */
}


/*
** Dump all LFS strings. If there are 71 fixed and 17 LFS strings, say, in
** the stringIndex, then these fixed and LFS  strings are numbered 1..71 and
** 72..88 respectively; this numbering is swapped to 18..88 and 1..17.  The
** fixed strings are fixed and can be omitted from the LFS image.
*/
static void DumpLFSstrings(DumpState *D) {
  lua_State *L = D->L;
  int n = D->sTScnt + D->lTScnt;
  int i, maxlen = 0, nStrings = n - D->nFixed;
  Table *revT = luaH_new(L);

  sethvalue(L, L->top++, revT); /* Put on stack to prevent GC */
  luaH_resize(L, revT, n, 0);
  luaC_checkGC(L);
  /* luaH_next scan of stringIndex table using two top of stack entries */
  setnilvalue(L->top++);
  api_incr_top(L);
  while (luaH_next(L, D->stringIndex, L->top-2)) {
   /*
    * Update the value to swap fix and LFS order, then insert (v, k) into
    * the reverse index table. Note that luaC_barrier checks not required
    * for overwrites and non-collectable values.
    */
    int len = tsslen(tsvalue(L->top-2));
    lua_Integer *i = &L->top[-1].value_.i;
    *i += *i > D->nFixed ? -D->nFixed : nStrings;        /* recalc index and */
    luaH_set(L, D->stringIndex, L->top-2)->value_.i = *i; /* update table value */
    luaH_setint(L, revT, ivalue(L->top-1), L->top-2); /* Add str to reverse T */
    if (len > maxlen) maxlen = len;  /* roll up maximum string size */
  }
  L->top -= 2; /* discard key and value stack slots */
  DumpInt(maxlen, D);
  DumpInt(D->sTScnt, D);
  DumpInt(D->lTScnt, D);
  DumpInt(nStrings, D);

  for (i = 1; i <= nStrings; i++) { /* dump out non-fixed strings in order */
    const TValue *o = luaH_getint(revT, i);
    DumpString(tsvalue(o), D);
  }
  L->top--; /* pop revT stack entry */
  luaC_checkGC(L);
}


/*
** Recursive scan all of the Protos in the Proto hierarchy
** to collect all referenced strings in 2 Lua Arrays at ToS.
*/
#define OVER(n) for (i = 0; i < (n); i++)
static void scanProtoStrings(const Proto *f, DumpState *D) {
  int i;
  addTS(f->source, D);
  OVER(f->sizek)        if (ttisstring(f->k + i))
                          addTS(tsvalue(f->k + i), D);
  OVER(f->sizeupvalues) addTS(f->upvalues[i].name, D);
  OVER(f->sizelocvars)  addTS(f->locvars[i].varname, D);
  OVER(f->sizep)        scanProtoStrings(f->p[i], D);
}


/*
** An LFS image comprises a prologue segment of all of the strings used in
** the image, followed by a set of Proto dumps.  Each of these is essentially
** the same as standard lua_dump format, except that string constants don't
** contain the string inline, but are instead an index into the prologue.
** Separating out the strings in this way simplifies loading the image
** content into an LFS region.
**
** A dummy container Proto, main, is used to hold all of the Protos to go
** into the image.  The Proto main itself is not callable; it is used as the
** image Proto index and only contains a Proto vector and a constant vector
** where each constant in the string names the corresponding Proto.
*/
int luaU_DumpAllProtos(lua_State *L, const Proto *m, lua_Writer w,
                       void *data, int strip) {
  DumpState D = {0};
  D.L = L;
  D.writer = w;
  D.data = data;
  D.strip = strip;

  lua_assert(L->stack_last - L->top > 5);  /* This dump uses 5 stack slots */
  DumpHeader(&D, LUAC_LFS_IMAGE_FORMAT);
  DumpInteger(G(L)->seed, &D);
  D.stringIndex = luaH_new(L);
  sethvalue(L, L->top++, D.stringIndex);        /* Put on stack to prevent GC */
  /* Add fixed strings + strings used in the Protos, then swap fixed/added blocks */
  addFixedStrings(&D);
  scanProtoStrings(m, &D);
  /* Dump out all non-fixed strings */
  DumpLiteral(LUA_STRING_SIG, &D);
  DumpLFSstrings(&D);
  /* Switch to string reference mode and add the Protos themselves */
  D.useStrRefs = 1;
  DumpLiteral(LUA_PROTO_SIG, &D);
  DumpProtos(m, &D);
  DumpConstants(m, &D);  /* Dump Function name vector */
  L->top--;
  return D.status;
}
#endif
