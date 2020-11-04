
#define lnodemcu_c
#define LUA_CORE

#include "lua.h"
#include <string.h>
#include <stdlib.h>

#include "lobject.h"
#include "lstate.h"
#include "lapi.h"
#include "lauxlib.h"
#include "lfunc.h"
#include "lgc.h"
#include "llex.h"
#include "lstring.h"
#include "ltable.h"
#include "ltm.h"
#include "lnodemcu.h"
#include "lundump.h"
#include "lvm.h"
#include "lzio.h"

/*
** This is a mixed bag of NodeMCU-specfic additions that will probably need
** modification for other IoT targets.  It is split into the following sections:
**
**    *  POSIX vs VFS file and Platform abstraction
**    *  NodeMCU lua.h LUA_API extensions
**    *  NodeMCU lauxlib.h LUALIB_API extensions
**    *  ESP and HOST lua_debugbreak() test stubs
**    *  NodeMCU bootstrap to set up and to reimage LFS resources
**
** Just search down for //== or ==// to flip through the sections.
*/

#define byte_addr(p)    cast(char *,p)
#define byteptr(p)      cast(lu_byte *, p)
#define byteoffset(p,q) ((int) cast(ptrdiff_t, (byteptr(p) - byteptr(q))))
#define wordptr(p)      cast(lu_int32 *, p)
#define wordoffset(p,q) (wordptr(p) - wordptr(q))
#define max(a,b)        ((a)>(b) ? (a) : (b))

//=============== POSIX vs VFS file and Platform abstraction =================//
/*
** The NodeMCU firmware includes a platform abstraction layer and a virtual
** file system (VFS).  The following HOST and ESP variants define a set vfs
** macros and emulation calls so the following source compiles and functions
** on both platform variants.
*/
#ifdef LUA_USE_ESP

#include "platform.h"
#include "user_interface.h"
#include "vfs.h"

#  define l_mountfs() vfs_mount("/FLASH", 0)
#  define l_file(f)   int f
#  define l_zero(f)   f = 0
#  define l_open(f)   vfs_open(f, "r")
#  define l_close(f)  vfs_close(f)
#  define l_feof(f)   vfs_eof(f)
#  define l_read(f,b) vfs_read(f, b, sizeof (b))
#  define l_rewind(f) vfs_lseek(f, 0, VFS_SEEK_SET)

int luaopen_file(lua_State *L);
extern void dbg_printf(const char *fmt, ...);                // DEBUG
#undef printf
#define printf(...) dbg_printf(__VA_ARGS__)                  // DEBUG

#define FLASH_PAGE_SIZE INTERNAL_FLASH_SECTOR_SIZE

/* Erasing the LFS invalidates ESP instruction cache,  so doing a block 32Kb */
/* read is the simplest way to flush the icache, restoring cache coherency */
#define flush_icache(F) \
  UNUSED(memcmp(F->addr, F->addr+(0x8000/sizeof(*F->addr)), 0x8000));
#define unlockFlashWrite()
#define lockFlashWrite()

#else // LUA_USE_HOST

#include<stdio.h>                                             // DEBUG

#  define l_mountfs()
#  define l_file(f)   FILE *f
#  define l_zero(f)   f = NULL
#  define l_open(n)   fopen(n,"rb")
#  define l_close(f)  fclose(f)
#  define l_feof(f)   feof(f)
#  define l_read(f,b) fread(b, 1, sizeof (b), f)
#  define l_rewind(f) rewind(f)

/*
** The ESP implementation use a platform_XXX() API to provide a level of
** H/W abstraction.  The following functions and macros emulate a subset
** of this API for the host environment. LFSregion is the true address in
** the luac process address space of the mapped LFS region.  All actual
** erasing and writing is done relative to this address.
**
** In normal LFS emulation the LFSaddr is also set to this LFSregion address
** so that any subsequent execution using LFS refers to the correct memory
** address.
**
** The second LFS mode is used to create absolute LFS images for directly
** downloading to the ESP or including in a firmware image, and in this case
** LFSaddr refers to the actual ESP mapped address of the  ESP LFS region.
** This is a 32-bit address typically in the address range 0x40210000-0x402FFFFF
** (and with the high 32bits set to 0 in the case of 64-bit execution). Such
** images are solely intended for ESP execution and any attempt to execute
** them in a host execution environment will result in an address exception.
*/
#define LFS_SIZE         0x40000
#define FLASH_PAGE_SIZE   0x1000
#define FLASH_BASE       0x90000           /* Some 'Random' but typical value */
#define IROM0_SEG     0x40210000ul

void         *LFSregion = NULL;
static void  *LFSaddr   = NULL;
static size_t LFSbase   = FLASH_BASE;

extern int request_restart;
#define system_restart()   request_restart = 1

#ifdef __unix__
/* On POSIX systems we can toggle the "Flash" write attribute */
# include <sys/mman.h>
# define aligned_malloc(a,n) posix_memalign(&a, FLASH_PAGE_SIZE, (n))
# define unlockFlashWrite() mprotect(LFSaddr, LFS_SIZE, PROT_READ| PROT_WRITE)
# define lockFlashWrite() mprotect(LFSaddr, LFS_SIZE, PROT_READ)
#else
# define aligned_malloc(a,n) ((a = malloc(n)) == NULL)
# define unlockFlashWrite()
# define lockFlashWrite()
#endif /* __unix__ defined */

