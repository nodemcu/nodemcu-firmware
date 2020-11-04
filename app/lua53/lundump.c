/*
** $Id: ldump.c,v 2.37.1.1 2017/04/19 17:20:42 roberto Exp $
** save precompiled Lua chunks
** See Copyright Notice in lua.h

This NodeMCU version of undump is a reimplementation of the standard Lua
version,and significantly extends the standard functionality by supporting
a number of load targets:

  (A)  RAM heap space. This is essentially standard Lua functionality where
       the Proto data structures and TStrings are created directly in RAM and
       comply with Lua GC assumptions, so the GC will collect unused storage
       in the case of a thrown error.

  (B)  LFS memory. This can be accessed programmatically during VM execution
       as ROM, and so in not writeable in this mode; however it can updated
       through a block erase + a serial write API.

Lua Flash Store (LFS) is design to support running Lua applications on IoT-
class processors which typically employ a modified Hardward architecture with
limited RAM and the ability to execute code and fetch constant data from flash
memory.  It is described in detail in a separate LFS whitepaper.

In the case of mode B, the load must support sub-variants:

-  Multi target execution. Though this LFS functionality is primarily aimed at
   IoT-class chipsets, it must also be capable of being executed on a standard
   CPU architecture (mainly for testing purposes).

-  Cross-target support.  Here the load is executed on, say, a PC-class. host
   CPU, but the generated LFS is designed to be loaded onto an IoT firmware
   build as part of provisioning.  Here any generated address references relate
   to the target environment, and are therefore invalid if executed in the host
   context.  This is further complicated in the case of 64-bit hosts, where any
   structures must be repacked to reflect the 32-bit target architecture.

-  Multi-pass loading. Because of IoT RAM limitations during IoT, LFS loading
   is done using two passes:

   -  The first validates that the file formats and computes some sizing
      parameters. The actual writes are dummies and so leave the LFS region
      unchanged. This can be executed within a normal Lua VM context. The
      process is protected so errors are returned to the caller.

   -  The second pass executes within the clean environment of the Lua RTS
      startup, that is before any library modules have been initialised or
      applications code executed.  Any errors will result in start-up being
      aborted.

A single largely unified source code base is used to support these two modes
and all sub-variants. It's scope is the creation of the Proto and TStrings
records and the other Lua internal data structures used to represent the Proto
hierarchies to be loaded.

The actual details of the LFS format are left to the calling function, and
likewise the implementation of the erase and serial R/W API are encapsulated
in a CB function provided by the caller.

The dump and undump functions now support both single top level function (TLF)
and multi-TLF dump streams.

All of this code complies with LUA_CODE module coding conventions.

LFS encoding add some extra design constraints:

-  It uses a simple serial allocation scheme where each separate resource (for
   example a Proto record or TString) is written atomically to the LFS at the
   next available location. Since these writes are both serial and atomic, all
   address references must be backward and therefore well determined when
   writing each object. This isn't the case with the standard Lua dump format,
   but this has been achieved with some simple reordering.

-  Proto records and the ROstrt reference other record hierarchies and these
   must therefore be cached in RAM in a way that can be collected on thrown
   error (for example allocation of heap resources can throw an out-of-memory
   (E:M) error). In RAM mode, all discarded resources are GC compliant and are
   recovered by normal Lua GC. For LFS modes, stack allocations are used where
   practical (e.g. Protos); other resources such as the ROstrt are maintained
   in the LoadState structure and these resources can be freed outside the
   protected call to the loader.

-  Resources written to LFS cannot then be reliably read programmatically until
   a cache flush has been carried out to ensure read coherence.  The actual
   flush call is encapsulated the the flash write CB, but the code logic must
   honour this before-and-after dependency.

-  The ROstrt uses chain linking to handle hash table overflow; TStrings with
   the same hash index are chained. The chains are dependent on ROstrt size,
   so the hash must be statically sized because TString links can't be updated
   in LFS modes. A two-pass approach circumvents this issue: the first pass
   undump computes the ROstrt size, the second pass using this calculated
   size to build the ROstrt with the TString records using this fixed size.

-  LFS supports the assembly of multiple load files into a single LFS region.
   The details of this assembly are largely deferred to the calling process,
   with this module limiting its scope to the output of the record hierarchies.
   However the API has been extended to separate out the new and close state
   calls as bookends to multiple undump calls, one per load stream.
*/

#define ldump_c
#define LUA_CORE

#include "lprefix.h"

#include <stddef.h>
#include <string.h>
#include <alloca.h>
#include <stdio.h> /*DEBUG*/

#include "lua.h"
#include "lapi.h"
#include "lgc.h"
#include "lmem.h"
#include "lobject.h"
#include "lfunc.h"
#include "lstate.h"
#include "lstring.h"
#include "ltable.h"
#include "lundump.h"
#include "lvm.h"

