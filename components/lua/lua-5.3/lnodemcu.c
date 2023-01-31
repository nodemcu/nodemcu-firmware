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
#include "lstring.h"
#include "ltable.h"
#include "ltm.h"
#include "lnodemcu.h"
#include "lundump.h"
#include "lzio.h"

#include "lfs.h"

#ifdef LUA_USE_ESP
#include "platform.h"
#include "task/task.h"
#else
// On the cross-compiler we do need the LFS reload mechanism, regardless
#undef CONFIG_NODEMCU_EMBEDDED_LFS_SIZE
#endif

#ifdef LUA_USE_ESP8266
#include "vfs.h"
#endif

/*
** This is a mixed bag of NodeMCU additions broken into the following sections:
**    *  POSIX vs VFS file API abstraction
**    *  Emulate Platform_XXX() API
**    *  ESP and HOST lua_debugbreak() test stubs
**    *  NodeMCU lua.h LUA_API extensions
**    *  NodeMCU lauxlib.h LUALIB_API extensions
**    *  NodeMCU bootstrap to set up and to reimage LFS resources
**
** Just search down for //== or ==// to flip through the sections.
*/

#define byte_addr(p)    cast(char *,p)
#define byteptr(p)      cast(lu_byte *, p)
#define byteoffset(p,q) ((int) cast(ptrdiff_t, (byteptr(p) - byteptr(q))))
#define wordptr(p)      cast(lu_int32 *, p)
#define wordoffset(p,q) (wordptr(p) - wordptr(q))


//====================== Wrap POSIX and VFS file API =========================//

#ifdef LUA_USE_ESP8266
int luaopen_file(lua_State *L);
#  define l_file(f)   int f
#  define l_open(f)   vfs_open(f, "r")
#  define l_close(f)  vfs_close(f)
#  define l_feof(f)   vfs_eof(f)
#  define l_read(f,b) vfs_read(f, b, sizeof (b))
#  define l_rewind(f) vfs_lseek(f, 0, VFS_SEEK_SET)
#else
#  define l_file(f)   FILE *f
#  define l_open(n)   fopen(n,"rb")
#  define l_close(f)  fclose(f)
#  define l_feof(f)   feof(f)
#  define l_read(f,b) fread(b, 1, sizeof (b), f)
#  define l_rewind(f) rewind(f)
#endif

#ifdef LUA_USE_ESP8266
extern void dbg_printf(const char *fmt, ...);                // DEBUG
#undef printf
#define printf(...) dbg_printf(__VA_ARGS__)                  // DEBUG
#endif

#ifdef LUA_USE_ESP

#define FLASH_PAGE_SIZE INTERNAL_FLASH_SECTOR_SIZE
/* Erasing the LFS invalidates ESP instruction cache,  so doing a block 64Kb */
/* read is the simplest way to flush the icache, restoring cache coherency */
#define flush_icache(F) \
  UNUSED(memcmp(F->addr, F->addr+(0x8000/sizeof(*F->addr)), 0x8000));
#define unlockFlashWrite()
#define lockFlashWrite()

#else // LUA_USE_HOST

//==== Emulate Platform_XXX() API within host luac.cross -e environement =====//

#include<stdio.h>                                             // DEBUG
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
#define PLATFORM_RCR_FLASHLFS  4
#define LFS_SIZE         0x40000
#define FLASH_PAGE_SIZE   0x1000
#define FLASH_BASE       0x90000           /* Some 'Random' but typical value */

void         *LFSregion = NULL;
static void  *LFSaddr   = NULL;
static size_t LFSbase   = FLASH_BASE;
extern char  *LFSimageName;

#ifdef __unix__
/* On POSIX systems we can toggle the "Flash" write attribute */
#include <sys/mman.h>
#define aligned_malloc(a,n) posix_memalign(&a, FLASH_PAGE_SIZE, (n))
#define unlockFlashWrite() mprotect(LFSaddr, LFS_SIZE, PROT_READ| PROT_WRITE)
#define lockFlashWrite() mprotect(LFSaddr, LFS_SIZE, PROT_READ)
#else
#define aligned_malloc(a,n) ((a = malloc(n)) == NULL)
#define unlockFlashWrite()
#define lockFlashWrite()
#endif

