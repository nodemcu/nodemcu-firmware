/*---
** $Id: lundump.c,v 2.44.1.1 2017/04/19 17:20:42 roberto Exp $
** load precompiled Lua chunks
** See Copyright Notice in lua.h
*/

#define lundump_c
#define LUA_CORE

#include "lprefix.h"

#include <string.h>
#include <alloca.h>
#include "lua.h"

#include "ldebug.h"
#include "ldo.h"
#include "lfunc.h"
#include "lmem.h"
#include "lnodemcu.h"
#include "lobject.h"
#include "lstring.h"
#include "lundump.h"
#include "lzio.h"

/*
** Unlike the standard Lua version of lundump.c, this NodeMCU must be able to
** store the unloaded Protos into one of two targets:
**
** (A)  RAM-based heap in the same way as standard Lua, and where the Proto
**      data structures can be created by direct addressing, but at the same
**      any references must honour the Lua GC assumptions, so that all storage
**      can be collected in the case of a thrown error.
**
** (B)  Flash programmable ROM memory that can be written to serially, but can
**      then be directly addressible through a memory-mapped address window
**      after processor restart.
**
** Mode (B) enables running Lua applications on small-memory IoT devices which
** support programmable flash storage such as the ESP8266 SoC. In the case of
** this chip, the usable RAM heap is roughly 45Kb, so the ability to store an
** extra 128Kb, say, of program into a Lua FLash Store (LFS) can materially
** increase the size of application that can be executed and yet still leave
** most of the heap for true R/W application data.
**
** The changes to this lundump.c facilitate the addition of mode (B).  Output
** to flash uses essentially a record oriented write-once API, and so a few
** macros below are used to encapsulate these two separate output modes.
** Handling the Proto record with its f->k vector requires an adjustment to the
** execution sequence to avoid interleaved resource writes in the case of mode
** (B), but still maintaining GC compliance conformance for mode (A).
**
** The no-interleave constraint also complicates the writing of string resources
** into flash, so the flashing process circumvents this issue by using two
** passes. The first writes any strings that needed to be loaded into flash to
** create a LFS-based ROstrt; the CPU is then restarted. All of the required
** strings can then be resolved against LFS addresses in flash on the 2nd pass.
**
** Note that this module and lundump.c can also be compiled into a host-based
** luac cross compiler. This cross compiler must use the same integer and float
** formats (e.g. 32 bit, 32-bit IEEE) as the target.  Mode B can also be used
** in the host luac.cross to build LFS images and in this case the undump must
** generate 32 address references whether the native size_t is 32 or 64 bit.
*/

typedef struct {
  lua_State  *L;
  ZIO        *Z;
  const char *name;
  LFSHeader  *fh;
  int         useStrRefs;
  TString   **TS;
  int         nTS;
  char       *workB;
  int         nWorkB;
  TString   **workList;
  int         nWorkList;
  int         mode;
  GCObject   *protogc;
  struct pv {
    char *key;
    Proto *p;
  }          *pt;
  int         npt;
} LoadState;


static l_noret error(LoadState *S, const char *why) {
  luaO_pushfstring(S->L, "%s: %s precompiled chunk", S->name, why);
  luaD_throw(S->L, LUA_ERRSYNTAX);
}

#define MODE_RAM 0
#define MODE_LFS 1

/*
** The NewVector and StoreXX functions operate in one of two modes depending
** the Proto hierary is being store to RAM or to LFS.  In MODE_RAM, RAM based
** resources must to allocated, any addresses refer to real RAM address and
** Stores move the content into place at these addresses.  In MODE_LFS, the
** flash API is used to write the resources serially to LFS, and any addresses
** are the future flash-mapped addresses that correspond to the point in the
** LFS where the resource is located.
*/

#define NewVector(S, n, t)  cast(t *,NewVector_(S, n, sizeof(t)))
#define StoreGetPos(S) luaN_writeFlash((S)->Z->data, NULL, 0)

static void *NewVector_(LoadState *S, int n, size_t s) {
  void *v;
  if (S->mode == MODE_RAM) {
    v = luaM_reallocv(S->L, NULL, 0, n, s);
    memset (v, 0, n*s);
  } else {
    v = StoreGetPos(S);
  }
  return v;
}