static TString *LFS_newlstr (LoadState *S, const char *s, size_t l, int extra);
static ROTable *LFS_createIndex (LoadState *S, const ROTable *indexMeta);
static void     LFS_flushcache (LoadState *S);
static Proto   *LoadFunction (LoadState *S, TString *psource);

typedef struct LoadState {
/* Set by initialisation */
  lua_State   *L;               /* lua_State for this undump */
  luaU_Writer  flashWriter;     /* Parameter usage slightly different */
  void        *flashWriterData; /* from standard writer; see lundump.h */
  int          mode;            /* Load mode: one of RAM, LFS or LFSA */
  int          pass;            /* Set for LFS modes 1 or 2 */
  unsigned int seed;
/* Set by undump call */
  ZIO*         Z;           /* ZIO block for source reader */
  const char  *name;        /* name of undump for error reporting */
  int          strip;       /* Debug strip level = 0, 1 or 2 */
/* Parameters to be exported on close */
  Table       *t;           /* Table caching future contents of LFS ROTable */
  int          nt;          /* Number of entries in t */
  Proto       *protoHead;   /* (LFS) Head of Proto linked list */
  TString     *shortTShead; /* (LFS) Head of short TS linked list */
  TString     *longTShead;  /* (LFS) Head of long TS linked list */
  TString     *rTSbarrier;  /* (LFS) TS below this are safe to read */
  stringtable  ROstrt;      /* RAM-cached version of ROstrt used for LFS load */
/* Internal State */
  unsigned int crc;       /* CRC of file content read so far */
  TString    **ts;        /* ts[0:tsLen] is the vector of TStrings referenced */
  int          tsLen;     /* in the current file being undumped. ts[0] = NULL */
  int          tsTot;     /* Sum of tsLen counts */
  char        *str;       /* Scratch string buffer uses to build LFS TStrings */
  size_t       strLen;    /* maximum string length in current module */
  struct protomap {
  TString     *name;
  Proto       *p;
  }           *pv;        /* Named Protos in current input stream */
  int          pvLen;     /* Length of pv */
  struct strtlink {
  TString     *nfTShead;
  TString     *rTShead;
 }            *map;       /* used to resolve cross image TString duplications */
  int        mapNdx;      /* growvector index and doubling size */
  int        mapLen;
  void      *dummyROaddr;
#ifdef LUA_USE_HOST
  Table      *ROstrtMap;  /* LFSA mode map of string -> RO TString addr */
#endif
} LoadState;

#define MODE_RAM      0   /* Loading into RAM */
#define MODE_LFS      1   /* Loading into a locally executable LFS */
#define MODE_LFSA     2   /* (Host only) Loading into a shadow ESP image */
#define inRAM(S) ((S)->mode == MODE_RAM)

static l_noret error(LoadState *S, const char *why) {
  luaO_pushfstring(S->L, "%s (compiled): %s", S->name, why);
  luaD_throw(S->L, LUA_ERRSYNTAX);
}

/*
** Byte stream Load functions
*/
static void LoadBlock (LoadState *S, void *b, size_t size) {
  lu_int32 left = luaZ_read(S->Z, b, size);
  if ( left != 0)
    error(S, "file truncated");
  S->crc = luaU_zlibcrc32(b, size, S->crc);
//*DEBUG*/ static size_t n = 0; n+=size; printf("Block:%8zu %8zu 0x%08x\n",size,n,S->crc);
}

static inline lu_byte LoadByte (LoadState *S) {
  lu_byte x;
  LoadBlock(S, &x, 1);
  return x;
}

/* (Unsigned) integers are typically small so are multi-byte encoded */
/* a single 0NNNNNNN or a multibyte (1NNNNNNN)* 0NNNNNNN */
static lua_Integer LoadIntTT (LoadState *S, lu_byte tt_data) {
  lu_byte b;
  lua_Integer x = tt_data & LUAU_DMASK;
  if (tt_data & 0x80) {
    do { b = LoadByte(S); x = (x<<7) + (b & 0x7f); } while (b & 0x80);
  }
  return x;
}

static lua_Integer LoadCount (LoadState *S) {
  int x = LoadIntTT(S, 0x80);
//*DEBUG*/ printf("Count:%u\n",x);
  return x;
}

static TString *LoadString (LoadState *S) {
  lu_byte tt = LoadByte(S);
  if ((tt & LUAU_TMASK) != LUAU_TSTRING)
    luaD_throw(S->L, LUA_ERRSYNTAX);
  lua_Integer i = LoadIntTT(S, tt);
//*DEBUG*/ printf("String:%u:%s\n",i, (i && S->ts[i]) ? getstr(S->ts[i]):"NULL");
  return S->ts[i];
}