#define platform_flash_get_sector_of_address(n) ((n)>>12)

void luaN_setabsolute(lu_int32 addr) {
  LFSaddr = cast(void *, cast(intptr_t, addr));
  LFSbase = addr;
}

bool lfs_get_location(lfs_location_info_t *out)
{
  if (!LFSregion) {
    if(aligned_malloc(LFSregion, LFS_SIZE))
      return false;
    memset(LFSregion, ~0, LFS_SIZE);
    lockFlashWrite();
  }
  if(LFSaddr == NULL)
    LFSaddr = LFSregion;

  out->size = LFS_SIZE;
  out->addr_mem = LFSaddr;
  out->addr_phys = LFSbase;
  return true;
}

bool lfs_get_load_filename(char *buf, size_t bufsiz)
{
  if (LFSimageName)
    strncpy(buf, LFSimageName, bufsiz);
  else if (buf && bufsiz)
    *buf = 0;
  return true;
}

bool lfs_clear_load_filename(void)
{
  LFSimageName = NULL;
  return true;
}

static void platform_flash_erase_sector(lu_int32 i) {
  lua_assert (i >= LFSbase/FLASH_PAGE_SIZE &&
              i < (LFSbase+LFS_SIZE)/FLASH_PAGE_SIZE);
  unlockFlashWrite();
  memset(byteptr(LFSregion) + (i*FLASH_PAGE_SIZE - LFSbase), ~(0), FLASH_PAGE_SIZE);
  lockFlashWrite();
}

static void platform_flash_write(const void *from, lu_int32 to, lu_int32 len) {
  lua_assert(to >= LFSbase && to + len < LFSbase + LFS_SIZE);  /* DEBUG */
  unlockFlashWrite();
  memcpy(byteptr(LFSregion) + (to-LFSbase), from, len);
  lockFlashWrite();
}

#define flush_icache(F)   /* not needed */

#endif

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

//===================== NodeMCU lua.h API extensions =========================//

LUA_API int lua_freeheap (void) {
#ifdef LUA_USE_HOST
  return MAX_INT;
#else
  return (int)esp_get_free_heap_size();
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
}

LUA_API void lua_getlfsconfig (lua_State *L, intptr_t *config) {
  global_State *g = G(L);
  LFSHeader *l = g->l_LFS;  
  if (!config)
    return;
  lfs_location_info_t lfs_loc;
  if (lfs_get_location(&lfs_loc) && lfs_loc.addr_mem == l)
  {
    config[0] = (intptr_t)lfs_loc.addr_mem;
    config[1] = (intptr_t)lfs_loc.addr_phys;
    config[2] = (intptr_t)lfs_loc.size;
  }
  if (g->ROstrt.hash) {
    config[3] = l->flash_size;                             /* LFS region used */
    config[4] = l->timestamp;                         /* LFS region timestamp */
  } else { 
    config[3] = config[4] = 0;
  }
}