#define PLATFORM_RCR_FLASHLFS  4
#define platform_flash_phys2mapped(n) \
    ((ptrdiff_t)(((size_t)LFSaddr) - LFSbase) + (n))
#define platform_flash_mapped2phys(n) \
    ((size_t)(n) - ((size_t)LFSaddr) + LFSbase)
#define platform_flash_get_sector_of_address(n) ((n)>>12)

/* Emulate platform RCR for FLASHLFS command */
static int reboot_request = 0;
#define platform_rcr_read(id,s) \
  ((id)==PLATFORM_RCR_FLASHLFS && reboot_request) ? 0 : ~0
#define platform_rcr_write(id,rec,l) \
  reboot_request = ((id)==PLATFORM_RCR_FLASHLFS) ? 1 : reboot_request;

#define platform_rcr_delete(id) \
  reboot_request = ((id)==PLATFORM_RCR_FLASHLFS) ? 0 : reboot_request;

void luaN_setabsolute(lu_int32 addr) {
  LFSaddr = cast(void *, cast(size_t, addr));
  LFSbase = addr - IROM0_SEG;
}

static lu_int32 platform_flash_get_partition (lu_int32 part_id, lu_int32 *addr) {
  lua_assert(part_id == NODEMCU_LFS0_PARTITION);
  if (!LFSregion) {
    if(aligned_malloc(LFSregion, LFS_SIZE))
      return 0;
    memset(LFSregion, ~0, LFS_SIZE);
    lockFlashWrite();
  }
  if(LFSaddr == NULL)
    LFSaddr = LFSregion;
  *addr = LFSbase;
  return LFS_SIZE;
}

static void platform_flash_erase_sector(lu_int32 i) {
  lua_assert (i >= LFSbase/FLASH_PAGE_SIZE &&
              i < (LFSbase+LFS_SIZE)/FLASH_PAGE_SIZE);
  unlockFlashWrite();
  memset(byteptr(LFSregion) + (i*FLASH_PAGE_SIZE - LFSbase), ~(0), FLASH_PAGE_SIZE);
  lockFlashWrite();
}

static void platform_s_flash_write(const void *from, lu_int32 to, lu_int32 len) {
  lua_assert(to >= LFSbase && to + len < LFSbase + LFS_SIZE);  /* DEBUG */
  unlockFlashWrite();
  memcpy(byteptr(LFSregion) + (to-LFSbase), from, len);
  lockFlashWrite();
}
#define flush_icache(F)   /* not needed */

#endif /* LUA_USE_HOST or LUA_USE_ESP */


//================= NodeMCU LFS header and FLash State types =================//
/*
** The LFSHeader uses offsets rather than pointers to avoid 32 vs 64 bit issues
** during host compilation.  The offsets are in units of lu_int32's and NOT
** size_t, though clearly any internal pointers are of the size_t for the
** executing architectures: 4 or 8 byte.  Likewise recources are size_t aligned
** so LFS regions built for 64-bit execution will have 4-byte alignment packing
** between resources.
*/

typedef struct LFSHeader LFSHeader;

struct LFSHeader{
  lu_int32 flash_sig;     /* a standard fingerprint identifying an LFS image */
  lu_int32 flash_size;    /* Size of LFS image in bytes */
  lu_int32 seed;          /* random number seed used in LFS */
  lu_int32 timestamp;     /* timestamp of LFS build */
  lu_int32 nROuse;        /* number of elements in ROstrt */
  lu_int32 nROsize;       /* size of ROstrt */
  lu_int32 oROhash;       /* offset of TString ** ROstrt hash */
  lu_int32 protoROTable;  /* offset of master ROTable for proto lookup */
  lu_int32 protoHead;     /* offset of linked list of Protos in LFS */
  lu_int32 shortTShead;   /* offset of linked list of short TStrings in LFS */
  lu_int32 longTShead;    /* offset of linked list of long TStrings in LFS */
  short    nFiles;
  short    nPVs;
};


typedef struct LFSflashState {
  lua_State  *L;
  LoadState  *S;
  int         pass;               /* 1 or 2; also 0 implies 1 */
  lu_int32    seed;
  LFSHeader  *hdr;
  l_file(f);
  lu_int32   *addr;
  lu_int32    oNdx;               /* in size_t units */
  lu_int32    oChunkNdx;          /* in size_t units */
  lu_int32   *oBuff;              /* FLASH_PAGE_SIZE bytes */
  lu_int32    inNdx;              /* in bytes */
  lu_int32    addrPhys;
  lu_int32    size;
  lu_int32    allocmask;
  stringtable ROstrt;
  GCObject   *pLTShead;
  const char **saveStr;
  int         nSave;
  lu_byte     inBuff[BUFSIZ];
} LFSflashState;


//===================== NodeMCU lua.h API extensions =========================//
/*
** These are LUA_CORE components that extend the core C API for Lua and publicly
** declared in lua.h.  These are written using the same implementation rules as
** lapi.c
*/
LUA_API int lua_freeheap (void) {
#ifdef LUA_USE_HOST
  return MAX_INT;
#else
  return (int) platform_freeheap();
#endif
}