static void LoadConstant (LoadState *S, TValue *o) {
  lu_byte tt = LoadByte(S);
  int i;
  switch (tt & LUAU_TMASK) {
  case LUAU_TNIL:
    setnilvalue(o);
    break;
  case LUAU_TBOOLEAN:
    setbvalue(o, (tt != LUAU_TBOOLEAN));
    break;
  case LUAU_TNUMFLT : {
    lua_Number fv;
    LoadBlock(S, &fv, sizeof(fv));
    setfltvalue(o, fv);
    break;
    }
  case LUAU_TNUMPINT:
    setivalue(o, LoadIntTT(S, tt));
    break;
  case LUAU_TNUMNINT:
    setivalue(o, -LoadIntTT(S, tt)-1);
    break;
  case LUAU_TSTRING:
    i = LoadIntTT(S, tt); /* always need to read this int */
   /* Only S->ts[1] points a valid TS on dummy pass 1 */
    setsvalue(S->L, o, S->ts[(S->pass==1) ? 1: i]);
    break;
  default:
      luaD_throw(S->L, LUA_ERRSYNTAX);
  }
}

/*
** The flash API is encapsulated in the FLASH_WRITE(S, b, l) macro, where `b`
** is the buffer to be written and `l` is the length to be written (this points
** to a dummy function on pass 1). These lengths are word aligned, except in
** the case of 64 bit host builds in LFS mode when these are size_t aligned.
** Unlike the standard dump usage, this writer returns a void * value which
** is the flash location written to.
*/

#define FLASH_WRITE(S,b,l) S->flashWriter(S->flashWriterData, b, l)
#define FLASH_SYNC(S) FLASH_WRITE(S, NULL, ~(size_t)0)
#define FLASH_CURR(S) FLASH_WRITE(S, NULL, 0)

/*
** The following compression maps are used in Store() ONLY in the case of
** 64-bit hosts to repack structs to adjust for 64-bit->32-bit points in
** absolute mode LFS creation.  These must match the definitions in lobject.h
*/
typedef Proto *pProto;
#ifdef LUA_USE_HOST64
# define FMT_DECL   , const char *pack
# define FMT_PACK   , pack
# define FMT_ARG(t) , FMT_ ## t
# define LFS_ABS_MODE(S)  (S->mode == MODE_LFSA)
# define FMT_LocVar        "Aww"
# define FMT_NULL          NULL
# define FMT_Instruction   "w"
# define FMT_Proto         "AwwwwwwwwwwAAAAAAAAA"
# define FMT_pProto        "A"
# define FMT_ROTable       "AWAA"
# define FMT_ROTable_entry "AWA"
# define FMT_TString       "AwwA"
# define FMT_UTString      ((sizeof(TString) == sizeof(UTString) || \
                             S->mode == MODE_LFSA) ? FMT_TString : \
                                                     FMT_TString "SSSSS")
# define FMT_pTString      "A"
# define FMT_TValue        "WA"
# define FMT_Upvaldesc     "AW"
#else
# define FMT_DECL
# define FMT_PACK
# define FMT_ARG(t)
# define LFS_ABS_MODE(S)  (0)
#endif

static void *Store (LoadState *S,
                    void *vec, int ndx,     /* dest is vec[ndx] in len chunks */
                    const void *src, size_t len           /* src in len chunk */
                    FMT_DECL) {   /* One of above pack formats on HOST builds */
  lu_byte *dest;                              /* returns addr of dest element */
  if (inRAM(S)) {
    dest = cast(lu_byte *, vec) + (ndx*len);
    if (dest != src)
      memcpy(dest, src, len);
#ifdef LUA_USE_HOST64
  } else if (S->mode == MODE_LFSA && pack) {
   /*
    * 64-bit hosts have two LFS modes. The default LFS mode is for writing to a
    * when writing to native LFS where the address pointers are 64 bit and any
    * pointers are word aligned.  LFSA mode is use to build "shadow" 32 bit LFS
    * based at an ESP absolute address. In this case, only the LS 32 bits of a
    * pointer are used (and never for actual addressing in host execution).
    */
    lu_int32 *w;
    const char *f = pack;
    dest = FLASH_CURR(S);
    for (w = (lu_int32 *)src; *f; w++, f++ ) {
      FLASH_WRITE(S, w, sizeof(*w));
      if (*f == 'A' || *f == 'W') /* Addr or word followed by alignment fill */
        w++;
    }
    lua_assert((w - (lu_int32 *)src) * sizeof(*w) == len);
#endif
  } else {
    dest = FLASH_WRITE(S, src, len);
  }
  return dest;
}

