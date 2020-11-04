/*
** $Id: ldump.c,v 2.37.1.1 2017/04/19 17:20:42 roberto Exp $
** save precompiled Lua chunks
** See Copyright Notice in lua.h
*/

#define ldump_c
#define LUA_CORE

#include "lprefix.h"

#include <stddef.h>
#include <string.h>

#include <stdio.h> /*DEBUG*/

#include "lua.h"
#include "lapi.h"
#include "lgc.h"
#include "lmem.h"
#include "lobject.h"
#include "lstate.h"
#include "lstring.h"
#include "ltable.h"
#include "lundump.h"
#include "lvm.h"


typedef struct {
  lua_State *L;
  lua_Writer writer;
  void *data;
  unsigned int crc;
  int strip;
  int status;
  int pass;
  int size;
  TString **ts;
  unsigned short *tsNdx;
  unsigned short *tsInvNdx;
  int tsHashLen;
  int tsCnt;
  int maxTSsize;
  int tsProbeCnt;
  Table *funcTable;
  int funcCnt;
} DumpState;

/*
** To ensure that dump files are loadable onto the ESP architectures:
** 1.  Integers are in the range -2^31 .. 2^31-1  (sint32_t)
** 2.  Floats are the IEEE 4 or 8 byte format, with a 4 byte default.
**
** The file formats are also different to standard because of two
** additional goals:
** 3.  The file must be serially loadable into a programmable flash
**     memory through a file-write like API call.
** 4.  Compactness of dump files is a key design goal.
*/

/*
** A simple hash table is use to collect unique TStrings that will be dumped
** to the output writer.  RAM is at a premium rather than dump time so the
** dump is processed in multiple passes with the first counting TStrings.
** This allows the hash to be size once and avoid the complexities of dynamic
** growth of hash tables.  We only need get and add functions.
**
** The order function creates the inverse Ndx -> TSstring lookup
*/
static int hashTSget (TString *s, DumpState *D) {
  int i, i0 = lmod(s->hash,D->tsHashLen);
  for (i = i0; D->ts[i] != NULL; i = lmod(i+1, D->tsHashLen)) {
    TString *b = D->ts[i];
    D->tsProbeCnt++;
    if (s == b) break;
    if (gettt(s) == LUA_TSHRSTR || gettt(b) == LUA_TSHRSTR) continue;
    if (luaS_eqlngstr(s, b)) break;
  }
  return i;
}

static void hashTSadd (TString *s, DumpState *D) {
  if (gettt(s) == LUA_TLNGSTR)
    luaS_hashlongstr(s);  /* Dumped TStrings must have a hash */
  int i = hashTSget(s, D);
  if (!D->ts[i]) {
    if (D->maxTSsize < tsslen(s))
      D->maxTSsize = tsslen(s);
    D->ts[i] = s;
    D->tsNdx[i] = D->tsCnt++;
  }
}

static void hashTSorder (DumpState *D) {
  for (int i = 0; i < D->tsHashLen; i++) {
    if (D->ts[i])
      D->tsInvNdx[D->tsNdx[i]] = i;
  }
}


/*
** There are 3 processing passes of the Proto hierarchy.  The 1st computes the
** size of the output file and the size of the tsHash table.  The 2nd writes
** the ts references to the tsHash.  The 3rd writes out the dump stream.
*/
static void DumpBlock (const void *b, size_t size, DumpState *D) {
  if (D->pass == 1) {
    D->size += size;
  } else if (D->pass == 3 && D->status == 0 && size > 0) {
    D->crc = luaU_zlibcrc32(b, size, D->crc);
    lua_unlock(D->L);
    D->status = (*D->writer)(D->L, b, size, D->data);
    lua_lock(D->L);
//*DEBUG*/ static size_t n = 0; n+=size; printf("Block:%8zu %8zu 0x%08x\n",size,n,D->crc);
  }
}

static inline void DumpByte (lu_byte x, DumpState *D) {
  DumpBlock(&x, 1, D);
}

#define DumpLiteral(s,D) DumpBlock(s, strlen(s), D)