LUA_API int lua_pushstringsarray(lua_State *L, int opt) {
  stringtable *strt = NULL;
  int i, j = 1;

  lua_lock(L);
  if (opt == 0)
    strt = &G(L)->strt;
#ifdef LUA_USE_ESP
  else if (opt == 1 && G(L)->ROstrt.hash)
    strt = &G(L)->ROstrt;
#endif
  if (strt == NULL) {
    setnilvalue(L->top);
    api_incr_top(L);
    lua_unlock(L);
    return 0;
  }

  Table *t = luaH_new(L);
  sethvalue(L, L->top, t);
  api_incr_top(L);
  luaH_resize(L, t, strt->nuse, 0);
  luaC_checkGC(L);
  lua_unlock(L);

  /* loop around all strt hash entries */
  for (i = 0, j = 1; i < strt->size; i++) {
    TString *e;
    /* loop around all TStings in this entry's chain */
    for(e = strt->hash[i]; e; e = e->u.hnext) {
      TValue s;
      setsvalue(L, &s, e);
      luaH_setint(L, hvalue(L->top-1), j++, &s);
    }
  }
  return 1;
}

LUA_API void lua_createrotable (lua_State *L, ROTable *t,
                                const ROTable_entry *e, ROTable *mt) {
  int i, j;
  lu_byte flags = ~0;
  const char *plast = (char *)"_";

  lua_lock(L);
  for (i = 0; e[i].key; i++) {
    if (e[i].key[0] == '_' && strcmp(e[i].key,plast)) {
      plast = e[i].key;
      lua_pushstring(L,e[i].key);
      for (j=0; j<TM_EQ; j++){
        if(tsvalue(L->top-1)==G(L)->tmname[i]) {
          flags |= cast_byte(1u<<i);
          break;
        }
      }
      lua_pop(L,1);
    }
  }
  t->next      = (GCObject *)1;
  t->tt        = LUA_TTBLROF;
  t->marked    = LROT_MARKED;
  t->flags     = flags;
  t->lsizenode = i;
  t->metatable = cast(Table *, mt);
  t->entry     = cast(ROTable_entry *, e);
  lua_unlock(L);
}

LUA_API void lua_getlfsconfig (lua_State *L, int *config) {
  global_State *g = G(L);
  LFSHeader *l = g->l_LFS;
  if (!config)
    return;
  config[0] = (int) (size_t) l;                  /* LFS region mapped address */
  config[1] = platform_flash_mapped2phys(config[0]);    /* ditto phys address */
  config[2] = g->LFSsize;                           /* LFS region actual size */
  if (g->ROstrt.hash) {
    config[3] = l->flash_size;                             /* LFS region used */
    config[4] = l->timestamp;                         /* LFS region timestamp */
  } else {
    config[3] = config[4] = 0;
  }
}

LUA_API int  lua_pushlfsindex (lua_State *L) {
  lua_lock(L);
  setobj2n(L, L->top, &G(L)->LFStable);
  api_incr_top(L);
  lua_unlock(L);
  return  ttnov(L->top-1);
}

/*
 * In Lua 5.3 luac.cross generates a top level Proto for each source file with
 * one upvalue that must be the set to the _ENV variable when its closure is
 * created, and as such this parallels some ldo.c processing.
 */
LUA_API int  lua_pushlfsfunc (lua_State *L) {
  lua_lock(L);
  const TValue *t = &G(L)->LFStable;
  if (ttisstring(L->top-1) && ttistable(t)) {
    luaV_gettable(L, t, L->top-1, L->top-1);
    if (ttislightuserdata(L->top-1)) {
      Proto *f = pvalue(L->top-1);  /* The pvalue is Proto * for Lua function */
      LClosure *cl = luaF_newLclosure(L, f->sizeupvalues);
      setclLvalue(L, L->top-1, cl);
      luaF_initupvals(L, cl);
      cl->p = f;
      if (cl->nupvalues >= 1) {                   /* does it have an upvalue? */
        UpVal *uv1 = cl->upvals[0];
        TValue *val = uv1->v;
        /* set 1st upvalue as global env table from registry */
        setobj(L, val, luaH_getint(hvalue(&G(L)->l_registry), LUA_RIDX_GLOBALS));
        luaC_upvalbarrier(L, uv1);
      }
      lua_unlock(L);
      return 1;
    }
  }
  setnilvalue(L->top-1);
  lua_unlock(L);
  return 0;
}

/*
** The undump processing can throw errors on out-of-memory and file corruption
** problems.  However, the historic convention is that any recoverable error
** during flash reload is caught and returns an error message on failure. If
** the validation pass is successful, then control is not returned but instead
** the CPU is restarted and the second pass of the flash load occurs during
** Lua startup.
**
** Hence the main load functionality is executed in a protected call.  See
** LFS section below
*/
  static void protectedLFSload (lua_State *L, void *ud);
  static void P1epilogue (LFSflashState *F);

LUA_API void lua_lfsreload (lua_State *L) {
  LFSflashState F = {0};
  LFSHeader hdr;
  memset(&hdr, ~0, sizeof(hdr));
  F.L    = L;
  F.size = G(L)->LFSsize;
  F.hdr  = &hdr;
  lua_lock(L);
  int status = luaD_pcall(L, protectedLFSload, &F, savestack(L, L->top-2), 0);
  if (status == LUA_OK) {
    P1epilogue( &F);
    luaM_freearray(L, F.saveStr, F.nSave);
    system_restart();
    luaO_pushfstring(L, "!LFS reload%s",
                        (status == LUA_OK) ? "! start phase 2": " aborted" );
    luaD_throw(L, LUA_ERRRUN);
  } else { /* Cleanup resources and return error string as ToS */
    if (F.f)
      l_close(F.f);
    luaM_freearray(L, F.saveStr, F.nSave);
    luaU_closeLoadState(F.S, NULL, NULL);   /* TODO:  sysLFS chaining */
  }
  lua_unlock(L);
}