#define STOREI(s,t,v,o)    ((t *) Store(s, (v), i, &(o), sizeof(t) FMT_ARG(t)))
#define STORE(s,t,po,o)    ((t *) Store(s, &(po), 0, &(o), sizeof(t) FMT_ARG(t)))
#define STOREBUF(s,po,o,n) ((lu_byte *) Store(s, (po), 0, (o), n FMT_ARG(NULL)))
#define STORENEXT(s)       FLASH_CURR(s)

static void LoadStrings (LoadState *S) {
  int i;
  i         = LoadCount(S) + 1;  /* buffer just in case luaM_newvector throws */
  S->str    = luaM_newvector(S->L, i, char);
  S->strLen = i;
  i         = LoadCount(S) + 1;
  S->ts     = luaM_newvector(S->L, i, TString *);
  S->tsLen  = i;
  S->tsTot += i;
  S->ts[0]  = NULL;
  for (int i = 1; i < S->tsLen; i++) {
    size_t l = LoadCount(S)-1;
    lua_assert(l < S->strLen);
    LoadBlock(S, S->str, l);
    S->str[l] = '\0';
    S->ts[i] = inRAM(S) ? luaS_newlstr(S->L, S->str, l) :
                           LFS_newlstr(S, S->str, l, 0);
    if (i <= S->pvLen)
      S->pv[i-1].name = (S->pass == 1) ? luaS_newlstr(S->L, S->str, l) :
                                         S->ts[i];
//*DEBUG*/ printf("%5u:%4zu:%s\n",i,l, S->str);
  }
  if (!inRAM(S)) {
    if (S->pass == 1)
      S->ts[1] = G(S->L)->tmname[0]; /* just need 1 valid TS for algo to work */
    LFS_flushcache(S);
  }
}


static Proto **LoadSubProtos (LoadState *S, int n, TString *source) {
 /*
  * The p vector must be cached in RAM to avoid p[i] stores being interleaved
  * with the Proto stores; so store the Protos, then store the p[i] entry.
  * In RAM mode, then the vector is allocated from heap and will become the
  * f->k vector, otherwise it is allocated on the stack, copied to LFS and
  * dropped on return.
  */
  Proto **p = (inRAM(S)) ? luaM_newvector(S->L, n, Proto *)
                         : alloca(n*sizeof(p[0]));
  int i;
  for (i = 0; i< n; i++)
    p[i] = LoadFunction(S, source);           /* load and store the sub-Proto */
  if (inRAM(S))
    return p;
  Proto **q = (Proto **) STORENEXT(S );
  for (i = 0; i< n; i++)                             /* Send p[0..n-1] to LFS */
    STOREI(S, pProto, p, p[i]);      /* This repacks in the case of LFSA mode */
  return q;
}

static void LoadLineInfo (LoadState *S, int n, Proto *f) {
  char *b = (inRAM(S)) ? luaM_newvector(S->L, n, lu_byte) : alloca(n);
  LoadBlock(S, b, n);
  f->lineinfo = STOREBUF(S, b, b, n);
  f->sizelineinfo = n;
}

static int aux_newvector (LoadState *S, size_t len, void **v) {
  int n = LoadCount(S);
  if (inRAM(S)) {
    *v = luaM_reallocv(S->L, NULL, 0, n, len);
    memset(*v, 0, n * len);
  } else {
    *v = (void *)STORENEXT(S );
  }
  return n;
}

#define OVER(t,v) \
  f->size ## v = n = aux_newvector(S, sizeof(t), (void**)&(f->v)); \
  for (i = 0; i< (n); i++)

static Proto *LoadFunction (LoadState *S, TString *source) {
  /* This function MUST be aligned to ldump.c:DumpFunction() */
//*DEBUG*/ static int cnt = 0; cnt++;
  int i, n;
  Proto F = {0};
  Proto *f = (inRAM(S)) ? luaF_newproto(S->L) : &F;
  f->source = LoadString(S);
  if (!f->source)
    f->source = source;
  f->sizep           = LoadCount(S);
  f->p               = LoadSubProtos(S, f->sizep, f->source);
  f->linedefined     = LoadCount(S);
  f->lastlinedefined = LoadCount(S);
//*DEBUG*/ printf("Function from %u to %u\n", f->linedefined, f->lastlinedefined);
  f->numparams       = LoadByte(S);
  f->is_vararg       = LoadByte(S);
  f->maxstacksize    = LoadByte(S);

  OVER(Instruction, code) {
    Instruction o;
    LoadBlock(S, &o, sizeof(o));
    STOREI(S, Instruction, f->code, o);
  }
  OVER(TValue, k) {
    TValue o;
    LoadConstant(S, &o);
    STOREI(S, TValue, f->k, o);
  }
  OVER(Upvaldesc, upvalues) {
    Upvaldesc uv = {LoadString(S), LoadByte(S), LoadByte(S)};
    STOREI(S, Upvaldesc, f->upvalues, uv);
  }
  OVER(LocVar, locvars) {
    LocVar lv = {LoadString(S), LoadCount(S), LoadCount(S)};
    STOREI(S, LocVar, f->locvars, lv);
  }
  LoadLineInfo(S, LoadCount(S), f);

  if (!inRAM(S)) {
    f->next = (GCObject *)S->protoHead;
    f = STORE(S, Proto, F, F);
    S->protoHead = f;
   /* Note that f is correctly located in RAM mode so no STORE needed. */
  } else if (S->strip > 0) {
   /* In RAM mode metadata stripping is also honoured to save memory. */
    luaU_stripdebug (S->L, f, S->strip, 0);
  }
  return f;
}