/* Store and element e of size s at offset i in a vector of size=s elements; */
#define Store(S, a, i, v)  Store_(S, (a), i, &(v), sizeof(v))
static void *Store_(LoadState *S, void *a, int ndx, const void *e, size_t s) {
  if (S->mode == MODE_RAM) {
    char *p = cast(char *, a) + ndx*s;
    if (p != cast(char *,e))
      memcpy(p, e, s);
    return cast(void *, p);
  }
  return (S->mode == MODE_LFS) ? cast(void *, luaN_writeFlash(S->Z->data, e, s)) : NULL;
}

/* reuse to copy a full vector into the store */
#define StoreN(S, v, n)  Store_(S, NULL, 0, v, (n)*sizeof(*v));
#define StoreFlush(S)    luaN_flushFlash((S)->Z->data);

#define LoadVector(S,b,n)	LoadBlock(S,b,(n)*sizeof((b)[0]))
#define LoadVar(S,x)		LoadVector(S,&x,1)

static void LoadBlock (LoadState *S, void *b, size_t size) {
  if (luaZ_read(S->Z, b, size) != 0)
    error(S, "truncated");
}

#define LOADER(n,t) \
static inline t n (LoadState *S) {t x; LoadVar(S, x); return x; }
LOADER(LoadByte, lu_byte)
LOADER(LoadNumber, lua_Number)


/* LoadInt loads 0..MAXINT */
static lua_Integer LoadInt (LoadState *S) {
  lua_Integer x = 0, shift = 0;
  while (1) {
    lu_byte b = LoadByte(S);
    x += (b & 0x7f)<<shift;
    shift += 7;
    if ((b & 0x80) == 0) break;
  }
  return x;
}


static lua_Integer LoadInteger (LoadState *S) {
  lu_byte tt = LoadByte(S);
  lua_Integer x = LoadInt(S);
  return tt==LUA_TNUMINT ? x : -x;
}


static TString *LoadString (LoadState *S) {
  TString *ts;
  int n = LoadInt(S) - 1;
  if (n < 0)
    return NULL;
  if  (S->useStrRefs)
    return S->TS[n];
  if (n <= LUAI_MAXSHORTLEN) {  /* short string? */
    char buff[LUAI_MAXSHORTLEN];
    LoadVector(S, buff, n);
    ts = luaS_newlstr(S->L, buff, n);
  } else {                      /* long string */
    ts = luaS_createlngstrobj(S->L, n);
    LoadVector(S, getstr(ts), n);             /* load directly in final place */
  }
  return ts;
}


static void LoadCode (LoadState *S, Proto *f) {
  Instruction *p;
  f->sizecode = LoadInt(S);
  f->code = luaM_newvector(S->L, f->sizecode, Instruction);
  LoadVector(S, f->code, f->sizecode);
  if (S->mode == MODE_LFS) {
    p = StoreN(S, f->code, f->sizecode);
    luaM_freearray(S->L, f->code, f->sizecode);
    f->code = p;
  }
}


static void *LoadFunction(LoadState *S, Proto *f, TString *psource);

static void LoadConstants (LoadState *S, Proto *f) {
  int i;
  f->sizek = LoadInt(S);
  f->k = NewVector(S, f->sizek, TValue);
  for (i = 0; i < f->sizek; i++) {
    TValue o;
    switch (LoadByte(S)) {
    case LUA_TNIL:
      setnilvalue(&o);
      break;
    case LUA_TBOOLEAN:
      setbvalue(&o, LoadByte(S));
      break;
    case LUA_TNUMFLT:
      setfltvalue(&o, LoadNumber(S));
      break;
    case LUA_TNUMINT:
      setivalue(&o, LoadInt(S));
      break;
    case LUA_TNUMNINT:
      setivalue(&o, -LoadInt(S) - 1);
      break;
    case LUA_TSHRSTR:
    case LUA_TLNGSTR:
      setsvalue2n(S->L, &o, LoadString(S));
      break;
    default:
      lua_assert(0);
    }
    Store(S, f->k, i, o);
  }
}


static void LoadProtos (LoadState *S, Proto *f) {
  int i;
 /*
  * For flash mode, any writes to Proto f must be defered until after
  * all of the writes to the sub Protos have been completed.
  */
  f->sizep = LoadInt(S);
  f->p = (S->mode == MODE_RAM) ? NewVector(S, f->sizep, Proto *) :
                                 alloca(f->sizep * sizeof (Proto *));
  for (i = 0; i < f->sizep; i++)
    f->p[i] = LoadFunction(S, luaF_newproto(S->L), f->source);

  if (S->mode == MODE_LFS)
    f->p = StoreN(S, f->p, f->sizep);
}