//================ NodeMCU lauxlib.h LUALIB_API extensions ===================//

/*
** These are auxiliary library function extending those implemented in lauxlib.c
** and publicly declared in lauxlib.h.  They are written using the same LUA_LIB
** implementation rules as lauxlib.c and only access Lua core services through
** the lua.h C API.  Whilst the core implementations are different for Lua 5.1
** and Lua 5.3, the lauxlib.h is (with specific documented exceptions) is common
** and this allow any application modules to be compiled against both versions.
*/

/*
** Return an array of functions in LFS  TODO:
*/
// TODO: EXTEND to check for __index meta and add sysLFS entries <<<<<<<<
LUALIB_API int luaL_pushlfsmodules (lua_State *L) {
  int i = 1;
  if (lua_pushlfsindex(L) == LUA_TNIL)
    return 0;                                 /* return nil if LFS not loaded */
  lua_newtable(L);      /* create dest table and move above LFS index ROTable */
  lua_insert(L, -2);
  lua_pushnil(L);
  while (lua_next(L, -2) != 0) {
    lua_pop(L, 1);                       /* dump the value (ptr to the Proto) */
    lua_pushvalue(L, -1);                            /* dup key (module name) */
    lua_rawseti(L, -4, i++);
  }
  lua_pop(L, 1);                                /* dump the LFS index ROTable */
  return 1;
}


LUALIB_API int  luaL_pushlfsdts (lua_State *L) {
  int config[5];
  lua_getlfsconfig(L, config);
  lua_pushinteger(L, config[4]);
  return 1;
}

LUALIB_API void luaL_lfsreload (lua_State *L) {
 /*
  * Do some type checking on files array then call lua_lfsreload() to drop
  * into LUA_CORE and do the pass 1 load.
  */
  int gcrunning = lua_gc(L, LUA_GCISRUNNING, 0);
  lua_settop(L, 1);
  if (lua_isstring(L, 1)) { /* convert string entry to single entry array */
    lua_createtable(L, 1, 0);
    lua_insert(L, 1);
    lua_rawseti(L, 1, 1);
  }
  luaL_argcheck(L, lua_type(L, 1) == LUA_TTABLE, 1, "Invalid file (list)");
  int n = luaL_len(L, 1);
  for (int i = 0; i < n; i++) {
    luaL_argcheck(L, lua_rawgeti(L, 1, i+1) == LUA_TSTRING, 1, "Invalid file (list)");
    lua_pop(L, 1);
  }
  lua_newtable(L);
  lua_gc(L, LUA_GCSTOP, 0);
  lua_lfsreload(L);     /* does not return if reload successful */
  if (gcrunning)
    lua_gc(L, LUA_GCRESTART, 0);
}


//============= ESP and HOST lua_debugbreak() test stubs =====================//

#ifdef DEVELOPMENT_USE_GDB
/*
 * lua_debugbreak is a stub used by lua_assert() if DEVELOPMENT_USE_GDB is
 * defined.  On the ESP, instead of crashing out with an assert error, this hook
 * starts the GDB remote stub if not already running and then issues a break.
 * The rationale here is that when testing the developer might be using screen /
 * PuTTY to work interactively with the Lua Interpreter via UART0.  However if
 * an assert triggers, then there is the option to exit the interactive session
 * and start the Xtensa remote GDB which will then sync up with the remote GDB
 * client to allow forensics of the error.  On the host it is an stub which can
 * be set as a breakpoint in the gdb debugger.
 */
extern void gdbstub_init(void);
extern void gdbstub_redirect_output(int);

LUALIB_API void lua_debugbreak(void) {
#ifdef LUA_USE_HOST
  /* allows debug backtrace analysis of assert fails */
  lua_writestring(" lua_debugbreak ", sizeof(" lua_debugbreak ")-1);
#else
  static int repeat_entry = 0;
  if  (repeat_entry == 0) {
    dbg_printf("Start up the gdb stub if not already started\n");
    gdbstub_init();
    gdbstub_redirect_output(1);
    repeat_entry = 1;
  }
  asm("break 0,0" ::);
#endif
}
#endif


//======== NodeMCU bootstrap to set up and to reimage LFS resources ==========//
/*
** This processing uses 2 init hooks during the Lua startup. The first is
** called early in the Lua state setup to initialize the LFS if present. The
** second is only used to rebuild the LFS region; this requires the Lua
** environment to be in place, so this second hook is immediately before
** processing LUA_INIT.
**
** An application library initiates an LFS rebuild by writing a FLASHLFS
** message to the Reboot Config Record area (RCR), and then restarting the
** processor.  This RCR record is read during startup by the 2nd hook. The
** content is the name of the Lua LFS image file to be loaded. If present then
** the LFS reload process is initiated instead of LUA_INIT.  This uses lundump
** functions to load the components directly into the LFS region.
**
** FlashState used to share context with the low level lua_load write routines
** is passed as a ZIO data field.  Note this is only within the phase
** processing and not across phases.
*/


