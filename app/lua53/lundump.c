/*---
** $Id: lundump.c,v 2.44.1.1 2017/04/19 17:20:42 roberto Exp $
** load precompiled Lua chunks
** See Copyright Notice in lua.h
*/

#define lundump_c
#define LUA_CORE

#include "lprefix.h"


#include <string.h>
#include "lua.h"

#include "ldebug.h"
#include "ldo.h"
#include "lfunc.h"
#include "llex.h"
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
**      then be directly addressable through a memory-mapped address window
**      after processor restart.
**
** Mode (B) a.k.a LFS (Lua FLash Store) enables running Lua applications on
** small-memory IoT devices which support programmable flash storage such as
** the ESP8266 SoC. In the case of this chip, the usable RAM heap is roughly
** 45Kb, so the ability to store an extra 128Kb, say, of program into LFS can
** materially increase the size of application that can be executed and leave
** most of the heap for true R/W application data.
**
** The changes to this lundump.c facilitate the addition of LFS mode. Writing
** to flash uses a record oriented write-once API, and ONCE THE FLASH CACHE
** HAS BEEN FLUSHED this data can be reread from using the memory-mapped RO
** Flash window. So in mode B, the resources aren't allocated in RAM but are
** written to Flash and the corresponding Flash read address is returned; also
** data can't be immediately read back using these addresses because of cache
** staleness.
** 
** Handling the Proto record with TSndx f->k vector requires an adjustment to
** the execution sequence to avoid interleaved resource writes in the case of
** mode (B), but still maintaining GC compliance conformance for mode (A). The
** no-interleave constraint also complicates the writing of string resources
** into flash, so the flashing process circumvents this issue by using two
** passes. The first writes any strings that needed to be loaded into flash to
** create a LFS-based ROstrt; the CPU is then restarted. All of the required
** strings can then be resolved against LFS addresses in flash on the 2nd pass.
**
** Note that this module and lundump.c can also be compiled into a host-based
** luac cross compiler. This cross compiler must use the same integer and float
** formats (e.g. 32 bit, 32-bit IEEE) as the target. LFS mode can also be used
** in the host luac.cross to build LFS images and in this case the undump must
** generate native 32 or 64 bit address references.
*/

#define MODE_RAM 0
#define MODE_LFS 1

typedef struct {
  lua_State  *L;           /* cache L to drop parameter list */
  ZIO        *Z;           /* ZIO context */
  const char *name;        /* Filename of the LFS image being loaded */
  LFSHeader  *fh;          /* LFS flash header block */
  TString   **TS;          /* List of TStrings being used in the image */
  lu_int32    TSlen;       /* Length of the same */
  lu_int32    TSndx;       /* Index into the same */
  lu_int32    TSnFixed;    /* Number of "fixed" TS */
  char        *buff;        /* Working buffer for assembling a TString */
  lu_int32    buffLen;     /* Maximum length of TS used in the image */
  TString   **list;        /* TS list used to index the ROstrt */
  lu_int32    listLen;     /* Length of the same */
  Proto     **pv;          /* List of Protos in LFS */
  lu_int32    pvLen;       /* Length of the same */
  GCObject   *protogc;     /* LFS proto linked list */
  lu_byte     useStrRefs;  /* Flag if set then TStings are a index into TS */
  lu_byte     mode;        /* Either LFS or RAM */
} LoadState;


static l_noret error(LoadState *S, const char *why) {
  luaO_pushfstring(S->L, "%s: %s precompiled chunk", S->name, why);
  luaD_throw(S->L, LUA_ERRSYNTAX);
}


