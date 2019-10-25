
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
#include "lstring.h"
#include "ltable.h"
#include "ltm.h"
#include "lnodemcu.h"
#include "lundump.h"
#include "lzio.h"

#ifdef LUA_USE_ESP
#include "platform.h"
#include "user_interface.h"
#include "vfs.h"
#endif

/*
** This is a mixed bag of NodeMCU additions broken into the following sections:
**    *  POSIX vs VFS file API abstraction
**    *  Emulate Platform_XXX() API
**    *  ESP and HOST lua_debugbreak() test stubs
**    *  NodeMCU lua.h API extensions
**    *  NodeMCU LFS Table emulator
**    *  NodeMCU bootstrap to set up and to reimage LFS resources
**
** Just search down for //== or ==// to flip through the sections.
*/

#define byte_addr(p) cast(char *,p)
//== Wrap POSIX and VFS file API =============================================//
#ifdef LUA_USE_ESP
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

//== Emulate Platform_XXX() API ==============================================//
#ifdef LUA_USE_ESP

extern void dbg_printf(const char *fmt, ...);                // DEBUG
#undef printf
#define printf(...) dbg_printf(__VA_ARGS__)                  // DEBUG

#define FLASH_PAGE_SIZE INTERNAL_FLASH_SECTOR_SIZE
/* Erasing the LFS invalidates ESP instruction cache,  so doing a block 64Kb */
/* read is the simplest way to flush the icache, restoring cache coherency */
#define flush_icache(F) \
  UNUSED(memcmp(F->addr, F->addr+(0x8000/sizeof(*F->addr)), 0x8000));
#define unlockFlashWrite()
#define lockFlashWrite()
 
#else // LUA_USE_HOST

#include<stdio.h>                                             // DEBUG


/*
** The ESP implementation use a platform_XXX() API to provide a level of 
** H/W abstraction.  The following functions and macros emulate a subset
** of this API for the host environment.
*/
void *LFSregion = NULL;
extern char *LFSimageName;

#define PLATFORM_RCR_FLASHLFS  4
#define LFS_SIZE         0x40000
#define FLASH_PAGE_SIZE   0x1000
#define FLASH_BASE       0x90000           /* Some 'Random' but typical value */

#ifdef __unix__       
/* On POSIX systems we can toggle the "Flash" write attribute */
#include <sys/mman.h>
#define aligned_malloc(a,n) posix_memalign(&a, FLASH_PAGE_SIZE, (n))
#define unlockFlashWrite() mprotect(LFSregion, LFS_SIZE, PROT_READ| PROT_WRITE)
#define lockFlashWrite() mprotect(LFSregion, LFS_SIZE, PROT_READ) 
#else
#define aligned_malloc(a,n) ((a = malloc(n)) == NULL)
#define unlockFlashWrite()
#define lockFlashWrite()
#endif

#define platform_rcr_write(id,rec,l) (128)
#define platform_flash_phys2mapped(n) \
    (byte_addr(LFSregion) + (n) - FLASH_BASE)
#define platform_flash_mapped2phys(n) \
    ((byte_addr((n)) - byte_addr(LFSregion)) + FLASH_BASE)
#define platform_flash_get_sector_of_address(n) ((n)>>12)
#define platform_rcr_delete(id) LFSimageName = NULL
#define platform_rcr_read(id,s) \
  (*s = LFSimageName, (LFSimageName) ? strlen(LFSimageName) : ~0);

static lu_int32 platform_flash_get_partition (lu_int32 part_id, lu_int32 *addr){
  lua_assert(part_id == NODEMCU_LFS0_PARTITION);
  if (!LFSregion) {
    lua_assert(!aligned_malloc(LFSregion, LFS_SIZE));
    memset(LFSregion, ~0, LFS_SIZE);
    lockFlashWrite();
  }
  *addr = FLASH_BASE;                                          
  return LFS_SIZE;
}

static void platform_flash_erase_sector(lu_int32 i) {
/* DEBUG */ lua_assert (i >= 144 && i < 208); 
  unlockFlashWrite();
  memset(cast(char *,LFSregion) + i*FLASH_PAGE_SIZE, ~(0), FLASH_PAGE_SIZE);
  lockFlashWrite();
}