#define FLASH_FORMAT_VERSION ( 3 << 8)
#define FLASH_FORMAT_MASK    0xF00
#define FLASH_SIG_PASS_MASK   0x0F
/* PASS_1 Start through to Pass 2 Done can be updated using NAND overwrite */
#define FLASH_SIG_PASS_1START 0x07
#define FLASH_SIG_PASS_1DONE  0x03
#define FLASH_SIG_PASS_2START 0x01
#define FLASH_SIG_PASS_2DONE  0x00
#define FLASH_SIG_B2_MASK     0x04
#define FLASH_SIG_IN_PROGRESS 0x08
#define FLASH_SIG  (0xfafaa050 | FLASH_FORMAT_VERSION)


#define WORDSIZE sizeof(lu_int32)

#define OSIZE (FLASH_PAGE_SIZE/WORDSIZE)
#ifdef LUA_USE_ESP
#define ALIGN(F,n) (n + WORDSIZE - 1) / WORDSIZE;
#else
#define ALIGN(F,n) ((n + F->allocmask) & ~(F->allocmask)) / WORDSIZE;
#endif

#define MODE_RAM      0   /* Loading into RAM */
#define MODE_LFS      1   /* Loading into a locally executable LFS */
#define MODE_LFSA     2   /* (Host only) Loading into a shadow ESP image */

/*
** ZIO lua_Reader spec, hence the L parameter
*/
static const char *ZIOreadF (lua_State *L, void *ud, size_t *size) {
  UNUSED(L);
  LFSflashState *F = cast(LFSflashState *, ud);
  if (F->inNdx > 0) {
    *size = F->inNdx;
    F->inNdx = 0;
  } else {
    if (l_feof(F->f)) return NULL;
    *size = l_read(F->f, F->inBuff) ;  /* read block */
  }
  return cast(const char *,F->inBuff);
}

/*
** LFS routines for output to Flash ROM
*/
static void eraseLFS(LFSflashState *F) {
  lu_int32 i;
  printf("\nErasing LFS from flash addr 0x%06x", F->addrPhys);
  for (i = 0; i < F->size; i += FLASH_PAGE_SIZE) {
    size_t *f = cast(size_t *, F->addr + i/sizeof(*f));
    lu_int32 s = platform_flash_get_sector_of_address(F->addrPhys + i);
    /* it is far faster not erasing if you don't need to */
    if (*f != ~0 || memcmp(f, f + 1, FLASH_PAGE_SIZE - sizeof(*f))) {
      platform_flash_erase_sector(s); printf(".");
    }
  }
  printf(" to 0x%06x\n", F->addrPhys + F->size-1);
  flush_icache(F);
}

static void flushtoLFS(void *vF) {
  LFSflashState *F = cast(LFSflashState *, vF);
  lu_int32 start   = F->addrPhys + F->oChunkNdx*WORDSIZE;
  lu_int32 size    = F->oNdx * WORDSIZE;
  lua_assert(start + size < F->addrPhys + F->size); /* is write in bounds? */
  platform_s_flash_write(F->oBuff, start, size);
  F->oChunkNdx += F->oNdx;
  F->oNdx = 0;
}

static void setLFSaddr(void *F, unsigned int o) {
  flushtoLFS(F);  /* flush the pending write buffer */
  lua_assert((o & (WORDSIZE-1))==0);
  cast(LFSflashState *,F)->oChunkNdx = o/WORDSIZE;
}

static void *writeLFSrec(void *vF, const void *rec, size_t n) {
  LFSflashState *F = cast(LFSflashState *, vF);
  lu_byte *p   = byteptr(F->addr + F->oChunkNdx + F->oNdx);
  if (rec == NULL) {  /* dummy call to flush or to get current flash position */
    if (n == ~0) {
      /* Flush Flash buffer and icache so LFS ROM reads are coherent */
      flushtoLFS(F);
      flush_icache(F);
    }
    return p;
  }
#if 0  /* Host only debugging */
/*DEBUG*/  /* catch any non-LFSregion address references */
/*DEBUG*/ for (int i = 0; i < n/sizeof(void *); i++) {
/*DEBUG*/   size_t w = cast(size_t *, rec)[i]; size_t w24 = w>>24;
/*DEBUG*/   if (w24 == 0x555555 || w24 == 0x7fffff)
/*DEBUG*/     lua_assert((size_t)(byteptr(LFSregion) - byteptr(w)) < 262144);
/*DEBUG*/ }
#endif
   while (1) {
    int nw  = ALIGN(F, n);
    if (F->oNdx + nw > OSIZE) {
      /* record overflows the buffer so fill buffer, flush and repeat */
      int rem = OSIZE - F->oNdx;
      if (rem)
        memcpy(F->oBuff+F->oNdx, rec, rem * WORDSIZE);
      rec     = wordptr(rec) + rem;
      n      -= rem * WORDSIZE;
      F->oNdx = OSIZE;
      flushtoLFS(F);
    } else {
      /* append remaining record to buffer */
      F->oBuff[F->oNdx+nw-1] = 0;       /* ensure any trailing odd byte are 0 */
      memcpy(F->oBuff+F->oNdx, rec, n);
      F->oNdx += nw;
      break;
    }
  }
  return p;
}