/*
** (Unsigned) integers are typically small so are encoded using a multi-byte
** encoding: that a single 0NNNNNNN or a multibyte (1NNNNNNN)* 0NNNNNNN.
**
** DumpCount() does a DumpBlock of an MBE encoded unsigned.
**
** DumpIntTT() is used for type encoded ints.  If negative then the type is
** forced to LUAU_TNUMNINT and the number negated so a +ve int always encoded.
** If the 1st byte is anything other than x000xxxx, then bytes are extended by
** a leading 10000000.  That way the type can always be inserted the 1st byte.
*/

typedef lu_byte mbe_buf_t[2*sizeof(lua_Integer)];

static lu_byte *MBEencode (lua_Integer x, lu_byte *buf) {
  lu_byte *b = buf + sizeof(mbe_buf_t);
  lua_assert(x>=0);
  *--b = x & 0x7f;  x >>= 7;
  while(x) {
    *--b = 0x80 + (x & 0x7f); x >>= 7;
  }
  lua_assert (b >= buf);
  return b;
}

static void DumpCount (lua_Integer x, DumpState *D) {
  mbe_buf_t buf;
//*DEBUG*/ printf("Count:%u\n",x);
  lu_byte *b = MBEencode(x, buf);
  DumpBlock(b, sizeof(buf) - (b - buf), D);
}

static void DumpIntTT (lu_byte tt, lua_Integer x, DumpState *D) {
  mbe_buf_t buf;
  lu_byte *b = MBEencode((x<0 ? (tt = LUAU_TNUMNINT, -x) : x) , buf);
  if (*b & ((lu_byte) LUAU_TMASK))/* Need an extra byte for the type bits? */
    *--b = 0x80;
  *b |= tt;
  DumpBlock(b, sizeof(buf) - (b - buf), D);
}


/*
** Strings are stored in LFS uniquely, any string references use this index.
** The table at D->stringIndex is used to lookup this unique index.
*/
static void DumpString (TString *s, DumpState *D) {
  if (D->pass == 1 && s != NULL) {
    D->tsCnt++;
  } else if (D->pass == 2&& s != NULL) {
    hashTSadd(s, D);
  } else if (D->pass == 3) {
    if (s == NULL) {
      DumpByte(LUAU_TSTRING + 0, D);
//*DEBUG*/ printf("String:0:NULL\n");
    } else {
      int i = hashTSget(s, D);
      lua_assert(s->hash == D->ts[i]->hash);
      lua_assert(tsslen(s) <= D->maxTSsize);
      i = D->tsNdx[i]+1;
//*DEBUG*/ printf("String:%u:%s\n",i,getstr(s));
      DumpIntTT(LUAU_TSTRING, i, D);  /* Slot [0] is reserved for NULL */
    }
  }
}


static void DumpConstant (const TValue *o, DumpState *D) {
  switch (ttype(o)) {
    case LUA_TNIL:
      DumpByte(LUAU_TNIL, D);
      break;
    case LUA_TBOOLEAN:
      DumpByte(LUAU_TBOOLEAN + bvalue(o), D);
      break;
    case LUA_TNUMFLT : {
      lua_Number x = fltvalue(o);
      DumpByte(LUAU_TNUMFLT, D);
      DumpBlock(&x, sizeof(x), D);
      break;
      }
    case LUA_TNUMINT:
      DumpIntTT(LUAU_TNUMPINT, ivalue(o), D);
      break;
    case LUA_TSHRSTR:
    case LUA_TLNGSTR:
      DumpString(tsvalue(o), D);
      break;
    default:
      lua_assert(0);
  }
}


static void DumpFunction (const Proto *f, TString *psource, DumpState *D);

#define OVER(n) DumpCount((n),D); for (int i = 0; i< (n); i++)

static void DumpFunction (const Proto *f, TString *psource, DumpState *D) {
  /* This function MUST be aligned to lundump.c:LoadFunction() */
//*DEBUG*/ static int cnt = 0; cnt++;
  DumpString(f->source == psource ? NULL : f->source, D);
  OVER(f->sizep)
    DumpFunction(f->p[i], f->source, D);
  DumpCount(f->linedefined, D);
  DumpCount(f->lastlinedefined, D);
//*DEBUG*/ printf("Function from %u to %u\n", f->linedefined, f->lastlinedefined);
  DumpByte(getnumparams(f), D);
  DumpByte(getis_vararg(f), D);
  DumpByte(getmaxstacksize(f), D);
  DumpCount(f->sizecode, D);
  DumpBlock(f->code, f->sizecode*sizeof(f->code[0]), D);
  OVER(f->sizek)
    DumpConstant(f->k + i, D);
  OVER(f->sizeupvalues) {
    DumpString(D->strip == 0 ? f->upvalues[i].name : 0, D);
    DumpByte(f->upvalues[i].instack, D);
    DumpByte(f->upvalues[i].idx, D);
  }
  OVER(D->strip == 0 ? f->sizelocvars : 0) {
    DumpString(f->locvars[i].varname, D);
    DumpCount(f->locvars[i].startpc, D);
    DumpCount(f->locvars[i].endpc, D);
  }
  DumpCount((D->strip <= 1 ? f->sizelineinfo : 0), D);
  DumpBlock(f->lineinfo, (D->strip <= 1 ? f->sizelineinfo : 0), D);
}


