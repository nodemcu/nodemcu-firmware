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
#include "lobject.h"
#include "lstate.h"
#include "ltable.h"
#include "lundump.h"
#include "lgc.h"

typedef struct {
  lua_State *L;
  lua_Writer writer;
  void *data;
  int strip;
  int status;
  int useLocks;
#ifdef LUA_USE_HOST
  int useStrRefs;
  Table *stringIndex;
  int cnt[2];
#endif
} DumpState;

/*
** For compatability with NodeMCU 5.1 the NodeMCU dump format (V 10)
** has some minor changes from the standard (V 01) format to ensure that
** these files are loadable onto the ESP8266 and ESP32 archtectures:
**
** 1.  Strings are a maximum of 32767 bytes long.  String lengths < 128
**     are stored as a single byte, otherwise as a bigendian two byte
**     size the high bit set.
** 2.  Integers are in the range -2^31 .. 2^31-1  (sint32_t)
** 3.  Floats are double IEEE (8 byte) format.
*/

/*
** All high-level dumps go through DumpVector; you can change it to
** change the endianness of the result
*/
#define DumpVector(v,n,D)	DumpBlock(v,(n)*sizeof((v)[0]),D)

#define DumpLiteral(s,D)	DumpBlock(s, sizeof(s) - sizeof(char), D)


static void DumpBlock (const void *b, size_t size, DumpState *D) {
  if (D->status == 0 && size > 0) {
// int i;
// for (i=0; i<size; i++) printf(" %02X", cast(const lu_byte *, b)[i]);
// printf("\n");
    if (D->useLocks) lua_unlock(D->L);
    D->status = (*D->writer)(D->L, b, size, D->data);
    if (D->useLocks) lua_lock(D->L);
  }
}


#define DumpVar(x,D)		DumpVector(&x,1,D)


static void DumpByte (int y, DumpState *D) {
  lu_byte x = (lu_byte)y;
  DumpVar(x, D);
}

/* Dump insigned int 0..MAXINT using multibyte encoding */
static void DumpInt (lua_Integer x, DumpState *D) {
  lua_assert(x>=0);
  do {
    lu_byte b = x & 0x7f;
    x >>= 7;
    DumpByte(b + (x>0 ? 1<<7 : 0), D);
  } while(x>0);
}


static void DumpNumber (lua_Number x, DumpState *D) {
  DumpVar(x, D);
}


static void DumpInteger (lua_Integer x, DumpState *D) {
  lu_byte tt = LUA_TNUMINT;
  if (x<0) {
    tt = LUA_TNUMNINT;
    x = -(x+1);                   /* offset by 1 to keep result as 0..MAX_INT */
  }
  DumpByte(tt, D);
  DumpInt(x, D);
}