/*
** Routines for constructing an LFS using the luaU undump functions to generate
** the various Proto, TString records etc. along with LHS header.  Note that
** the LFS is created in two passes
**
** -  Pass 1 is a dummy run to validate input file formats and computes the
**    size of string table index. The actual writes are dummied and so leave
**    the LFS region unchanged on error. This pass be executed within a normal
**    Lua VM context, and is protected so errors are thrown and returned by the
**    caller. If load scan completes successfully then a P1epilogue erases the
**    LFS and writes out a set of TStrings for use by pass 2; the caller then
**    restarts the CPU.
**
** -  Pass 2 reads these strings and uses this list to reload the LC files,
**    this time writing the Protos, etc. to LFS.  The CPU is restarted a second
**    time to load the new LFS.
*/

extern void _ResetHandler(void);

static void P1epilogue (LFSflashState *F) {
  UTString dummy;    /* UTString (not TString) for Test Suite compatibility */
  memset(&dummy, ~0, sizeof(dummy));

  eraseLFS(F);
  setLFSaddr(F, 0);
  F->hdr->flash_sig = FLASH_SIG | FLASH_SIG_PASS_1START; /* minimal Pass 1 header */
  writeLFSrec(F, F->hdr, sizeof(*F->hdr));
 /*
  * Now dump out the list of files and the list of Proto names as a dummy
  * TStrings header + the CString constant of the field.  The actual headers
  * will be overwritten in pass 2 which works fine because of NAND writing
  * rules.
  */
  for (int i = 0; i < F->nSave; i++) {
    writeLFSrec(F, &dummy, sizeof(dummy));
    writeLFSrec(F, F->saveStr[i], strlen(F->saveStr[i])+1);
  }
  writeLFSrec(F, NULL, ~0);  /* Flush to LFS and flush cache */

  F->hdr->flash_sig = FLASH_SIG | FLASH_SIG_PASS_1DONE;
  setLFSaddr(F, 0);
  writeLFSrec(F, &F->hdr->flash_sig, sizeof(F->hdr->flash_sig));
  writeLFSrec(F, NULL, ~0);  /* Flush to LFS and flush cache */
  platform_rcr_write(PLATFORM_RCR_FLASHLFS, NULL, 0); /* do phase 2 on reboot */
#ifdef LUA_USE_ESP
 /*
  * Things can get very problematic if the LFS has disappeared.  For example we
  * can't guarantee that the caller hasn't invoked the reload as a protected 
  * call from an LFS function than now no longer exists.  It's just unsafe to
  * unroll the stack, so the easiest approach is to invoke the reset handler.
  */
  ets_delay_us(5000);
  _ResetHandler();      /* Trigger reset; Phase 2 will be entered on reboot */
#endif
  G(F->L)->ROstrt.hash = NULL;    /* Disable any resolutions against ROstrt */
}

static Table *P2prologue (LFSflashState *F) {
  int i,j,k,n;
  LFSHeader *hdr = cast(LFSHeader *,F->addr);  /* The Pass 1 header in LFS */
  lu_int32 *w = wordptr(hdr + 1);
 /*
  * Independent of the TStrings defined in the Protos themselves, the ROstrt
  * always contains some standard TStrings to avoid their being creating in RAM:
  * -  the names of the LC files to be loaded;
  * -  the names of the Protos in the LFS;
  * -  the fixed strings defined in ltm.c and llex.c.
  *
  * The first two sets have already been to LFS at the end of pass 1 but with
  * dummy TStrings, so overwriting then works fine under NAND rules.
  *
  * The fixed strings are exposed through the luaX_getstr() and luaT_getstr()
  * functions.
  */
  for (j = 0; luaX_getstr(j, NULL); j++) {}       /* count LEX strings */
  for (k = 0; luaT_getstr(k); k++) {}   /* count TM strings */

  n = max(max(j,k), hdr->nFiles+hdr->nPVs) + 1;
  const char **p = cast(const char **, malloc(n * sizeof(*p) + j));
  lu_byte *e = cast(lu_byte *, p + n);
  lua_assert(p);

  for (i = 0; i < hdr->nFiles+hdr->nPVs; i++) { /* add LFS P1 strings */
    w += sizeof(UTString)/sizeof(lu_int32);
    p[i] = cast(const char *, w);
    while (*++w != ~0) {}
  }
  p[i] = NULL;
  setLFSaddr(F, sizeof(*hdr));  /* skip over the LFS header */
  luaU_addstrings(F->S, p, NULL);

  /* Add now addressable files TStrings to Files table */
  Table *t = luaH_new(F->L);
  luaH_resize(F->L, t, hdr->nFiles, 0);
  for (i = 0, w = wordptr(hdr + 1); i < hdr->nFiles; i++) {
    TValue v;
    TString *ts = cast(TString *, cast(UTString *, p[i]) - 1);
    setsvalue(F->L, &v, ts);
    luaH_setint(F->L, t, i + 1, &v);
  }
  for (i = 0; i<j; i++)
    p[i] = luaX_getstr(i, e+i);
  p[j] = NULL;
  luaU_addstrings(F->S, p, e);

  for (i = 0; i<k; i++)
    p[i] = luaT_getstr(i);
  p[k] = NULL;
  luaU_addstrings(F->S, p, NULL);

  free(p);
  return t;
}

#define FOFFSET(p) wordoffset(p,F->addr)