#define MAX_LITERAL (sizeof(LUA_SIGNATURE) + sizeof(LUAC_DATA))
static void checkBuffer (LoadState *S, const char *s, size_t l, const char *msg) {
  char buff[MAX_LITERAL];
  LoadBlock(S, buff, l);
  if (memcmp(s, buff, l) != 0)
    error(S, msg);
}

#define checkLiteral(S,s,m) checkBuffer(S,s,strlen(s),m)
#define checkbyte(S,v,e) if (LoadByte(S)!=(v)) error(S, e)
static void checkHeader (LoadState *S, int format) {
  TValue num;
  checkLiteral(S, LUA_SIGNATURE+1, "not a Lua compiled file");
  checkbyte(S, LUAC_VERSION, "Lua version mismatch");
  checkbyte(S, format, "format version mismatch");
  checkLiteral(S, LUAC_DATA, "file corrupted");
  checkbyte(S, sizeof(int), "int size mismatch");
  checkbyte(S, sizeof(Instruction), "Instruction size mismatch");
  checkbyte(S, sizeof(lua_Integer), "lua_Integer size mismatch");
  checkbyte(S, sizeof(lua_Number), "lua_Number size mismatch");
/* Note that we multi-byte encoded integers so no need to check size_t or endian */
  LoadConstant(S, &num);
  if (nvalue(&num) != LUAC_NUM)
    error(S, "float format mismatch");
}

/*
** Load precompiled chunk(s) to support standard LUA_API load functions. This
** supports both single top level funciton (TLF) and multi-TLF dump streams.  In
** the case of a single TLF a Closure is added to ToS else an array of Closures.
** Note that this protected call is used to recover all working resources even
** if an error is thrown.
*/
static void protectedLoad (lua_State *L, void *ud) {
  LoadState *S = (LoadState *) ud;
  checkHeader(S, LUAC_FORMAT);
  int n = LoadByte(S);
  S->pv = luaM_newvector(L, n, struct protomap);
  memset(S->pv, 0, n*sizeof(struct protomap));
  S->pvLen = n;
  LoadStrings(S);
  checkLiteral(S, LUA_PROTO_SIG, "string table corrupted");
  for (int i = 0; i < n; i++) {
    S->pv[i].p = LoadFunction(S, NULL);
  }
  unsigned int crc = ~S->crc;
  checkBuffer(S, (char *)&crc, sizeof(crc), "CRC mismatch");
  if (inRAM(S) && n == 1) {
    /* return LCLosure on ToS if single TLF in RAM mode */
    LClosure *cl = luaF_newLclosure(L, S->pv[0].p->sizeupvalues);
    cl->p = S->pv[0].p;
    luaF_initupvals(L, cl);
    setclLvalue(L, L->top, cl);
  } else {
    /* return Array on ToS; values are LClosures in RAM mode */
    /*                      values are LwUD of Proto* in LFS modes */
    TValue key, *node;
    if (S->t == NULL)
      S->t = luaH_new(S->L);
    luaH_resize(L, S->t, 0, S->nt + n);
    for (int i = 0; i < n; i++) {
      setsvalue(L, &key, S->pv[i].name);
      node = luaH_set(L, S->t, &key);
      if(!ttisnil(node))
        S->nt++;  /* Allow for duplicated names */
      if (inRAM(S)) {
        LClosure *cl = luaF_newLclosure(L, S->pv[i].p->sizeupvalues);
        cl->p = S->pv[i].p;
        luaF_initupvals(L, cl);
        setclLvalue(L, node, cl);

      } else { /*LFS modes */
        setpvalue(node, S->pv[i].p);
      }
    }
    sethvalue(L, L->top, S->t);   /* Set ToS to array of Closures */
  }
  luaD_inctop(L);
}

