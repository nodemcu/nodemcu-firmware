
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
#include "vfs.h"
#endif


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
  lua_writestring(" lua_debugbreak", sizeof(" lua_debugbreak")-1);
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


/*
** Functions to set up and reimage LFS resources. This processing uses 2 init
** hooks during the Lua startup. The first is called early in the Lua state
** setup to initialize the LFS if present. The second is only used to rebuild
** the LFS region; this requires the full Lua environment to be in place, so
** this second hook is immediately before processing LUA_INIT.
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



#define getfield(L,t,f) \
  lua_getglobal(L, #t); \
  luaL_getmetafield( L, 1, #f );\
  lua_remove(L, -2);

/*
 * Wrap POSIX and VFS file API to make code common
 */
#ifdef LUA_USE_ESP
# define l_file(f)   int f
# define l_open(f)   vfs_open(f, "r")
# define l_close(f)  vfs_close(f)
# define l_feof(f)   vfs_eof(f)
# define l_read(f,b) vfs_read(f, b, sizeof (b))
# define l_rewind(f) vfs_lseek(f, 0, VFS_SEEK_SET)
#else
# define l_file(f)   FILE *f
# define l_open(n)   fopen(n,"rb")
# define l_close(f)  fclose(f)
# define l_feof(f)   feof(f)
# define l_read(f,b) fread(b, 1, sizeof (b), f)
# define l_rewind(f) rewind(f)
#endif

typedef struct LFSflashState {
  lua_State  *L;
  LFSHeader   hdr;
  l_file(f);
  const char *LFSfileName;
  size_t     *addr;
  lu_int32    oNdx;               /* in size_t units */
  lu_int32    oChunkNdx;          /* in size_t units */
  size_t      oBuff[BUFSIZ];
  lu_byte     inBuff[BUFSIZ];
  lu_int32    inNdx;              /* in bytes */
  lu_int32    addrPhys;
  lu_int32    startSector;
  lu_int32    size;
  stringtable ROstrt;
  GCObject   *pLTShead;
} LFSflashState;


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


#ifdef LUA_USE_HOST //==========================================================
/*
** The luaN_init hooks are designed to allow the NodeMCU firmware to connect
** the LFS region during startup.  A simple load-and-go LFS capability has
** been added to luac.cross for testing purposes, so the following platform_XXX
** stubs emulate the H/W abstraction layer for the ESP SoCs to enable this
** code to run in a host environment.
*/
#define PLATFORM_RCR_FLASHLFS 0
#define LFS_SIZE 0x20000
/*static*/ size_t LFSregion[LFS_SIZE/sizeof(size_t)];
extern char *LFSimageName;
static lu_int32 platform_rcr_read (uint8_t rec_id, void **rec) {
  UNUSED(rec_id);
  *rec = LFSimageName;
  return (LFSimageName) ? strlen(LFSimageName) : 0;
}
static lu_int32 platform_flash_get_partition (lu_int32 part_id, lu_int32 *addr){
  UNUSED(part_id);
  *addr = 0x100000;  /* some arbitrary figure */
  return sizeof(LFSregion);
}
#define platform_rcr_write(id,rec,l) (128)
#define platform_flash_phys2mapped(n) LFSregion
#define platform_flash_mapped2phys(n) ((n)&0x100000)
#define platform_flash_get_sector_of_address(n) (n>>12)
#define platform_rcr_delete(id) LFSimageName = NULL
#undef NODE_ERR
#define NODE_ERR(s) fprintf(stderr, s)

static void eraseLFS(LFSflashState *F) {
  memset(F->addr, ~0, F->size);
}

LUAI_FUNC void luaN_flushFlash(void *vF) {
  LFSflashState *F = cast(LFSflashState *, vF);
  memcpy(F->addr + F->oChunkNdx, F->oBuff, F->oNdx * sizeof(size_t));
  F->oChunkNdx += F->oNdx;
  F->oNdx = 0;
}

#else // LUA_USE_ESP ===========================================================

static void eraseLFS(LFSflashState *F) {
  int start = F->startSector;
  int end = F->size / INTERNAL_FLASH_SECTOR_SIZE;
  int i;
  for (i = start; i<=start+end; i++)
    platform_flash_erase_sector(i);
}

LUAI_FUNC void luaN_flushFlash(LFSflashState *F) {
  platform_s_flash_write(F->oBuff,
                         F->addrPhys + F->oChunkNdx*sizeof(size_t),
                         F->oNdx * sizeof(size_t));
  F->oChunkNdx += F->oNdx;
  F->oNdx = 0;
}

#endif // LUA_USE_ESP/HOST =====================================================