/*
** The NewVector and StoreXX functions operate in one of two modes depending
** the Proto hierary is being store to RAM or to LFS.
**
** MODE_RAM: RAM based resources must to allocated, and any addresses refer to
** real RAM address and Stores move the content into place at these addresses.
**
** MODE_LFS: the flash API is used to write the resources serially to LFS, and
** any addresses are the future flash-mapped addresses that correspond to the
** mapped in the LFS where the resource will be located after LFS mapping.
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
    return p;
  }
  return (S->mode == MODE_LFS) ? luaN_writeFlash(S->Z->data, e, s) : NULL;
}

#define StoreN(S, v, n)  Store_(S, NULL, 0, (v), (n)*sizeof(*(v)));
#define StoreFlush(S)    luaN_flushFlash((S)->Z->data);

#define LoadVector(S,b,n)	LoadBlock(S,b,(n)*sizeof((b)[0]))

static void LoadBlock (LoadState *S, void *b, size_t size) {
  lu_int32 left = luaZ_read(S->Z, b, size);
  if ( left != 0)
    error(S, "truncated");
}


#define LoadVar(S,x)		LoadVector(S,&x,1)


static lu_byte LoadByte (LoadState *S) {
  lu_byte x;
  LoadVar(S, x);
  return x;
}


static lua_Integer LoadInt (LoadState *S) {
  lu_byte b;
  lua_Integer x = 0;
  do { b = LoadByte(S); x = (x<<7) + (b & 0x7f); } while (b & 0x80);
  return x;
}


static lua_Number LoadNumber (LoadState *S) {
  lua_Number x;
  LoadVar(S, x);
  return x;
}


static lua_Integer LoadInteger (LoadState *S, lu_byte tt_data) {
  lu_byte b;
  lua_Integer x = tt_data & LUAU_DMASK;
  if (tt_data & 0x80) {
    do { b = LoadByte(S); x = (x<<7) + (b & 0x7f); } while (b & 0x80);
  }
  return (tt_data & LUAU_TMASK) == LUAU_TNUMNINT ? -x-1 : x;
}

static TString *LoadString_ (LoadState *S, int prelen) {
  TString *ts;
  char buff[LUAI_MAXSHORTLEN];
  int n = LoadInteger(S, (prelen < 0 ? LoadByte(S) : prelen)) - 1;
  if (n < 0)
    return NULL;
  if  (S->useStrRefs)
    ts = S->TS[n];
  else if (n <= LUAI_MAXSHORTLEN) {  /* short string? */
    LoadVector(S, buff, n);
    ts = luaS_newlstr(S->L, buff, n);
  } else {                      /* long string */
    ts = luaS_createlngstrobj(S->L, n);
    LoadVector(S, getstr(ts), n);             /* load directly in final place */
  }
  return ts;
}
#define LoadString(S) LoadString_(S,-1) 
#define LoadString2(S,pl) LoadString_(S,(pl)) 

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
   /*
    * tt is formatted 0bFTTTDDDD where TTT is the type; the F and the DDDD
    * fields are used by the integer decoder as this often saves a byte in
    * the endcoding.
    */
    lu_byte tt = LoadByte(S);
    switch (tt & LUAU_TMASK) {
    case LUAU_TNIL:
      setnilvalue(&o);
      break;
    case LUAU_TBOOLEAN:
      setbvalue(&o, !(tt == LUAU_TBOOLEAN));
      break;
    case LUAU_TNUMFLT:
      setfltvalue(&o, LoadNumber(S));
      break;
    case LUAU_TNUMPINT:
    case LUAU_TNUMNINT:
      setivalue(&o, LoadInteger(S, tt));
      break;
    case LUAU_TSTRING:
      setsvalue2n(S->L, &o, LoadString2(S, tt));
      break;
    default:
      lua_assert(0);
    }
    Store(S, f->k, i, o);
  }
}

/*
** The handling of Protos has support both modes, and in the case of flash
** mode, this requires some care as any writes to a Proto f must be deferred 
** until after all of the writes to its sub Protos have been completed; so
** the Proto record and its p vector must be retained in RAM until stored to
** flash. 
**
** Recovery of dead resources on error handled by the Lua GC as standard in 
** the case of RAM loading.  In the case of loading an LFS image into flash, 
** the error recovery could be done through the S->protogc list, but given
** that the immediate action is to restart the CPU, there is little point
** in adding the extra functionality to recover these dangling resources.
*/
static void LoadProtos (LoadState *S, Proto *f) {
  int i, n = LoadInt(S);
  Proto **p = luaM_newvector(S->L, n, Proto *);
  f->p = p;
  f->sizep = n;
  memset (p, 0, n * sizeof(*p));
  for (i = 0; i < n; i++)
    p[i] = LoadFunction(S, luaF_newproto(S->L), f->source);

  if (S->mode == MODE_LFS) {
    f->p = StoreN(S, p, f->sizep);
    luaM_freearray(S->L, p, n);
  } 
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
    LocVar lv = {LoadString(S), LoadInt(S), LoadInt(S)};
    Store(S, f->locvars, i, lv);
  }
}


static void *LoadFunction (LoadState *S, Proto *f, TString *psource) {
 /*
  * Main protos have f->source naming the file used to create the hierarchy;
  * subordinate protos set f->source != NULL to inherit this name from the
  * parent.  In LFS mode, the Protos are moved from the GC to a local list
  * in S, but no error GC is attempted as discussed in LoadProtos.
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
    f->source = psource;  /* reuse parent's source */
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
      setLFSbit(f);
      /* cache the RAM next and set up the next for the LFS proto chain */
      f->next = cast(GCObject *, S->fh->protoHead);
      S->fh->protoHead = p = Store(S, NULL, 0, *f);
    } else {
      p = Store(S, NULL, 0, *f);
    }
    S->protogc = save;  /* pop f from the head of the protogc list */
    luaM_free(S->L, f);  /* and collect the dead resource */
    f = p;
  }
  return f;
}