static void LoadUpvalues (LoadState *S, Proto *f) {
  int i, nostripnames = LoadByte(S);
  f->sizeupvalues = LoadInt(S);
  if (f->sizeupvalues) {
    f->upvalues = NewVector(S, f->sizeupvalues, Upvaldesc);
    for (i = 0; i < f->sizeupvalues ; i++) {
      TString *name = nostripnames ? LoadString(S) : NULL;
      Upvaldesc uv = {name, LoadByte(S), LoadByte(S)};
      Store(S, f->upvalues, i, uv);
    }
  }
}


static void LoadDebug (LoadState *S, Proto *f) {
  int i;
  f->sizelineinfo = LoadInt(S);
  if (f->sizelineinfo) {
    lu_byte *li = luaM_newvector(S->L, f->sizelineinfo, lu_byte);
    LoadVector(S, li, f->sizelineinfo);
    if (S->mode == MODE_RAM) {
      f->lineinfo = li;
    } else {
      f->lineinfo = StoreN(S, li, f->sizelineinfo);
      luaM_freearray(S->L, li, f->sizelineinfo);
    }
  }
  f->sizelocvars = LoadInt(S);
  f->locvars = NewVector(S, f->sizelocvars, LocVar);
  for (i = 0; i < f->sizelocvars; i++) {
    LocVar lv = { LoadString(S), LoadInt(S), LoadInt(S)};
    Store(S, f->locvars, i, lv);
  }
}


static void *LoadFunction (LoadState *S, Proto *f, TString *psource) {
 /*
  * Main protos have f->source naming the file used to create the hierachy;
  * subordinate protos set f->source != NULL to inherit this name from the
  * parent.  In LFS mode, the Protos are moved from the GC to a local list
  * in S for local GC on thrown errors.
  */
  Proto *p;
  global_State *g = G(S->L);

  if (S->mode == MODE_LFS) {
    lua_assert(g->allgc == obj2gco(f));
    g->allgc = f->next;                    /* remove object from 'allgc' list */
    f->next = S->protogc;         /* push f into the head of the protogc list */
    S->protogc = obj2gco(f);
  }
  f->source = LoadString(S);
  if (f->source == NULL)  /* no source in dump? */
    f->source = psource; /* reuse parent's source */
  f->linedefined = LoadInt(S);
  f->lastlinedefined = LoadInt(S);
  f->numparams = LoadByte(S);
  f->is_vararg = LoadByte(S);
  f->maxstacksize = LoadByte(S);
  LoadProtos(S, f);
  LoadCode(S, f);
  LoadConstants(S, f);
  LoadUpvalues(S, f);
  LoadDebug(S, f);
  if (S->mode == MODE_LFS) {
    GCObject *save = f->next;
    if (f->source != NULL) {
      /* cache the RAM next and set up the next for the LFS proto chain */
      f->next = cast(GCObject *, S->fh->protoHead);
      S->fh->protoHead = p = Store(S, NULL, 0, *f);
    } else {
      p = Store(S, NULL, 0, *f);
    }
    S->protogc = save;             /* pop f from the head of the protogc list */
    luaM_free(S->L, f);                      /* and collect the dead resource */
    f = p;
    }
  return f;
}


static void checkliteral (LoadState *S, const char *s, const char *msg) {
  char buff[sizeof(LUA_SIGNATURE) + sizeof(LUAC_DATA)];   /* larger than both */
  size_t len = strlen(s);
  LoadVector(S, buff, len);
  if (memcmp(s, buff, len) != 0)
    error(S, msg);
}


static void fchecksize (LoadState *S, size_t size, const char *tname) {
  if (LoadByte(S) != size)
    error(S, luaO_pushfstring(S->L, "%s size mismatch in", tname));
}
#define checksize(S,t)	fchecksize(S,sizeof(t),#t)