LUAI_FUNC char *luaN_writeFlash(void *vF, const void *rec, size_t n) {
  LFSflashState *F = cast(LFSflashState *, vF);
  int nw = (n + sizeof(size_t) - 1) / sizeof(size_t);
  int rem = BUFSIZ - F->oNdx;
  char *p = cast(char *, F->addr + F->oChunkNdx + F->oNdx);

  if (n==0)
    return p;
  while (1) {
    if (nw > rem) { /* fill the remainder of the oBuff & flush to flash */
      if (rem)
        memcpy(F->oBuff+F->oNdx, rec, rem * sizeof(size_t));
      rec = cast(void *, cast(size_t *, rec) + rem);
      F->oNdx = BUFSIZ;
      n  -= rem * sizeof(size_t);
      nw -= rem;
      luaN_flushFlash(F);
    } else {
      F->oBuff[F->oNdx+nw-1] = 0;
      memcpy(F->oBuff+F->oNdx, rec, n);
      F->oNdx += nw;
      break;
    }
  }
  return p;
}


LUAI_FUNC void  luaN_setFlash(void *F, unsigned int o) {
  luaN_flushFlash(F);  /* flush the pending write buffer */
  lua_assert((o & (sizeof(size_t)-1))==0);
  cast(LFSflashState *,F)->oChunkNdx = o/sizeof(size_t);
}


/*
** Hook used in Lua Startup to carry out the optional LFS startup processes.
*/
LUAI_FUNC int luaN_init (lua_State *L, int hook) {
  static LFSflashState *F;
  int n;
  static LFSHeader *fh;
  static const char *LFSfileName=NULL;
  /* Entry 1 is called from lstate.c:f_luaopen() before modules are initialised */
  if (hook == 1) {
    F = calloc(sizeof(LFSflashState), 1);
    n = platform_rcr_read(PLATFORM_RCR_FLASHLFS, cast(void**, &LFSfileName));
    F->LFSfileName = LFSfileName;
    F->size = platform_flash_get_partition (NODEMCU_LFS0_PARTITION, &F->addrPhys);
    if (F->size) {
      F->addr  = cast(size_t *, platform_flash_phys2mapped(F->addrPhys));
      F->startSector = (F->addrPhys);
      fh = cast(LFSHeader *, F->addr);
      if (n == 0) {
        /* Set up LFS hooks on normal Entry */
        if (fh->flash_sig   == FLASH_SIG) {
          global_State *g = G(L);
          g->l_LFS       = fh;
          g->seed        = fh->seed;
          g->ROstrt.hash = fh->pROhash;
          g->ROstrt.nuse = fh->nROuse ;
          g->ROstrt.size = fh->nROsize;
          sethvalue(L, &g->LFStable, cast(Table *, fh->protoROTable));
           lua_writestringerror("LFS image %s\n", "loaded");
        } else {
          if ((fh->flash_sig != 0 && fh->flash_sig != ~0)) {
            lua_writestringerror("LFS image %s\n", "corrupted.  Erasing.");
            eraseLFS(F);
          }
        }
      }
    }
    return 0;
  } else {  /* hook 2 called from protected pmain, so can throw errors. */
    int status = 0;
    if (LFSfileName) {                         /* hook == 2 LFS image load */
      ZIO z;
      if (!(F->f = l_open(LFSfileName))) {
        free(F);
        return luaL_error(L, "cannot open %s", LFSfileName);
      }
      luaZ_init(L, &z, readF, F);
      lua_lock(L);
      status = luaU_undumpLFS(L, &z);
      lua_unlock(L);
      l_close(F->f);
      free(F);
      if (status != LUA_OK)
        lua_error(L);                                        /* rethrow error */
      platform_rcr_delete(PLATFORM_RCR_FLASHLFS);
      status = 1;                                     /* return 1 == restart! */
    } else {                                     /* hook == 2, Normal startup */
      free(F);
    }
    if (fh->flash_sig != FLASH_SIG && fh->flash_sig == ~0){
      eraseLFS(F);
      lua_writestringerror("%s\n", "\n*** erasing LFS ***\n");
    }
    return status;
  }
}


// =============================================================================
LUAI_FUNC int  luaN_reload_reboot (lua_State *L) {
  return 0;
}

LUAI_FUNC int  luaN_index (lua_State *L) {
  return 0;
}

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
    lua_Integer ba = cast(lua_Integer, cast(size_t, fh));
    lua_createtable(L, 0, 3);
    lua_pushinteger(L, ba);
    lua_setfield(L, -2, "lfs_mapped");
    lua_pushinteger(L, platform_flash_mapped2phys(ba));
    lua_setfield(L, -2, "lfs_base");
    lua_pushinteger(L, fh->nROsize);
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


LROT_BEGIN(LFS_meta, NULL, LROT_MASK_INDEX)
  LROT_FUNCENTRY( __index, lfs_func)
LROT_END(LFS_meta, NULL, LROT_MASK_INDEX)

LROT_BEGIN(LFS, LROT_TABLEREF(LFS_meta), 0)
LROT_END(LFS, LROT_TABLEREF(LFS_meta), 0)