static void platform_s_flash_write(const void *from, lu_int32 to, lu_int32 len) {  
  lua_assert(to >= 0x90000 && to + len < 0xb0000);  /* DEBUG */
  unlockFlashWrite();
  memcpy(cast(char *,LFSregion) + (to-FLASH_BASE), from, len);
  lockFlashWrite();
}

#define flush_icache(F)   /* not needed */

#endif

//== ESP and HOST lua_debugbreak() test stubs ================================//

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
#else
#define lua_debugbreak() (void)(0)
#endif

//== NodeMCU lua.h API extensions ============================================//

LUA_API int lua_freeheap (void) {
#ifdef LUA_USE_HOST
  return MAX_INT;
#else
  return (int) platform_freeheap();
#endif
}

LUA_API int lua_getstrings(lua_State *L, int opt) {
  stringtable *tb = NULL;
  Table *t;
  int i, j, n = 0;

  if (n == 0)
    tb = &G(L)->strt;
#ifdef LUA_USE_ESP
  else if (n == 1 && G(L)->ROstrt.hash)
    tb = &G(L)->ROstrt;
#endif
  if (tb == NULL)
   return 0;

  lua_lock(L);
  t = luaH_new(L);
  sethvalue(L, L->top, t);
  api_incr_top(L);
  luaH_resize(L, t, tb->nuse, 0);
  luaC_checkGC(L);
  lua_unlock(L);

  for (i = 0, j = 1; i < tb->size; i++) {
    TString *o;
    for(o = tb->hash[i]; o; o = o->u.hnext) {
      TValue s;
      setsvalue(L, &s, o);
      luaH_setint(L, hvalue(L->top-1), j++, &s);  /* table[n] = true */
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

//== NodeMCU LFS Table emulator ==============================================//

static int lfs_func (lua_State* L);

LROT_BEGIN(LFS_meta, NULL, LROT_MASK_INDEX)
  LROT_FUNCENTRY( __index, lfs_func)
LROT_END(LFS_meta, NULL, LROT_MASK_INDEX)

LROT_BEGIN(LFS, LROT_TABLEREF(LFS_meta), 0)
LROT_END(LFS, LROT_TABLEREF(LFS_meta), 0)


static int lfs_func (lua_State* L) {           /*T[1] = LFS, T[2] = fieldname */
  const char *name = lua_tostring(L, 2);
  LFSHeader *fh = G(L)->l_LFS;
  Proto *f;
  LClosure *cl;
  lua_settop(L,2);
  if (!fh) {                                  /* return nil if LFS not loaded */
    lua_pushnil(L);
    return 1;
  }
  if (!strcmp(name, "_config")) {
    size_t ba = cast(size_t, fh);
    lua_createtable(L, 0, 3);
    lua_pushinteger(L, cast(lua_Integer, ba));
    lua_setfield(L, -2, "lfs_mapped");
    lua_pushinteger(L, cast(lua_Integer, platform_flash_mapped2phys(ba)));
    lua_setfield(L, -2, "lfs_base");
    lua_pushinteger(L, G(L)->LFSsize);
    lua_setfield(L, -2, "lfs_size");
    return 1;
  } else if (!strcmp(name, "_list")) {
    int i = 1;
    setobjs2s(L, L->top-2, &G(L)->LFStable);  /* overwrite T[1] with LSFtable */
    lua_newtable(L);                                /* new list table at T[3] */
    lua_pushnil(L);                                 /* first key (nil) at T4] */
    while (lua_next(L, 1) != 0) {         /* loop over LSFtable k,v at T[4:5] */
      lua_pop(L, 1);                                            /* dump value */
      lua_pushvalue(L, -1);                                        /* dup key */
      lua_rawseti(L, 3, i++);                                 /* table[i]=key */
    }
    return 1;
  } else if (!strcmp(name, "_time")) {
    lua_pushinteger(L, fh->timestamp);
    return 1;
  }
  setobjs2s(L, L->top-2, &G(L)->LFStable);    /* overwrite T[1] with LSFtable */
  if (lua_rawget(L,1) != LUA_TNIL) {                    /* get LFStable[name] */
    lua_pushglobaltable(L);
    f = cast(Proto *, lua_topointer(L,-2));
    lua_lock(L);
    cl = luaF_newLclosure(L, f->sizeupvalues);
    setclLvalue(L, L->top-2, cl);       /* overwrite f addr slot with closure */
    cl->p = f;                                                /* bind f to it */
    if (cl->nupvalues >= 1) {           /* does it have at least one upvalue? */
      luaF_initupvals(L, cl );                            /* initialise upvals */
      setobj(L, cl->upvals[0]->v, L->top-1);             /* set UV[1] to _ENV */
    }
    lua_unlock(L);
    lua_pop(L,1);                          /* pop _ENV leaving closure at ToS */
  }
  return 1;
}

//== NodeMCU bootstrap to set up and to reimage LFS resources ================//
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
  const char *LFSfileName;
  size_t     *addr;
  lu_int32    oNdx;               /* in size_t units */
  lu_int32    oChunkNdx;          /* in size_t units */
  size_t     *oBuff;              /* FLASH_PAGE_SIZE bytes */
  lu_byte    *inBuff;             /* FLASH_PAGE_SIZE bytes */
  lu_int32    inNdx;              /* in bytes */
  lu_int32    addrPhys;
  lu_int32    size;
  stringtable ROstrt;
  GCObject   *pLTShead;
} LFSflashState;
#define WORDSIZE sizeof(size_t)
#define WORDS(n) (n + WORDSIZE - 1) / WORDSIZE;
#define OSIZE (FLASH_PAGE_SIZE/WORDSIZE)
#define ISIZE (FLASH_PAGE_SIZE)

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
  lua_writestringerror("\nErasing LFS from flash addr 0x%06x", F->addrPhys);
  unlockFlashWrite();
  for (i = 0; i < F->size; i += FLASH_PAGE_SIZE) {
    lu_int32 s = platform_flash_get_sector_of_address(F->addrPhys + i);
printf(".");
    platform_flash_erase_sector(s);
  }
  lua_writestringerror(" to 0x%06x\n", F->addrPhys + F->size-1);
  flush_icache(F);
  lockFlashWrite();
}

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
  platform_s_flash_write(F->oBuff, start, size);
  F->oChunkNdx += F->oNdx;
  F->oNdx = 0;
}