static void checkHeader (LoadState *S, int format) {
  checkliteral(S, LUA_SIGNATURE + 1, "not a");  /* 1st char already checked */
  if (LoadByte(S) != LUAC_VERSION)
    error(S, "version mismatch in");
  if (LoadByte(S) != format)
    error(S, "format mismatch in");
  checkliteral(S, LUAC_DATA, "corrupted");
  checksize(S, int);
  checksize(S, size_t);
  checksize(S, Instruction);
  checksize(S, lua_Integer);
  checksize(S, lua_Number);
  if (LoadInteger(S) != LUAC_INT)
    error(S, "endianness mismatch in");
  if (LoadNumber(S) != LUAC_NUM)
    error(S, "float format mismatch in");
}


/*
** Load precompiled chunk to support standard LUA_API load functions. The
** extra LFS functionality is effectively NO-OPed out on this MODE_RAM path.
*/
LClosure *luaU_undump(lua_State *L, ZIO *Z, const char *name) {
  LoadState S;
  LClosure *cl;
  if (*name == '@' || *name == '=')
    S.name = name + 1;
  else if (*name == LUA_SIGNATURE[0])
    S.name = "binary string";
  else
    S.name = name;
  S.L = L;
  S.Z = Z;
  S.mode = MODE_RAM;
  S.fh = NULL;
  S.useStrRefs = 0;
  checkHeader(&S, LUAC_FORMAT);
  cl = luaF_newLclosure(L, LoadByte(&S));
  setclLvalue(L, L->top, cl);
  luaD_inctop(L);
  cl->p = luaF_newproto(L);
  LoadFunction(&S, cl->p, NULL);
  lua_assert(cl->nupvalues == cl->p->sizeupvalues);
  return cl;
}

/*============================================================================**
** NodeMCU extensions for LFS support and Loading.  Note that this funtionality
** is called from a hook in the lua startup with the RTS locked, so like
** luaU_undump(), luaU_undumpLFS() runs within a lua_lock() and so cannot use
** the external Lua API. It uses the stack, staying within LUA_MINSTACK limits.
**
** The in-RAM Protos used to assemble proto content prior to writing to LFS
** need special treatment since these hold LFS references rather than RAM ones
** and will cause the Lua GC to error if swept.  Rather than adding complexity
** to lgc.c for this one-off process, these Protos are removed from the allgc
** list and fixed in a local one, and collected inline.
**============================================================================*/

/*
** Load TStrings in dump format.
** ALl TStrings used in an LFS image are dumped as a unique and collated set.
** Subsequent strings in the Proto streams use an index reference into this
** list rather than an inline copy.  This function loads and stores them into
** LFS, constructing the ROstrt for the shorter interned strings.
*/
static void LoadAllStrings (LoadState *S) {
  lua_State *L = S->L;
  global_State *g = G(L);
  LFSHeader *fh = S->fh;
  int i;
  int ns = LoadInt(S), nl = LoadInt(S), nb = sizelstring(LoadInt(S));
  int n = ns + nl, nlist = 1<<luaO_ceillog2(ns);
  char *b = luaM_newvector(L, nb, char);
  TString *ts = cast(TString *, b);
  char *s = getstr(ts);
  TString **list;

  /* allocate dynamic resources and save in S for error path collection */
  S->workB = b;
  S->nWorkB = nb;
  list = luaM_newvector(L, nlist, TString *);
  S->workList = list;
  S->nWorkList = nlist;
  memset (list, 0, nlist*sizeof(TString *));
  S->TS = luaM_newvector(L, n, TString *);
  S->nTS = n;

  for (i = 0; i < n; i++) {
    int l = LoadInt(S) - 1;                   /* No NULL entry in list of TSs */
    size_t totl = sizelstring(l);
    lua_assert (totl <= nb);
    LoadVector(S, s, l);
    s[l] = '\0';
    ts->marked = ~WHITEBITS;
    ts->extra  = 0;
    if (l <= LUAI_MAXSHORTLEN) {                              /* short string */
      TString  **p;
      ts->tt = LUA_TSHRSTR;
      ts->shrlen = cast_byte(l);
      ts->hash = luaS_hash(s, l, g->seed);
      p = list + lmod(ts->hash, nlist);
      ts->u.hnext = *p;
      ts->next = cast(GCObject *, fh->shortTShead);
      S->TS[i] = fh->shortTShead = *p = StoreN(S, b, totl);
    } else {                                                   /* long string */
      ts->tt = LUA_TLNGSTR;
      ts->shrlen = 0;
      ts->u.lnglen = l;
      ts->hash = g->seed;
      luaS_hashlongstr(ts);                     /* sets hash and extra fields */
      ts->next = cast(GCObject *, fh->longTShead);
      S->TS[i] = fh->longTShead = StoreN(S, b, sizelstring(l));
    }
  }
  fh->pROhash = StoreN(S, list, nlist);
  fh->nROuse  = ns;
  fh->nROsize = nlist;
  StoreFlush(S);
  S->workB = luaM_freearray(L, S->workB, S->nWorkB);
  S->workList = luaM_freearray(L, S->workList, S->nWorkList);
  S->nWorkB = S->nWorkList = 0;
}