LUA_API int lua_pushlfsindex(lua_State *L) {
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
LUA_API int lua_pushlfsfunc(lua_State *L) {
  lua_lock(L);
  const TValue *t = &G(L)->LFStable;
  if (ttisstring(L->top-1) && ttistable(t)) {
    const TValue *v = luaH_getstr (hvalue(t), tsvalue(L->top-1));
    if (ttislightuserdata(v)) {
      Proto *f = pvalue(v);   /* The pvalue is a Proto * for the Lua function */
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
      return 1;
    }
  }
  setnilvalue(L->top-1);
  lua_unlock(L);
  return 0;
}


//================ NodeMCU lauxlib.h LUALIB_API extensions ===================//

/*
 * Return an array of functions in LFS
 */
LUALIB_API int luaL_pushlfsmodules(lua_State *L) {
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

LUALIB_API int luaL_pushlfsdts(lua_State *L) {
  intptr_t config[5];
  lua_getlfsconfig(L, config);
  lua_pushinteger(L, config[4]);
  return 1;
}

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


typedef struct LFSflashState {
  lua_State  *L;
  LFSHeader   hdr;
  l_file(f);
  lu_int32   *addr;
  lu_int32    oNdx;               /* in size_t units */
  lu_int32    oChunkNdx;          /* in size_t units */
  lu_int32   *oBuff;              /* FLASH_PAGE_SIZE bytes */
  lu_byte    *inBuff;             /* FLASH_PAGE_SIZE bytes */
  lu_int32    inNdx;              /* in bytes */
  lu_int32    addrPhys;
  lu_int32    size;
  lu_int32    allocmask;
  stringtable ROstrt;
  GCObject   *pLTShead;
} LFSflashState;
#define WORDSIZE sizeof(lu_int32)
#define OSIZE (FLASH_PAGE_SIZE/WORDSIZE)
#define ISIZE (FLASH_PAGE_SIZE)
#ifdef LUA_USE_ESP
#define ALIGN(F,n) (n + WORDSIZE - 1) / WORDSIZE;
#else
#define ALIGN(F,n) ((n + F->allocmask) & ~(F->allocmask)) / WORDSIZE;
#endif

#ifndef CONFIG_NODEMCU_EMBEDDED_LFS_SIZE
/* This conforms to the ZIO lua_Reader spec, hence the L parameter */
static const char *readF (lua_State *L, void *ud, size_t *size) {
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

static void eraseLFS(LFSflashState *F) {
  lu_int32 i;
#ifdef LUA_USE_ESP
  printf("\nErasing LFS from flash addr 0x%06lx", F->addrPhys);
#endif
  unlockFlashWrite();
  for (i = 0; i < F->size; i += FLASH_PAGE_SIZE) {
    size_t *f = cast(size_t *, F->addr + i/sizeof(*f));
    lu_int32 s = platform_flash_get_sector_of_address(F->addrPhys + i);
    /* it is far faster not erasing if you don't need to */
#ifdef LUA_USE_ESP
    if (*f == ~0 && !memcmp(f, f + 1, FLASH_PAGE_SIZE - sizeof(*f)))
      continue;
    printf(".");
#endif
    platform_flash_erase_sector(s);
  }
#ifdef LUA_USE_ESP
  printf(" to 0x%06lx\n", F->addrPhys + F->size-1);
#endif
  flush_icache(F);
  lockFlashWrite();
}
#endif

LUAI_FUNC void  luaN_setFlash(void *F, unsigned int o) {
  luaN_flushFlash(F);  /* flush the pending write buffer */
  lua_assert((o & (WORDSIZE-1))==0);
  cast(LFSflashState *,F)->oChunkNdx = o/WORDSIZE;
}

LUAI_FUNC void luaN_flushFlash(void *vF) {
  LFSflashState *F = cast(LFSflashState *, vF);
  lu_int32 start   = F->addrPhys + F->oChunkNdx*WORDSIZE;
  lu_int32 size    = F->oNdx * WORDSIZE;
  lua_assert(start + size < F->addrPhys + F->size); /* is write in bounds? */
//printf("Flush Buf: %6x (%u)\n", F->oNdx, size);                      //DEBUG
  platform_flash_write(F->oBuff, start, size);
  F->oChunkNdx += F->oNdx;
  F->oNdx = 0;
}

LUAI_FUNC void *luaN_writeFlash(void *vF, const void *rec, size_t n) {
  LFSflashState *F = cast(LFSflashState *, vF);
  lu_byte *p   = byteptr(F->addr + F->oChunkNdx + F->oNdx);
//int i; printf("writing %4u bytes:", (lu_int32) n); for (i=0;i<n;i++){printf(" %02x", byteptr(rec)[i]);} printf("\n");
  if (n==0)
    return p;
  while (1) {
    int nw  = ALIGN(F, n);
    if (F->oNdx + nw > OSIZE) {
      /* record overflows the buffer so fill buffer, flush and repeat */
      int rem = OSIZE - F->oNdx;
      if (rem)
        memcpy(F->oBuff+F->oNdx, rec, rem * WORDSIZE);
      rec     = cast(void *, cast(lu_int32 *, rec) + rem);
      n      -= rem * WORDSIZE;
      F->oNdx = OSIZE;
      luaN_flushFlash(F);
    } else {
      /* append remaining record to buffer */
      F->oBuff[F->oNdx+nw-1] = 0;       /* ensure any trailing odd byte are 0 */
      memcpy(F->oBuff+F->oNdx, rec, n);
      F->oNdx += nw;
      break;
    }
  }
//int i; for (i=0;i<(rem * WORDSIZE); i++) {printf("%c%02x",i?' ':'.',*((lu_byte*)rec+i));}
//for (i=0;i<n; i++) printf("%c%02x",i?' ':'.',*((lu_byte*)rec+i));
//printf("\n");
  return p;
}


/*
** Hook used in Lua Startup to carry out the optional LFS startup processes.
*/
LUAI_FUNC int luaN_init (lua_State *L) {
  static LFSflashState *F = NULL;
  static LFSHeader *fh;
  char fname[CONFIG_SPIFFS_OBJ_NAME_LEN-1];
  bool have_load_file = lfs_get_load_filename(fname, sizeof(fname));

 /*
  * The first entry is called from lstate.c:f_luaopen() before modules
  * are initialised.  This is detected because F is NULL on first entry.
  */
  if (F == NULL) {
    size_t Fsize = sizeof(LFSflashState) + OSIZE*WORDSIZE + ISIZE;
    /* outlining the buffers just makes debugging easier.  Sorry */
    F = calloc(Fsize, 1);
    F->oBuff = wordptr(F + 1);
    F->inBuff = byteptr(F->oBuff + OSIZE);

    lfs_location_info_t lfs_loc;
    if (lfs_get_location(&lfs_loc))
    {
      F->size = lfs_loc.size;
      F->addr = cast(lu_int32 *, lfs_loc.addr_mem);
      F->addrPhys = lfs_loc.addr_phys;

      fh = cast(LFSHeader *, F->addr);
      if (!have_load_file) {
        global_State *g = G(L);
        g->LFSsize     = F->size;
        g->l_LFS       = fh;
      /* Set up LFS hooks on normal Entry */
        if (fh->flash_sig   == FLASH_SIG) {
          g->seed        = fh->seed;
          g->ROstrt.hash = cast(TString **, F->addr + fh->oROhash);
          g->ROstrt.nuse = fh->nROuse ;
          g->ROstrt.size = fh->nROsize;
          sethvalue(L, &g->LFStable, cast(Table *, F->addr + fh->protoROTable));
           lua_writestringerror("LFS image %s\n", "loaded");
        } else if ((fh->flash_sig != 0 && fh->flash_sig != ~0)) {
          lua_writestringerror("LFS image %s\n", "corrupted.");
#ifndef CONFIG_NODEMCU_EMBEDDED_LFS_SIZE
          eraseLFS(F);
#endif
        }
      }
    }
    return 0;
  } else {  /* hook 2 called from protected pmain, so can throw errors. */
#ifndef CONFIG_NODEMCU_EMBEDDED_LFS_SIZE
    int status = 0;
    if (have_load_file) {                         /* hook == 2 LFS image load */
      ZIO z;
     /*
      * To avoid reboot loops, the load is only attempted once, so we
      * always deleted the RCR record if we enter this path. Also note
      * that this load process can throw errors and if so these are
      * caught by the parent function in lua.c
      */
#ifdef DEVELOPMENT_USE_GDB
/* For GDB builds,  prefixing the filename with ! forces a break in the hook */
      if (fname[0] == '!') {
        lua_debugbreak();
        F->LFSfileName++;
      }
#endif
      lfs_clear_load_filename();
#ifdef LUA_USE_ESP8266
      luaopen_file(L);
#endif
      if (!(F->f = l_open(fname))) {
        free(F);
        return luaL_error(L, "cannot open %s", fname);
      }
      eraseLFS(F);
      luaZ_init(L, &z, readF, F);
      lua_lock(L);
#ifdef LUA_USE_HOST
      F->allocmask = (LFSaddr == LFSregion) ? sizeof(size_t) - 1 :
                                              sizeof(lu_int32) - 1;
      status = luaU_undumpLFS(L, &z, LFSaddr != LFSregion);
#else
      status = luaU_undumpLFS(L, &z, 0);
#endif
      lua_unlock(L);
      l_close(F->f);
      free(F);
      F = NULL;
      if (status == LUA_OK)
        lua_pushstring(L, "!LFSrestart!");                /* Signal a restart */
      lua_error(L);                          /* throw error / restart request */
    } else {                                     /* hook == 2, Normal startup */
      free(F);
      F = NULL;
    }
    return status;
#else
    return 0; // Embedded LFS - no reloading possible
#endif
  }
}


// =============================================================================
#define getfield(L,t,f) \
  lua_getglobal(L, #t); luaL_getmetafield( L, 1, #f ); lua_remove(L, -2);

LUALIB_API void luaL_lfsreload (lua_State *L) {
#if defined(CONFIG_NODEMCU_EMBEDDED_FLS_SIZE)
  (void)L;
  lua_pushstring(L, "Not allowed to write to LFS section");
  return 1;
#else
#ifdef LUA_USE_ESP
  size_t l;
  int off = 0;
  const char *img = lua_tolstring(L, 1, &l);
#ifdef DEVELOPMENT_USE_GDB
  if (*img == '!')   /* For GDB builds, any leading ! is ignored for checking */
    off = 1;         /* existence. This forces a debug break in the init hook */
#endif
  lua_settop(L, 1);
  lua_getglobal(L, "file");
  if (lua_isnil(L, 2)) {
    lua_pushstring(L, "No file system mounted");
    return;
  }
  lua_getfield(L, 2, "exists");
  lua_pushstring(L, img + off);
  lua_call(L, 1, 1);
  if (G(L)->LFSsize == 0 || lua_toboolean(L, -1) == 0) {
    lua_pushstring(L, "No LFS partition allocated");
    return;
  }
  if (lfs_set_load_filename(img))
  {
    esp_restart();
    luaL_error(L, "system restarting");
  }
#else
  (void)L;
#endif
#endif
}


#ifdef LUA_USE_ESP
/*
** Task callback handler to support pcallx with full traceback
*/
static void do_task (task_param_t task_fn_ref, task_prio_t prio) {
  lua_State* L = lua_getstate();
  if (prio < LUA_TASK_LOW || prio > LUA_TASK_HIGH)
    luaL_error(L, "invalid post task");
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
  static task_handle_t task_handle = 0;
  if (!task_handle)
    task_handle = task_get_id(do_task);
  if (lua_isfunction(L, -1) && prio >= LUA_TASK_LOW && prio <= LUA_TASK_HIGH) {
    int task_fn_ref = luaL_ref(L, LUA_REGISTRYINDEX);
    if(!task_post(prio, task_handle, (task_param_t)task_fn_ref)) {
      luaL_unref(L, LUA_REGISTRYINDEX, task_fn_ref);
      luaL_error(L, "Task queue overflow. Task not posted");
    }
    return task_fn_ref;
  } else {
    return luaL_error(L, "invalid post task");
  }
}
#else
/*
** Task execution isn't supported on HOST builds so returns a -1 status
*/
LUALIB_API int luaL_posttask( lua_State* L, int prio ) {            // [-1, +0, -]
  (void)L; (void)prio;
  return -1;
}
#endif