static void DumpHeader (DumpState *D) {
  const TValue num = {{.n = LUAC_NUM}, LUA_TNUMFLT};
  DumpLiteral(LUA_SIGNATURE, D);
  DumpByte(LUAC_VERSION, D);
  DumpByte(LUAC_FORMAT, D);
  DumpLiteral(LUAC_DATA, D);
  DumpByte(sizeof(int), D);
  DumpByte(sizeof(Instruction), D);
  DumpByte(sizeof(lua_Integer), D);
  DumpByte(sizeof(lua_Number), D);
/* Note that we multi-byte encoded integers so need to check size_t or endian */
  DumpConstant(&num, D);
}

/*
** Loop over top level functions (TLFs) in the Index table.  Entries in the
** table are either [i]=func or name=func. In the first case the name is 
** stripped from the Proto source field.  The first N TStrings in the TString 
** vector are the names of the N TLFs, so these are defined before dumping each
** in turn.
*/
static void DumpPass (int pass, DumpState *D) {
  lua_State *L = D->L;
  size_t l;
  D->pass = pass;
  setnilvalue(L->top-2);
  if (pass == 2) {
    while (luaH_next(L, D->funcTable, L->top-2)) {
      if (ttisLclosure(L->top-1)) {
        const char *s = luaU_getbasename(L, getproto(L->top-1)->source, &l);
        hashTSadd((ttisstring(L->top-2) ? tsvalue(L->top-2) :
                                          luaS_newlstr(L, s, l)), D);
        D->funcCnt++;
      }
    }
  }
  setnilvalue(L->top-2);
  while (luaH_next(L, D->funcTable, L->top-2)) {
    if (ttisLclosure(L->top-1))
      DumpFunction(getproto(L->top-1), NULL, D);
  }
}

static void protectedDump (lua_State *L, void *ud) {
  DumpState *D = (DumpState *) ud;
  unsigned int crc;
  L->top++; api_incr_top(L);  /* add key and value slots to stack */

  DumpPass(1, D);
  D->tsHashLen = 1 << luaO_ceillog2(((D->tsCnt)*5)/4);
  D->ts = luaM_newvector(L, D->tsHashLen, TString *);
  memset(D->ts, 0, sizeof(D->ts[0])*D->tsHashLen);
  D->tsNdx = luaM_newvector(L, D->tsHashLen, unsigned short);
  memset(D->tsNdx, 0, sizeof(D->tsNdx[0])*D->tsHashLen);
  D->tsCnt = 0;
  DumpPass(2, D);
  D->tsInvNdx = luaM_newvector(L, D->tsCnt, unsigned short);
  memset(D->tsInvNdx, 0, sizeof(D->tsInvNdx[0])*D->tsCnt);
  hashTSorder(D);
  D->pass = 3;
  DumpHeader(D);
  DumpByte(D->funcCnt, D);
  DumpCount(D->maxTSsize, D);
//*DEBUG*/ printf("*** String Dump ***\n");
  OVER(D->tsCnt) {
    int j = D->tsInvNdx[i];
    TString *ts = D->ts[j];
    const char *s = getstr(ts);
    size_t l = tsslen(ts);
    DumpCount(l + 1, D);   /* include trailing '\0' */
    DumpBlock(s, l, D);  /* no need to save '\0' */
//*DEBUG*/ printf("%5u:%4zu:%s\n",i+1,l, s ? getstr(D->ts[j]) : "NULL");
  }
//*DEBUG*/ printf("*** End String Dump ***\n");
  DumpLiteral(LUA_PROTO_SIG, D);
  DumpPass(3, D);
  crc = ~D->crc;
  DumpBlock(&crc, sizeof(crc), D);
  L->top -= 2;
}