static unsigned int LoadAllProtos (LoadState *S) {
  lua_State *L = S->L;
  int i, nk, n = LoadInt(S);
  struct pv *pt = S->pt = luaM_newvector(L, n, struct pv);
  S->npt = n;

  /* Load Protos and store addresses in the pt table */
  for (i = 0; i < n; i++) {
    pt[i].p = LoadFunction(S, luaF_newproto(L), NULL);
  }
  nk = LoadInt(S);
  lua_assert(nk == n+1);
  for (i = 0; i < n; i++) {                        /* generate name=proto map */
    int j = i;
    lu_byte tt = novariant(LoadByte(S));
    char *name = getstr(S->TS[LoadInt(S)-1]);
    lua_assert(tt==LUA_TSTRING);
    if (name[0] == '_') {                   /* entries beginning with '_' get */
      Proto *pi = pt[i].p;
      for (j = 0; j < i; j++) {             /* slotted in at the top in order */
        if (pt[j].key[0] != '_' || strcmp(name,pt[j].key)>0)
          break;
      }
      memmove(pt+j+1, pt+j, (i-j)*sizeof(*pt));
      pt[j].p = pi;
    }
    pt[j].key = name;
  }
  /* Write out the pt table and the ROTable entry array + master ROTable */
  {
    ROTable_entry eol = {NULL, LRO_NILVAL};
    ROTable ev = { (GCObject *)1, LUA_TTBLROF, LROT_MARKED,
                 (lu_byte) ~0, n, NULL, cast(ROTable_entry *, StoreGetPos(S))};
    for (i = 0; i < n; i++) {                      /* generate name=proto map */
      ROTable_entry me = {pt[i].key, LRO_LUDATA(pt[i].p)};
      StoreN(S, &me, 1);
    }
    StoreN(S, &eol, 1);
    S->fh->protoROTable = StoreN(S, &ev, 1);
  }
  return LoadInteger(S);
}

static void   undumpLFS(lua_State *L, void *ud) {
  LoadState *S = cast(LoadState *, ud);
  void *F = S->Z->data;
  char *startLFS = StoreGetPos(S);

  luaN_setFlash(F, sizeof(LFSHeader));
  S->fh->flash_sig = FLASH_SIG;
  if (LoadByte(S) != LUA_SIGNATURE[0])
    error(S, "invalid header in");
  checkHeader(S, LUAC_LFS_IMAGE_FORMAT);
  G(S->L)->seed = S->fh->seed = LoadInteger(S);
  checkliteral(S, LUA_STRING_SIG,"no string vector");
  LoadAllStrings (S);
  checkliteral(S, LUA_PROTO_SIG,"no Proto vector");
  S->fh->timestamp = LoadAllProtos(S);
  S->fh->flash_size = cast(char *, StoreGetPos(S)) - startLFS;
  luaN_setFlash(F, 0);
  StoreN(S, S->fh, 1);
  luaN_setFlash(F, 0);
  S->TS = luaM_freearray(L, S->TS, S->nTS);
}


/*
** Load precompiled LFS image.  This is called from a hook in the firmware
** startup if LFS reload is required.
*/
LUAI_FUNC int luaU_undumpLFS(lua_State *L, ZIO *Z) {
  LFSHeader fh = {0};
  LoadState S  = {0};
  int status;
  S.L = L;
  S.Z = Z;
  S.mode = MODE_LFS;
  S.useStrRefs = 1;
  S.fh = &fh;
  L->nny++;                                 /* do not yield during undump LFS */
  status = luaD_pcall(L, undumpLFS, &S, savestack(L, L->top), L->errfunc);
  luaM_freearray(L, S.TS, S.nTS);
  luaM_freearray(L, S.workB, S.nWorkB);
  luaM_freearray(L, S.workList, S.nWorkList);
  luaM_freearray(L, S.pt, S.npt);
  L->nny--;
  return status;
}