/*
** luaU_undump() preserves standard Lua functionality by returning a Lua
** closure at ToS if single TLF dump stream is loaded and returning a Proto
** at ToS. Used by  lua_dofile() etc.  The dump stream can also contain a
** multiple TLF in which case an array of {name=closure} is returned at ToS.
*/
void luaU_undumpx (lua_State *L, ZIO *Z, const char *name, LoadState *S) {
  int init = (S == NULL);

  if (init)
    S = luaU_LoadState(L, NULL, 0, NULL, NULL, MODE_RAM, 0);
  S->name = (*name == '@' || *name == '=') ? name + 1 :
            ((*name == LUA_SIGNATURE[0]) ? "binary string" : name);
  S->Z    = Z;
  S->crc = luaU_zlibcrc32(LUA_SIGNATURE, sizeof(char), ~0);


  /* Protected of call of Load Proto to tidy up allocs on error */
  int status = luaD_pcall(L, protectedLoad, S, savestack(L, L->top), 0);
  S->pv   = luaM_freearray(S->L, S->pv,  S->pvLen);  S->pvLen  = 0;
  S->str  = luaM_freearray(S->L, S->str, S->strLen); S->strLen = 0;
  S->ts   = luaM_freearray(S->L, S->ts,  S->tsLen);  S->tsLen  = 0;
  S->map  = luaM_freearray(S->L, S->map, S->mapLen); S->mapLen  = 0; S->mapNdx  = 0;
  if (init)
    luaU_closeLoadState(S, NULL, NULL);
  if (status != LUA_OK)    /* error while running protectedSave? */
    luaD_throw(L, status); /* re-throw error */
}


void luaU_addstrings(LoadState *S, const char **s, lu_byte *extra) {
  for (; *s; s++) {
    S->tsTot++;
    LFS_newlstr(S, *s, strlen(*s), (extra ? *extra++ : 0));
  }
  LFS_flushcache(S);
}

static void *dummyWriter (void *ud, const void *b, size_t sz) {
  char **p = (char **) ud;
  sz = (sz + sizeof(size_t) - 1) & (~(sizeof(size_t)-1));
  *p += sz;
  return *p;
}

/*
** In the case of LFS loads, these can be multi-file so some load state must
** be passed between calls to luaU_undumpx, hence this state creation and the
** companion close. LFS load is two pass with the ROstrtSize set on pass 2.
*/
LoadState *luaU_LoadState (lua_State *L, Table *t, int ROstrtSize,
                           luaU_Writer fw, void *fwd,
                           int mode, unsigned int seed) {

  LoadState *S = luaM_new(L, LoadState);
  memset(S, 0, sizeof(LoadState));
  S->mode = mode;
  S->L    = L;
  S->seed = seed;
  S->t    = t;
  if (!inRAM(S)) {            /* S->pass etc defaults to 0 for    */
    if (ROstrtSize == 0) {
      S->pass = 1;
      S->flashWriter     = &dummyWriter;
      S->flashWriterData = &(S->dummyROaddr);
      S->dummyROaddr     = NULL;
    } else {
      S->pass = 2;
      S->ROstrt.size  = ROstrtSize;
      S->ROstrt.nuse  = 0;
      S->ROstrt.hash  = luaM_newvector(L, ROstrtSize, TString *);
      memset(S->ROstrt.hash, 0, ROstrtSize*sizeof(TString *));
      S->flashWriter  = fw;
      S->flashWriterData = fwd;
      S->rTSbarrier   = STORENEXT(S);
  #ifdef LUA_USE_HOST
      if (S->mode == MODE_LFSA)
        S->ROstrtMap = luaH_new(L); /* GC must be stopped to prevent GC */
  #endif
    }
  }
  return S;
}

typedef struct TString *pTString;

void luaU_closeLoadState (LoadState *S, luaU_data *res, const ROTable *indexMeta) {
  if (!inRAM(S)) {
    ROTable *pt = NULL;
    TString **index = NULL;
    if (S->pass == 2) {
      pt = LFS_createIndex(S, indexMeta);
      index = cast(TString **, STORENEXT(S));
      for (int i = 0; i < S->ROstrt.size; i++)
        STOREI(S, pTString, NULL,S->ROstrt.hash[i]);
      S->ROstrt.hash = luaM_freearray(S->L, S->ROstrt.hash, S->ROstrt.size);
    }
    if(res) {
      res->ROstrt.hash  = index;
      res->ROstrt.size  = S->ROstrt.size;
      res->ROstrt.nuse  = S->ROstrt.nuse;
      res->protoROTable = pt;
      res->protoHead    = S->protoHead;
      res->shortTShead  = S->shortTShead;
      res->longTShead   = S->longTShead;
      res->tsTot        = S->tsTot;
     }
  }
  luaM_free(S->L, S);
}


LUAI_FUNC const char *luaU_getbasename (lua_State *L, const TString *filename,
                                        size_t *len) {
  const char *fn = getstr(filename);
  const char *s = strrchr(fn, '/');
  s = s ? s :  strrchr(fn, '\\');
  s = s ? s + 1 : fn;
  while (*s == '.') s++;
  const char *e = strchr(s, '.');
  if (len)
    *len = e ? e - s : strlen(s);
  return s;
}