static void protectedLFSload (lua_State *L, void *ud) {
  LFSflashState *F = cast(LFSflashState *, ud);

  int pass = (F->pass == 2) ? 2 : 1;
  unsigned int seed = 1234512345;  // << TODO: this needs Randomizing
  Table *files, *pvs;
  int    nFiles, nPVs;
  ZIO z;

#ifdef LUA_USE_HOST
  int mode = MODE_LFS;             // << TODO: this needs acquiring for LFSA
  /* Writes to LFS are size_t or lu_int32 aligned depending on mode */
  F->allocmask = (mode == MODE_LFS) ? sizeof(size_t) - 1 : sizeof(lu_int32) - 1;
#endif
  if (pass == 1) {
    pvs   = hvalue(L->top-1);
    F->S  = luaU_LoadState(L, pvs, 0,  NULL, NULL, MODE_LFS, seed);
    files = hvalue(L->top-2);
  } else { /* pass == 2 */
    l_mountfs();
    pvs   = luaH_new(L);
    F->S  = luaU_LoadState(L, pvs, F->ROstrt.size, &writeLFSrec, F, MODE_LFS, seed);
    files = P2prologue(F);
  }

  nFiles = luaH_getn(files);
  for (int i = 1; i <= nFiles; i++) {
    const char *fname = svalue(luaH_getint(files, i));
    char b1[1];

#ifdef DEVELOPMENT_USE_GDB  /* For GDB builds, check for dummy break filename */
    if (fname[0] == '!') {      /* "!1" and "!2" break on passes 1 and 2 resp */
      if (fname[1] & pass) lua_debugbreak();
    continue;
    }
#endif
    F->f = fname ? l_open(fname) : 0;
    if (!F->f || l_read(F->f, b1) != 1 || *b1 != LUA_SIGNATURE[0]) {
      L->top -= 2;
      luaO_pushfstring(F->L, "Invalid or missing file %s", fname);
      luaD_throw(L, LUA_ERRRUN);
    }
    luaZ_init(L, &z, ZIOreadF, F);
    luaU_undumpx(L, &z, fname, F->S);  /* This can throw an error */
    l_close(F->f); l_zero(F->f);
  }

  if (pass == 1) {
    luaU_data res;
    int i;
    TValue tv[2];
    memset(&res, 0, sizeof(res));
    setnilvalue(tv);                     /* loop around the pvs to count them */
    for (nPVs = 0; luaH_next(F->L, pvs, tv); nPVs++) {}
    F->saveStr = luaM_newvector(L, nFiles + nPVs, const char *);
    F->nSave = nFiles + nPVs;
    luaU_closeLoadState(F->S, &res, NULL);
    F->hdr->seed = seed;
    F->hdr->nROsize = 1 << luaO_ceillog2(res.tsTot+30);
    F->hdr->nFiles = nFiles;
    F->hdr->nPVs = nPVs;
    lua_unlock(L);
    lua_gc(L, LUA_GCRESTART, 0);
    lua_gc(L, LUA_GCCOLLECT, 0);
    lua_lock(L);
    F->oBuff = calloc(OSIZE, sizeof(lu_int32));
    F->addr  = wordptr(G(L)->l_LFS);
    F->addrPhys = platform_flash_mapped2phys(cast(size_t, F->addr));
   /*
    * Some of the file and PV names might be in LFS so these need to be copied
    * into RAM before the LFS can be erased and rewritten. The CPU is restarted
    * on completion of this stage so memory leakage isn't really an issue.
    */
    for (i = 0; i < nFiles; i++)
      F->saveStr[i] = svalue(luaH_getint(files, i+1));
    setnilvalue(tv);
    while(luaH_next(F->L, pvs, tv))
      F->saveStr[i++] = svalue(tv);

    for (i = 0; i < F->nSave; i++) {
      if ((FOFFSET(F->saveStr[i])) < (F->size/sizeof(lu_int32))) {
        /* Dup the string into RAM if it is LFS */
        if ((F->saveStr[i] = strdup(F->saveStr[i])) == NULL)
          luaD_throw(L, LUA_ERRMEM);
      }
    }
  }
}