/*
** Dump Lua function as precompiled chunk L->top-1 is array of name->function
*/
int luaU_dump (lua_State *L, Table *pt, lua_Writer w, void *data,
               int strip) {
  DumpState D = {0};
  D.L = L;
  D.writer = w;
  D.data = data;
  D.strip = strip;
  D.funcTable = pt;
  D.crc = ~0;
  /* Protected of call of Dump Protos to tidy up allocs on error */
  int status = luaD_pcall(L, protectedDump, &D, savestack(L, L->top), 0);
  luaM_freearray(L, D.ts, D.tsHashLen);
  luaM_freearray(L, D.tsNdx, D.tsHashLen);
  luaM_freearray(L, D.tsInvNdx, D.tsCnt);
  return status;  //<<<<<<<<<<<< or we throw on zero. D.status status
}


static int strip1debug (lua_State *L, Proto *f, int level) {
  int i, len = 0;
  switch (level) {
    case 2:
      if (f->lineinfo) {
        f->lineinfo = luaM_freearray(L, f->lineinfo, f->sizelineinfo);
        len += f->sizelineinfo;
      }
    case 1:
      for (i=0; i<f->sizeupvalues; i++)
        f->upvalues[i].name = NULL;
      f->locvars = luaM_freearray(L, f->locvars, f->sizelocvars);
      len += f->sizelocvars * sizeof(LocVar);
      f->sizelocvars = 0;
  }
  return len;
}

/* This is a recursive function so it's stack size has been kept to a minimum! */
int luaU_stripdebug (lua_State *L, Proto *f, int level, int recv){
  int len = 0, i;
  if (recv != 0 && f->sizep != 0) {
    for(i=0;i<f->sizep;i++) len += luaU_stripdebug(L, f->p[i], level, recv);
  }
  len += strip1debug (L, f, level);
  return len;
}

/*
 * CRC32 checksum
 *
 * Copyright (c) 1998-2003 by Joergen Ibsen / Jibz
 * All Rights Reserved
 *
 * http://www.ibsensoftware.com/
 *
 * This software is provided 'as-is', without any express
 * or implied warranty.  In no event will the authors be
 * held liable for any damages arising from the use of
 * this software.
 *
 * Permission is granted to anyone to use this software
 * for any purpose, including commercial applications,
 * and to alter it and redistribute it freely, subject to
 * the following restrictions:
 *
 * 1. The origin of this software must not be
 *    misrepresented; you must not claim that you
 *    wrote the original software. If you use this
 *    software in a product, an acknowledgment in
 *    the product documentation would be appreciated
 *    but is not required.
 *
 * 2. Altered source versions must be plainly marked
 *    as such, and must not be misrepresented as
 *    being the original software.
 *
 * 3. This notice may not be removed or altered from
 *    any source distribution.
 */

/*
 * CRC32 algorithm taken from the zlib source, which is
 * Copyright (C) 1995-1998 Jean-loup Gailly and Mark Adler
 */

static const unsigned int crc32tab[16] = {
   0x00000000, 0x1db71064, 0x3b6e20c8, 0x26d930ac, 0x76dc4190,
   0x6b6b51f4, 0x4db26158, 0x5005713c, 0xedb88320, 0xf00f9344,
   0xd6d6a3e8, 0xcb61b38c, 0x9b64c2b0, 0x86d3d2d4, 0xa00ae278,
   0xbdbdf21c
};

/* crc is previous value for incremental computation, 0xffffffff initially */
unsigned int luaU_zlibcrc32(const void *data, size_t length, unsigned int crc) {
   const unsigned char *buf = (const unsigned char *) data;

//*DEBUG*/ printf("CRC32: 0x%08x ", crc);

   for (int i = 0; i < length; ++i) {
//*DEBUG*/ printf("%02x ", buf[i]);
      crc ^= buf[i];
      crc = crc32tab[crc & 0x0f] ^ (crc >> 4);
      crc = crc32tab[crc & 0x0f] ^ (crc >> 4);
   }

   // return value suitable for passing in next time, for final value invert it
//*DEBUG*/ printf("- 0x%08x\n", crc);
   return crc;
}