/*============================================================================**
** NodeMCU extensions for LFS support and Loading.
**============================================================================*/

/*
** Building TStrings in RAM essentially uses standard Lua core functionality.
** LFS is a more complex implementation because:
**
** 1)  Unlike core RAM-based TStrings which support flexible R/W access, any
**     written TStrings can't be reread until after cache-flush barrier allows
**     coherent read access to written LFS resources.  LFS supports multi-file
**     load, and this flush occurs after each file load.
**
** 2)  Each file contains a preamble list of *unique* TStrings used in the
 **     Protos in the file; however the TString might already exist, loaded from
 **     a previous file, hence any TString hash chain (ordered latest first) can
 **     contain non-flushed TS (nfTS) followed by readable TS (rTS).  Any
**     potential match can only be in the rTS linked chain, so the resolution
**     must skip over the nfTS, and these skip links must be cached in RAM.
**
**     This could have been implemented by having a second nfTS vector, but
**     this would double the RAM needed to hold the RAM-cached ROstrt hash
**     vector. The number of mixed nfTS + rTS chains is small and can be
**     handled by a dynamically-sized set of [nfTS head, rTS head] buckets with
**     the ROstrt referencing this bucket.  This encoding is denormalised into
**     the RTstrt hash entry (*o).  This is a little nasty but saves RAM.
**
     **     * If (size_t) (*o) < max bucket count then this refers to a bucket.
**       This is best an index since the bucket list can grow and be relocated.
**     * If *o < last flushed TS then is an rTS reference
**     * Otherwise it is an nfTS reference and we can't get a repeat match.
**
** 3)  Since TSs written to LFS can't be updated, the ROstrt can't be resized,
**     so a two-pass read is done: Pass 1 computes the ROstrt size and skips
**     all TString building; Pass 2 builds the TS using a fixed size hash.
**
** 4)  There is also a special LFSA mode where the LFS region uses 32-bit
 **     target based address even on a 64-bit host and any records are repacked
**     so pointer go from size_t to 32-bit.  In this case the LFS is effectively
**     read-only but RAM isn't an issue so an on-host table is used to map
 **     strings to their resolved absolute address.
*/

#define isstrtlink(o)    (*cast(size_t *, o) <= S->mapNdx)
#define isnfTS(o)        (*o > S->rTSbarrier)
#define strtlink(o)      S->map[*cast(size_t *, o) - 1]
#define setstrtlink(o,n) {*cast(size_t *, o) = (n) + 1;}

static TString *LFS_newlstr (LoadState *S, const char *s, size_t l, int extra) {
  UTString sts = {0};               /* This must be Lua Test Suite compatible */
                                    /* hence the use of UTString wrappers :-( */
  TString *ts, *tsnext, *rts, **o;
  if (S->pass == 1)
    return NULL;        /* Nothing to do on Pass 1 or string already resolved */

  ts         = &sts.tsv;
  ts->marked = bitmask(LFSBIT) | BIT_ISCOLLECTABLE;
  ts->extra  = extra;

    if (l > LUAI_MAXSHORTLEN) {  /* long TS are simple: no de-duplication done. */
    ts->tt       = LUA_TLNGSTR;
    ts->shrlen   = 0;
    ts->u.lnglen = l;
    ts->hash     = S->seed;
    luaS_hashlongstr(ts);                       /* sets hash and extra fields */
    ts->next      = (GCObject *)S->longTShead;
    ts            = STORE(S, TString, sts, sts);
    STOREBUF(S, NULL, s, l+1);
    S->longTShead = ts;
  } else {                                                    /* short string */
    ts->hash    = luaS_hash(s, l, S->seed);
    o           = S->ROstrt.hash + lmod(ts->hash, S->ROstrt.size);
    tsnext      = *o;
#ifdef LUA_USE_HOST
    if (S->mode == MODE_LFSA) {
      TString *ts1 = luaS_newlstr(S->L, s, l);
      const TValue *o = luaH_getstr(S->ROstrtMap, ts1);   /* key uses RAMstrt */
      if (!ttisnil(o))
        return tsvalue(o);
    } else {
#endif
   /*
    * The hash slot points to a 0 or more TS in a chain, with 4 chain cases:
    *  (A) NIL; (B) nfTS chain; (C) rTS chain; (D) nfTS + rTS chain.  Cases
    *  (C + D) need might have an existing match. (A + B) are straight updates.
    *  (C) needs a new map bucket.  (C + D) update the bucket nfTS head.
    */
    if (*o == NULL || isnfTS(o)) {
      rts = NULL;                                        /* Case A and Case B */
    } else if (!isstrtlink(o)) {
      rts = *o;                                                     /* Case C */
    } else {
      rts    = strtlink(o).rTShead;                                 /* Case D */
      tsnext = strtlink(o).nfTShead;
    }
    for (; rts != NULL; rts = rts->u.hnext) {               /* Scan rTS chain */
      if (l == getshrlen(rts) &&
           memcmp(s, getstr(rts), l * sizeof(char)) == 0)
        return rts;                                    /* return rTS if match */
    }
#ifdef LUA_USE_HOST
    }
#endif
    ts->tt      = LUA_TSHRSTR;
    ts->shrlen  = (lu_byte)l;
    ts->u.hnext = tsnext;      /* ignore NFTS / RTS distinctions for chaining */
    ts->next    = (GCObject *)S->shortTShead;
     ts          = (TString *) STORE(S, UTString, sts, sts);
    STOREBUF(S, NULL, s, l+1);
    S->shortTShead = ts;

     if (*o == NULL || isnfTS(o)) {
      *o = ts;                                           /* Case A and Case B */
    } else {
      if (!isstrtlink(o)) {                                         /* Case C */
        luaM_growvector(S->L, S->map, S->mapNdx, S->mapLen,
                        struct strtlink, 1024, "strt clashes");
        S->map[S->mapNdx].rTShead  = *o;
        setstrtlink(o, S->mapNdx++);
      }
      strtlink(o).nfTShead = ts;                         /* Case C and Case D */
    }
    S->ROstrt.nuse++;
  }

#ifdef LUA_USE_HOST
  if (S->mode == MODE_LFSA) {
    TValue tv, *node;
    setsvalue(S->L, &tv, luaS_newlstr(S->L, s, l));
    node = luaH_set(S->L, S->ROstrtMap, &tv);
    lua_assert(ttisnil(node));      /* key uses RAMstrt, value is LFS TS addr */
    setsvalue(S->L, node, ts);
  }
#endif
  return ts;
}