static void checkliteral (LoadState *S, const char *s, const char *msg) {
  char buff[sizeof(LUA_SIGNATURE) + sizeof(LUAC_DATA)]; /* larger than both */
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
 /*
  * The standard Lua VM does a check on the sizeof size_t and endian check on
  * integer; both are dropped as the former prevents dump files being shared 
  * across 32 and 64 bit machines, and we use multi-byte coding of ints.
  */
  checksize(S, Instruction);
  checksize(S, lua_Integer);
  checksize(S, lua_Number);
  LoadByte(S);  /* skip number tt field */
  if (LoadNumber(S) != LUAC_NUM)
    error(S, "float format mismatch in");
}


/*
** Load precompiled chunk to support standard LUA_API load functions. The
** extra LFS functionality is effectively NO-OPed out on this MODE_RAM path.
*/
LClosure *luaU_undump(lua_State *L, ZIO *Z, const char *name) {
  LoadState S = {0};
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
** is called from a hook in the lua startup within a lua_lock() (as with
** LuaU_undump),  so luaU_undumpLFS() cannot use the external Lua API. It does
** uses the Lua stack, but staying within LUA_MINSTACK limits.
**
** The in-RAM Protos used to assemble proto content prior to writing to LFS
** need special treatment since these hold LFS references rather than RAM ones
** and will cause the Lua GC to error if swept.  Rather than adding complexity
** to lgc.c for this one-off process, these Protos are removed from the allgc
** list and fixed in a local one, and collected inline.
**============================================================================*/

/*
** Write a TString to the LFS. This parallels the lstring.c algo but writes
** directly to the LFS buffer and also append the LFS address in S->TS. Seeding
** is based on the seed defined in the LFS image, rather than g->seed.
*/
static void addTS(LoadState *S, int l, int extra) {
  LFSHeader *fh   = S->fh;
  TString *ts     = cast(TString *, S->buff);
  char *s         = getstr(ts);
  size_t totl     = sizelstring(l);

  lua_assert (totl <= S->buffLen);
  s[l] = '\0';
  /* The collectable and LFS bits must be set; all others inc the whitbits clear */
  ts->marked = bitmask(LFSBIT) | BIT_ISCOLLECTABLE;
  ts->extra  = extra;

  if (l <= LUAI_MAXSHORTLEN) {                              /* short string */
    TString  **p;
    ts->tt = LUA_TSHRSTR;
    ts->shrlen = cast_byte(l);
    ts->hash = luaS_hash(s, l, fh->seed);
    p = S->list + lmod(ts->hash, S->listLen);
    ts->u.hnext = *p;
    ts->next = cast(GCObject *, fh->shortTShead);
    S->TS[S->TSndx] = fh->shortTShead = *p = StoreN(S, S->buff, totl);

  } else {                                                   /* long string */
    ts->tt = LUA_TLNGSTR;
    ts->shrlen = 0;
    ts->u.lnglen = l;
    ts->hash = fh->seed;
    luaS_hashlongstr(ts);                     /* sets hash and extra fields */
    ts->next = cast(GCObject *, fh->longTShead);
    S->TS[S->TSndx] = fh->longTShead = StoreN(S, S->buff, sizelstring(l));
  }
  S->TSndx++;
}

/*
** The runtime (in ltm.c and llex.c) declares ~100 fixed strings and so these
** are moved into LFS to free up an extra ~2Kb RAM.  Extra get token access
** functions have been added to these modules.  These tokens aren't unique as 
** ("nil" and "function" are both tokens and typenames), hardwiring this
** duplication debounce as a wrapper around addTS() is the simplest way of
** voiding the need for extra lookup resources.
*/
static void addTSnodup(LoadState *S, const char *s, int extra) {
  int i, l = strlen(s);
  static struct {const char *k; int found; } t[] = {{"nil", 0},{"function", 0}};
  for (i = 0; i < sizeof(t)/sizeof(*t); i++) {
    if (!strcmp(t[i].k, s)) {
      if (t[i].found) return;  /* ignore the duplicate copy */
      t[i].found = 1; /* flag that this constant is already loaded */
      break;
      }
  }
  memcpy(getstr(cast(TString *, S->buff)), s, l);
  addTS(S, l, extra);
}

/*
** Load TStrings in dump format. ALl TStrings used in an LFS image excepting
** any fixed strings are dumped as a unique collated set.  Any strings in the
** following Proto streams use an index reference into this list rather than an
** inline copy.  This function loads and stores them into LFS, constructing the
** ROstrt for the shorter interned strings.
*/
static void LoadAllStrings (LoadState *S) {
  lua_State *L = S->L;
  global_State *g = G(L);
  int nb       = sizelstring(LoadInt(S));
  int ns       = LoadInt(S); 
  int nl       = LoadInt(S);
  int nstrings = LoadInt(S);
  int n        = ns + nl;
  int nlist    = 1<<luaO_ceillog2(ns);
  int i, extra;
  const char *p;  

  /* allocate dynamic resources and save in S for error path collection */
  S->TS      = luaM_newvector(L, n+1, TString *);
  S->TSlen   = n+1;
  S->buff    = luaM_newvector(L, nb, char);
  S->buffLen = nb;
  S->list    = luaM_newvector(L, nlist, TString *);
  S->listLen = nlist;
  memset (S->list, 0, nlist*sizeof(TString *));

  /* add the strings in the image file to LFS */
  for (i = 1; i <= nstrings; i++) {
    int tt = LoadByte(S);
    lua_assert((tt&LUAU_TMASK)==LUAU_TSTRING);
    int l = LoadInteger(S, tt) - 1; /* No NULL entry in list of TSs */
    LoadVector(S, getstr(cast(TString *, S->buff)), l);
    addTS(S, l, 0);
  }

  /* add the fixed strings to LFS */
  for (i = 0; (p = luaX_getstr(i, &extra))!=NULL; i++) {
    addTSnodup(S, p, extra);
  }
  addTSnodup(S, getstr(g->memerrmsg), 0);
  addTSnodup(S, LUA_ENV, 0);
  for (i = 0; (p = luaT_getstr(i))!=NULL; i++) {
    addTSnodup(S, p, 0);
  }
  /* check that the actual size is the same as the predicted */
  lua_assert(n == S->TSndx-1);
  
  S->fh->pROhash = StoreN(S, S->list, nlist);
  S->fh->nROuse  = ns;
  S->fh->nROsize = nlist;
  StoreFlush(S);
  S->buff    = luaM_freearray(L, S->buff, nb);
  S->buffLen = 0;
  S->list    = luaM_freearray(L, S->list, nlist);
  S->listLen = 0;
}


static void LoadAllProtos (LoadState *S) {
  lua_State *L = S->L;
  ROTable_entry eol = {NULL, LRO_NILVAL};
  int i, n = LoadInt(S);
  S->pv    = luaM_newvector(L, n, Proto *);
  S->pvLen = n;
  
  /* Load Protos and store addresses in the Proto vector */
  for (i = 0; i < n; i++) {
    S->pv[i] = LoadFunction(S, luaF_newproto(L), NULL);
  }
  /* generate the ROTable entries from first N constants; the last is a timestamp */
  lua_assert(n+1 == LoadInt(S));
  ROTable_entry *entry_list = cast(ROTable_entry *, StoreGetPos(S));
  for (i = 0; i < n; i++) {
    lu_byte tt_data = LoadByte(S);
    lua_assert((tt_data & LUAU_TMASK) == LUAU_TSTRING);
    ROTable_entry me = {getstr(LoadString2(S, tt_data)), LRO_LUDATA(S->pv[i])};
    StoreN(S, &me, 1);
  }
  StoreN(S, &eol, 1);
  /* terminate the ROTable entry list and store the ROTable header */
  ROTable ev = { (GCObject *)1, LUA_TTBLROF, LROT_MARKED,
                 (lu_byte) ~0, n, NULL, entry_list};
  S->fh->protoROTable = StoreN(S, &ev, 1);
  /* last const is timestamp */
  S->fh->timestamp = LoadInteger(S, LoadByte(S));
}


static void undumpLFS(lua_State *L, void *ud) {
  LoadState *S = cast(LoadState *, ud);
  void *F = S->Z->data;
  char *startLFS = StoreGetPos(S);

  luaN_setFlash(F, sizeof(LFSHeader));
  S->fh->flash_sig = FLASH_SIG;
  if (LoadByte(S) != LUA_SIGNATURE[0])
    error(S, "invalid header in");
  checkHeader(S, LUAC_LFS_IMAGE_FORMAT);
  S->fh->seed = LoadInteger(S, LoadByte(S));
  checkliteral(S, LUA_STRING_SIG,"no string vector");
  LoadAllStrings (S);
  checkliteral(S, LUA_PROTO_SIG,"no Proto vector");
  LoadAllProtos(S);
  S->fh->flash_size = cast(char *, StoreGetPos(S)) - startLFS;
  luaN_setFlash(F, 0);
  StoreN(S, S->fh, 1);
  luaN_setFlash(F, 0);
  S->TS = luaM_freearray(L, S->TS, S->TSlen);
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
  luaM_freearray(L, S.TS, S.TSlen);
  luaM_freearray(L, S.buff, S.buffLen);
  luaM_freearray(L, S.list, S.listLen);
  luaM_freearray(L, S.pv, S.pvLen);
  L->nny--;
  return status;
}