static void DumpString (TString *s, DumpState *D) {
  if (s == NULL) {
    DumpInt(0, D);
  } else { 
    const char *str = getstr(s);
    size_t l = tsslen(s);
#ifdef LUA_USE_HOST
    if (D->useStrRefs) {
      const TValue *o = luaH_getstr(D->stringIndex, s);
      DumpInt(ivalue(o), D);
      return;
    }
#endif
    DumpInt(l + 1, D);   /* include trailing '\0' */
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
    if (ttype(o) != LUA_TNUMINT)
      DumpByte(ttype(o), D);
    switch (ttype(o)) {
    case LUA_TNIL:
      break;
    case LUA_TBOOLEAN:
      DumpByte(bvalue(o), D);
      break;
    case LUA_TNUMFLT:
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
    DumpString(NULL, D);                         /* same source as its parent */
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
  DumpByte(sizeof(size_t), D);
  DumpByte(sizeof(Instruction), D);
  DumpByte(sizeof(lua_Integer), D);
  DumpByte(sizeof(lua_Number), D);
  DumpInteger(LUAC_INT, D);
  DumpNumber(LUAC_NUM, D);
}


/*
** dump Lua function as precompiled chunk
*/
int luaU_dump(lua_State *L, const Proto *f, lua_Writer w, void *data,
              int strip) {
  DumpState D;
  D.L = L;
  D.writer = w;
  D.data = data;
  D.strip = strip;
  D.status = 0;
  D.useLocks = 1;
#ifdef LUA_USE_HOST
  D.useStrRefs = 0;
#endif
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
static void addTS(TString *ts, DumpState *D) {
  lua_State *L = D->L;
  if (ttisnil(luaH_getstr(D->stringIndex, ts))) {
    TValue k, v, *slot;
    int n = D->cnt[0] + D->cnt[1] + 1;
    D->cnt[gettt(ts)==LUA_TSHRSTR ? 0 : 1]++;
    setsvalue(L, &k, ts);
    setivalue(&v, n);
    slot = luaH_set(L, D->stringIndex, &k);
    setobj2t(L, slot, &v);
    luaC_barrierback(L, D->stringIndex, &v);
  }
}

#define OVER(n) for (i = 0; i < (n); i++)

/*
** Scan all of the Protos in the Proto hiearchy to collect
** all referenced strings in 2 Lua Arrays at ToS.
*/
static void scanProtoStrings(const Proto *f, DumpState *D) {
  int i;
  addTS(f->source, D);
  OVER(f->sizek)        if (ttisstring(f->k + i)) addTS(tsvalue(f->k + i), D);
  OVER(f->sizeupvalues) addTS(f->upvalues[i].name, D);
  OVER(f->sizelocvars)  addTS(f->locvars[i].varname, D);
  OVER(f->sizep)        scanProtoStrings(f->p[i], D);
}

/*
** Dump all strings
** ToS = table of Short or Long strings
*/
static void DumpAllStrings(DumpState *D) {
  lua_State *L = D->L;
  int i, maxl=0, n = D->cnt[0] + D->cnt[1];
  Table *revT = luaH_new(L);
  sethvalue(L, L->top++, revT);                 /* Put on stack to prevent GC */
  luaH_resize(L, revT, n, 0);
  luaC_checkGC(L);
  setnilvalue(L->top++);                /* allocate key on stack & set to nil */
  api_incr_top(L);                                 /* allocate value on stack */

  while (luaH_next(L, D->stringIndex, L->top-2)) {         /* loop over table */
    int l = tsslen(tsvalue(L->top-2));         /* track maximinum string size */
    if (l>maxl) maxl = l;
    luaH_setint(L, revT, ivalue(L->top-1), L->top-2);      /* Add str to revT */
    luaC_barrierback(L, revT, L->top-2);
  }
  L->top -= 2;                           /* discard key and value stack slots */
  DumpInt(D->cnt[0], D);
  DumpInt(D->cnt[1], D);
  DumpInt(maxl, D);
  for (i = 1; i <= n; i++) {                 /* dump out all strings in order */
    const TValue *o = luaH_getint(revT, i);
    DumpString(tsvalue(o), D);
  }
  L->top--;                                           /* pop revT stack entry */
  luaC_checkGC(L);
}

/*
** An LFS image comrises a prologue segment of all of the strings used in the
** image, followed by a set of Proto dumps.  Each of these is essentially the
** same as standard lua_dump format, except that string constants don't contain
** the string inline, but are instead an index into the prologue.  Separating
** out the strings in this way simplifies loading the image content into an LFS
** region.
**
** A dummy container Proto, main, is used to hold all of the Protos to go into
** the image.  The Proto main itself is not callable; it is used as the image
** Proto index and only contains a Proto vector and a constant vector where
** each constant in the string names the corresponding proto.
*/
int luaU_DumpAllProtos(lua_State *L, const Proto *m, lua_Writer w,
                       void *data, int strip) {
  DumpState D;
  D.L = L;
  D.writer = w;
  D.data = data;
  D.strip = strip;
  D.status = 0;
  D.useLocks = 0;
  D.useStrRefs = 0;
  D.cnt[0] = D.cnt[1] = 0;
  lua_assert(L->stack_last - L->top > 5);     /* This dump uses 4 stack slots */
  DumpHeader(&D, LUAC_LFS_IMAGE_FORMAT);
  DumpInteger(G(L)->seed, &D);
  D.stringIndex = luaH_new(L);
  sethvalue(L, L->top++, D.stringIndex);        /* Put on stack to prevent GC */
  scanProtoStrings(m, &D);
  DumpLiteral(LUA_STRING_SIG, &D);
  DumpAllStrings(&D);                                     /* Dump all strings */
  D.useStrRefs = 1;
  DumpLiteral(LUA_PROTO_SIG, &D);
  DumpProtos(m, &D);                                           /* Dump Protos */
  DumpConstants(m, &D);                          /* Dump Function name vector */
  L->top--;
  return D.status;
}
#endif