static void LFS_flushcache (LoadState *S) {
  TString **o = S->ROstrt.hash;
  FLASH_SYNC(S);         /* all non-flushed refs are now flushed and readable */
  for (int i = 0; i < S->ROstrt.size; i++, o++) {
    if (*o && isstrtlink(o))
      *o = strtlink(o).nfTShead;
    if (*o > S->rTSbarrier)  /* Reset the barrier to the highest TS processed */
      S->rTSbarrier = *o;
    lua_assert(*o != (TString *)0xabababababababab);
  }
  S->mapNdx = 0;
}

static ROTable *aux_LFSrotable (LoadState *S, ROTable_entry *ev, int size,
                                int hasindex, const ROTable *meta) {
  ROTable t;
  t.next      = (GCObject *)1;
  t.tt        = LUA_TTBLROF;
  t.marked    = 0;
  t.flags     = hasindex ? cast(lu_byte, 1<<TM_INDEX) : 0;
  t.lsizenode = size;
  t.metatable = cast(Table *, meta);
  t.entry     = ev;

  return STORE(S, ROTable, t, t);
}


/*
** Create the index ROTable. In the case of a sys + app LFS config, the
** appLFS ROTable has an index pointing to the sysLFS table, so the extra
** metatable also needs to be generated.
*/
static ROTable *LFS_createIndex (LoadState *S, const ROTable *indexMeta) {
  struct ROTable_entry e;
  const struct ROTable_entry *ev;
  struct ROTable_entry nl = {0};
  int cnt = 0;
  TValue tv[2];
  if (inRAM(S))
    return NULL;
  setnilvalue(cast(TValue *, &nl.value));
  if (indexMeta) {
    /* create {__index = meta, NULLVAL} entry vector } */
    e.key = getstr(LFS_newlstr(S, "__index", sizeof("__index")-1, 0));
    sethvalue(S->L, cast(TValue *,&e.value), cast(Table *, indexMeta));
    ev = STORE(S, ROTable_entry, e , e);
    STORE(S, ROTable_entry, nl, nl);
    /* and store the extra metatable */
    indexMeta = cast(const ROTable *, aux_LFSrotable(S, ev, 1, 1, NULL));
  }
  ev = cast(struct ROTable_entry *, STORENEXT(S));
  setnilvalue(tv);
  while (luaH_next(S->L, S->t, tv)) {
    TString *ts = tsvalue(tv);
    ts = LFS_newlstr(S, getstr(ts), tsslen(ts), 0);  // <<< is this needed
    lua_assert(ts);
    e.key = getstr(ts);                             // need LFSA compatible version
    setobj(S->L, cast(TValue *, &e.value), tv+1);
    STORE(S, ROTable_entry, e, e);
    cnt++;
  }
  e.key = NULL;
  setnilvalue((TValue *)&e.value);
  STORE(S, ROTable_entry, e, e);
  return aux_LFSrotable(S, ev, cnt, 0, indexMeta);
}