/*
** Hook used in Lua Startup to carry out the optional LFS startup processes.
*/
LUAI_FUNC int luaN_init (lua_State *L) {
  static LFSflashState *F = NULL;
  static LFSHeader *fh;
 /*
  * The first entry is called from lstate.c:f_luaopen() before modules
  * are initialised.  This is detected because F is NULL on first entry.
  */
  if (F == NULL) {
    F = calloc(1, sizeof(LFSflashState));
    F->L = L;
     F->size = platform_flash_get_partition(NODEMCU_LFS0_PARTITION, &F->addrPhys);
    if (F->size) {
      global_State *g = G(L);
      g->LFSsize      = F->size;
      F->addr         = wordptr(platform_flash_phys2mapped(F->addrPhys));
      fh              = cast(LFSHeader *, F->addr);
     /* Pass 2 requires RCR flag and Pass 1 completed, else normal startup */
      if ((fh->flash_sig != (FLASH_SIG | FLASH_SIG_PASS_1DONE)) ||
          (platform_rcr_read(PLATFORM_RCR_FLASHLFS, NULL) == ~0)) {
      /* Set up LFS hooks on normal Entry */
        g->l_LFS       = fh;
        if (fh->flash_sig == FLASH_SIG) {
          g->seed        = fh->seed;
          g->ROstrt.hash = cast(TString **, F->addr + fh->oROhash);
          g->ROstrt.nuse = fh->nROuse ;
          g->ROstrt.size = fh->nROsize;
          sethvalue(L, &g->LFStable, cast(Table *, F->addr + fh->protoROTable));
          lua_writestringerror("LFS image %s\n", "loaded");
        } else if (fh->flash_sig != ~0) {
lua_debugbreak();
          lua_writestringerror("LFS image %s\n", "corrupted.");
          eraseLFS(F);
        }
      } else {
        F->pass = 2;           /* flash LFS command read and pass 1 completed */
     }
    }
    return 0;
  } else {
    int status = 0;
   /*
    * Hook 2 called from protected pmain, so can throw errors. (Actually from
    * luaL_openlibs() within pmain after opening the base, package and string
    * libraries, that is before loading any application libraries.  LFS pass 2
    * always throws an error forcing a restart.  Note that the RCR record is
    * deleted on entering this path, so the load is only attempted once/
    */
    if (F->pass == 2) {                           /* hook == 2 LFS image load */
      LFSHeader fh2;
      luaU_data res;
      platform_rcr_delete(PLATFORM_RCR_FLASHLFS);
//*DEBUG*/ lua_debugbreak();
      F->oBuff = calloc(OSIZE, WORDSIZE);
      F->ROstrt.size = fh->nROsize;
      lua_lock(L);
      fh2.flash_sig =  FLASH_SIG | FLASH_SIG_PASS_2START; /* minimal Pass 1 header */
      writeLFSrec(F, &fh2.flash_sig, sizeof(fh2.flash_sig));

      int status = luaD_pcall(L, protectedLFSload, F, savestack(L, L->top-2), 0);
      luaU_closeLoadState(F->S, &res, NULL);
      if (status == LUA_OK) {
        /* Create final header and write to LFS */
        fh2.flash_sig    = FLASH_SIG;
        fh2.seed         = fh->seed;
        fh2.timestamp    = 0;
        fh2.nROuse       = res.ROstrt.nuse;
        fh2.nROsize      = res.ROstrt.size;
        fh2.oROhash      = FOFFSET(res.ROstrt.hash);
        fh2.protoROTable = FOFFSET(res.protoROTable);
        fh2.protoHead    = FOFFSET(res.protoHead);
        fh2.shortTShead  = FOFFSET(res.shortTShead);
        fh2.longTShead   = FOFFSET(res.longTShead);
        fh2.nFiles       = fh->nFiles;
        fh2.nPVs         = fh->nPVs;
        fh2.flash_size   = byteoffset(writeLFSrec(F, NULL, 0), F->addr);
        setLFSaddr(F, 0);
        writeLFSrec(F, &fh2, sizeof(fh2));
        flushtoLFS(F);
      }
      if (F->f) l_close(F->f);
      free(F->oBuff); F->oBuff = NULL;
      free(F); F = NULL;
      system_restart();
      luaO_pushfstring(L, (status == LUA_OK) ? "!LFS restart" :
                                               "!LFS phase 2 aborted");
      luaD_throw(L, LUA_ERRRUN);
    } else {                                     /* hook == 2, Normal startup */
      free(F); F = NULL;
    }
    return status;
  }
}

// =============================================================================

#ifdef LUA_USE_ESP
extern void lua_main(void);
/*
** Task callback handler. Uses luaN_call to do a protected call with full traceback
*/
static void do_task (platform_task_param_t task_fn_ref, uint8_t prio) {
  lua_State* L = lua_getstate();
  if(task_fn_ref == (platform_task_param_t)~0 && prio == LUA_TASK_HIGH) {
    lua_main();                   /* Undocumented hook for lua_main() restart */
    return;
  }
  if (prio < LUA_TASK_LOW|| prio > LUA_TASK_HIGH)
    luaL_error(L, "invalid posk task");
/* Pop the CB func from the Reg */
  lua_rawgeti(L, LUA_REGISTRYINDEX, (int) task_fn_ref);
  luaL_checktype(L, -1, LUA_TFUNCTION);
  luaL_unref(L, LUA_REGISTRYINDEX, (int) task_fn_ref);
  lua_pushinteger(L, prio);
  luaL_pcallx(L, 1, 0);
}

/*
** Schedule a Lua function for task execution
*/
LUALIB_API int luaL_posttask ( lua_State* L, int prio ) {         // [-1, +0, -]
  static platform_task_handle_t task_handle = 0;
  if (!task_handle)
    task_handle = platform_task_get_id(do_task);
  if (L == NULL && prio == LUA_TASK_HIGH+1) { /* Undocumented hook for lua_main */
    platform_post(LUA_TASK_HIGH, task_handle, (platform_task_param_t)~0);
    return -1;
  }
  if (lua_isfunction(L, -1) && prio >= LUA_TASK_LOW && prio <= LUA_TASK_HIGH) {
    int task_fn_ref = luaL_ref(L, LUA_REGISTRYINDEX);
    if(!platform_post(prio, task_handle, (platform_task_param_t)task_fn_ref)) {
      luaL_unref(L, LUA_REGISTRYINDEX, task_fn_ref);
      luaL_error(L, "Task queue overflow. Task not posted");
    }
    return task_fn_ref;
  } else {
    return luaL_error(L, "invalid posk task");
  }
}
#else
/*
** Task execution isn't supported on HOST builds so returns a -1 status
*/
LUALIB_API int luaL_posttask( lua_State* L, int prio ) {            // [-1, +0, -]
  return -1;
}
#endif