LUAI_FUNC char *luaN_writeFlash(void *vF, const void *rec, size_t n) {
  LFSflashState *F = cast(LFSflashState *, vF);
  char *p   = byte_addr(F->addr + F->oChunkNdx + F->oNdx);
  if (n==0)
    return p;
//if (p - (char*)F->addr > 0x7828)  lua_debugbreak();                  //DEBUG
  
  while (1) {
//lu_int32 offset = (lu_int32)(p-(char*)F->addr);
    int   nw  = WORDS(n);
    if (F->oNdx + nw > OSIZE) {
      /* record overflows the buffer so fill buffer, flush and repeat */
      int rem = OSIZE - F->oNdx;
      if (rem)
        memcpy(F->oBuff+F->oNdx, rec, rem * WORDSIZE);
      rec     = cast(void *, cast(size_t *, rec) + rem);
      n      -= rem * WORDSIZE;
      F->oNdx = OSIZE;
//printf("Flash OP:  %06x %x\n", offset, rem);  //DEBUG
      luaN_flushFlash(F);
    } else {
      /* append remaining record to buffer */
      F->oBuff[F->oNdx+nw-1] = 0;       /* ensure any trailing odd byte are 0 */
      memcpy(F->oBuff+F->oNdx, rec, n);
      F->oNdx += nw;
//printf("Flash OP:  %06x %x (%u)\n", offset, nw, n);  //DEBUG
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
LUAI_FUNC int luaN_init (lua_State *L, int hook) {
  static LFSflashState *F;
  int n;
  static LFSHeader *fh;
  /* Entry 1 is called from lstate.c:f_luaopen() before modules are initialised */
  if (hook == 1) {
    size_t Fsize = sizeof(LFSflashState) + OSIZE*WORDSIZE + ISIZE;
    /* outlining the buffers just makes debugging easier.  Sorry */
    F = calloc(Fsize, 1);
    F->oBuff = cast(size_t *, F + 1);
    F->inBuff = cast(lu_byte *, F->oBuff + OSIZE);
    n = platform_rcr_read(PLATFORM_RCR_FLASHLFS, cast(void**, &F->LFSfileName));
    F->size = platform_flash_get_partition (NODEMCU_LFS0_PARTITION, &F->addrPhys);
    if (F->size) {
      F->addr  = cast(size_t *, platform_flash_phys2mapped(F->addrPhys));
      fh = cast(LFSHeader *, F->addr);
      if (n < 0) {
        global_State *g = G(L);
        g->LFSsize     = F->size;
      /* Set up LFS hooks on normal Entry */
        if (fh->flash_sig   == FLASH_SIG) {
          g->l_LFS       = fh;
          g->seed        = fh->seed;
          g->ROstrt.hash = fh->pROhash;
          g->ROstrt.nuse = fh->nROuse ;
          g->ROstrt.size = fh->nROsize;
          sethvalue(L, &g->LFStable, cast(Table *, fh->protoROTable));
           lua_writestringerror("LFS image %s\n", "loaded");
        } else if ((fh->flash_sig != 0 && fh->flash_sig != ~0)) {
          lua_writestringerror("LFS image %s\n", "corrupted.");
          eraseLFS(F);
        }
      }
    } 
    return 0;
  } else {  /* hook 2 called from protected pmain, so can throw errors. */
    int status = 0;
    if (F->LFSfileName) {                         /* hook == 2 LFS image load */
      ZIO z;
     /*
      * To avoid reboot loops, the load is only attempted once, so we
      * always deleted the RCR record if we enter this path. Also note
      * that this load process can throw errors and if so these are
      * caught by the parent function in lua.c
      */
#ifdef DEVELOPMENT_USE_GDB
/* For GDB builds,  prefixing the filename with ! forces a break in the hook */
      if (F->LFSfileName[0] == '!') {
        lua_debugbreak();    
        F->LFSfileName++;
      }
#endif
      platform_rcr_delete(PLATFORM_RCR_FLASHLFS);
      if (!(F->f = l_open(F->LFSfileName))) {
        free(F);
        return luaL_error(L, "cannot open %s", F->LFSfileName);
      }
      eraseLFS(F);
      luaZ_init(L, &z, readF, F);
      lua_lock(L);
      status = luaU_undumpLFS(L, &z);
      lua_unlock(L);
      l_close(F->f);
      free(F);
      if (status != LUA_OK)
        lua_error(L);                                        /* rethrow error */
      status = 1;                                     /* return 1 == restart! */
    } else {                                     /* hook == 2, Normal startup */
      free(F);
    }
    return status;
  }
}


// =============================================================================
#define getfield(L,t,f) \
  lua_getglobal(L, #t); luaL_getmetafield( L, 1, #f ); lua_remove(L, -2);

LUAI_FUNC int  luaN_reload_reboot (lua_State *L) {
  int n = 0;
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
  lua_getfield(L, 2, "exists");
  lua_pushstring(L, img + off);
  lua_call(L, 1, 1);
  if (G(L)->LFSsize == 0 || lua_toboolean(L, -1) == 0) {
    lua_pushstring(L, "No LFS partition allocated");
    return 1;
  }
  n = platform_rcr_write(PLATFORM_RCR_FLASHLFS, img, l+1);/* incl trailing \0 */
  if (n>0)
    system_restart();
#endif
  lua_pushboolean(L, n>0);
  return 1;
}

LUAI_FUNC int  luaN_index (lua_State *L) {
  lua_settop(L,1);
  if (lua_isstring(L, 1)){
    lua_getglobal(L, "LFS");
    lua_getfield(L, 2, lua_tostring(L,1));
  } else {
    lua_pushnil(L);
  }
  return 1;
}
