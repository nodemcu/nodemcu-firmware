/*
** $Id: lflash.c
** See Copyright Notice in lua.h
*/

#define lflash_c
#define LUA_CORE
#define LUAC_CROSS_FILE
#include "lua.h"

#ifdef LUA_FLASH_STORE
#include "lobject.h"
#include "lauxlib.h"
#include "lstate.h"
#include "lfunc.h"
#include "lflash.h"
#include "platform.h"
#include "vfs.h"

#include "c_fcntl.h"
#include "c_stdio.h"
#include "c_stdlib.h"
#include "c_string.h"

/*
 * Flash memory is a fixed memory addressable block that is serially allocated by the
 * luac build process and the out image can be downloaded into SPIFSS and loaded into
 * flash with a node.flash.load() command. See luac_cross/lflashimg.c for the build
 * process.
 */

static char    *flashAddr;
static uint32_t flashAddrPhys;
static uint32_t flashSector;
static uint32_t curOffset;

#define ALIGN(s)     (((s)+sizeof(size_t)-1) & ((size_t) (- (signed) sizeof(size_t))))
#define ALIGN_BITS(s) (((uint32_t)s) & (sizeof(size_t)-1))
#define ALL_SET      cast(uint32_t, -1)
#define FLASH_SIZE   LUA_FLASH_STORE
#define FLASH_PAGE_SIZE INTERNAL_FLASH_SECTOR_SIZE
#define FLASH_PAGES  (FLASH_SIZE/FLASH_PAGE_SIZE)

char flash_region_base[FLASH_SIZE] ICACHE_FLASH_RESERVED_ATTR;

#ifdef NODE_DEBUG
extern void dbg_printf(const char *fmt, ...) __attribute__ ((format (printf, 1, 2)));
void dumpStrt(stringtable *tb, const char *type) {
  int i,j;
  GCObject *o;

  NODE_DBG("\nDumping %s String table\n\n========================\n", type);
  NODE_DBG("No of elements: %d\nSize of table: %d\n", tb->nuse, tb->size);
  for (i=0; i<tb->size; i++)
    for(o = tb->hash[i], j=0; o; (o=o->gch.next), j++ ) {
      TString *ts =cast(TString *, o);
      NODE_DBG("%5d %5d %08x %08x %5d %1s %s\n",  
               i, j, (size_t) ts, ts->tsv.hash, ts->tsv.len,
               ts_isreadonly(ts) ? "R" : " ",  getstr(ts));
    }
} 

LUA_API void dumpStrings(lua_State *L) {
  dumpStrt(&G(L)->strt, "RAM");
  if (G(L)->ROstrt.hash)
    dumpStrt(&G(L)->ROstrt, "ROM");
}
#endif

/* =====================================================================================
 * The next 4 functions: flashPosition, flashSetPosition, flashBlock and flashErase
 * wrap writing to flash. The last two are platform dependent.  Also note that any
 * writes are suppressed if the global writeToFlash is false.  This is used in 
 * phase I where the pass is used to size the structures in flash.
 */
static char *flashPosition(void){
  return  flashAddr + curOffset;
}


static char *flashSetPosition(uint32_t offset){
  NODE_DBG("flashSetPosition(%04x)\n", offset);
  curOffset = offset;
  return flashPosition();
}


static char *flashBlock(const void* b, size_t size)  {
  void *cur = flashPosition();
  NODE_DBG("flashBlock((%04x),%08x,%04x)\n", curOffset,b,size);
  lua_assert(ALIGN_BITS(b) == 0 && ALIGN_BITS(size) == 0);
  platform_flash_write(b, flashAddrPhys+curOffset, size);
  curOffset += size;
  return cur;
}


static void flashErase(uint32_t start, uint32_t end){
  int i;
  if (start == -1) start = FLASH_PAGES - 1;
  if (end == -1) end = FLASH_PAGES - 1;
  NODE_DBG("flashErase(%04x,%04x)\n", flashSector+start, flashSector+end);
  for (i = start; i<=end; i++)
    platform_flash_erase_sector( flashSector + i );
}


/*
 * Hook in lstate.c:f_luaopen() to set up ROstrt and ROpvmain if needed
 */  
LUAI_FUNC void luaN_init (lua_State *L) {
//  luaL_dbgbreak();
  curOffset      = 0;
  flashAddr     = flash_region_base;
  flashAddrPhys = platform_flash_mapped2phys((uint32_t)flashAddr);
  flashSector   = platform_flash_get_sector_of_address(flashAddrPhys);
  FlashHeader *fh = cast(FlashHeader *, flashAddr);

  /*
   * For the LFS to be valid, its signature has to be correct for this build variant,
   * thr ROhash and main proto fields must be defined and the main proto address 
   * be within the LFS address bounds. (This last check is primarily to detect the
   * direct imaging of an absolute LFS with the wrong base address. 
   */

  if ((fh->flash_sig & (~FLASH_SIG_ABSOLUTE)) != FLASH_SIG ) {
    NODE_ERR("Flash sig not correct: %p vs %p\n",
       fh->flash_sig & (~FLASH_SIG_ABSOLUTE), FLASH_SIG); 
    return;
  }

  if (fh->pROhash == ALL_SET ||
      ((fh->mainProto - cast(FlashAddr, fh)) >= fh->flash_size)) {
    NODE_ERR("Flash size check failed: %p vs 0xFFFFFFFF; %p >= %p\n",
       fh->mainProto - cast(FlashAddr, fh), fh->flash_size);
    return;
  }
 
  G(L)->ROstrt.hash = cast(GCObject **, fh->pROhash);
  G(L)->ROstrt.nuse = fh->nROuse ;
  G(L)->ROstrt.size = fh->nROsize;
  G(L)->ROpvmain    = cast(Proto *,fh->mainProto);
}

#define BYTE_OFFSET(t,f) cast(size_t, &(cast(t *, NULL)->f))
/*
 * Rehook address chain to correct Flash byte addressed within the mapped adress space 
 * Note that on input each 32-bit address field is split into 2×16-bit subfields
 *  -  the lu_int16 offset of the target address being referenced
 *  -  the lu_int16 offset of the next address pointer. 
 */

static int rebuild_core (int fd, uint32_t size, lu_int32 *buf, int is_absolute) {
  int bi;  /* byte offset into memory mapped LFS of current buffer */ 
  int wNextOffset = BYTE_OFFSET(FlashHeader,mainProto)/sizeof(lu_int32);
  int wj;  /* word offset into current input buffer */
  for (bi = 0; bi < size; bi += FLASH_PAGE_SIZE) {
    int wi   = bi / sizeof(lu_int32);
    int blen = ((bi + FLASH_PAGE_SIZE) < size) ? FLASH_PAGE_SIZE : size - bi;
    int wlen = blen / sizeof(lu_int32);
    if (vfs_read(fd, buf , blen) != blen)
      return 0;

    if (!is_absolute) {
      for (wj = 0; wj < wlen; wj++) {
        if ((wi + wj) == wNextOffset) {  /* this word is the next linked address */
          int wTargetOffset = buf[wj]&0xFFFF;
          wNextOffset = buf[wj]>>16;
          lua_assert(!wNextOffset || (wNextOffset>(wi+wj) && wNextOffset<size/sizeof(lu_int32)));
          buf[wj] = cast(lu_int32, flashAddr + wTargetOffset*sizeof(lu_int32));
        }
      }
    }

    flashBlock(buf, blen);
  }
  return size;
}


/*
 * Library function called by node.flash.load(filename).
 */
LUALIB_API int luaN_reload_reboot (lua_State *L) {
  int fd, status, is_absolute;
  FlashHeader fh;

  const char *fn = lua_tostring(L, 1);
  if (!fn || !(fd = vfs_open(fn, "r")))
    return 0;
   
  if (vfs_read(fd, &fh, sizeof(fh)) != sizeof(fh) ||
      (fh.flash_sig & (~FLASH_SIG_ABSOLUTE)) != FLASH_SIG)
    return 0;

  if (vfs_lseek(fd, -1, VFS_SEEK_END) != fh.flash_size-1 ||
      vfs_lseek(fd, 0, VFS_SEEK_SET) != 0)
    return 0;

  is_absolute = fh.flash_sig & FLASH_SIG_ABSOLUTE;
  lu_int32 *buffer = luaM_newvector(L, FLASH_PAGE_SIZE / sizeof(lu_int32), lu_int32);

  /*
   * This is the point of no return.  We attempt to rebuild the flash.  If there
   * are any problems them the Flash is going to be corrupt, so the only fallback
   * is to erase it and reboot with a clean but blank flash.  Otherwise the reboot
   * will load the new LFS.
   *
   * Note that the Lua state is not passed into the lua core because from this 
   * point on, we make no calls on the Lua RTS.
   */
  flashErase(0,-1); 
  if (rebuild_core(fd, fh.flash_size, buffer, is_absolute) != fh.flash_size)
    flashErase(0,-1);  
  /*
   * Issue a break 0,0.  This will either enter the debugger or force a restart if
   * not installed.  Follow this by a H/W timeout is a robust way to insure that
   * other interrupts / callbacks don't fire and reference THE old LFS context.
   */
  asm("break 0,0" ::);
  while (1) {}

  return 0;
}


/*
 * In the arg is a valid LFS module name then return the LClosure pointing to it.
 * Otherwise return:
 *  -  The Unix time that the LFS was built
 *  -  The base address and length of the LFS
 *  -  An array of the module names in the the LFS
 */
LUAI_FUNC int luaN_index (lua_State *L) {
  int i;
  int n = lua_gettop(L);

  /* Return nil + the LFS base address if the LFS isn't loaded */ 
  if(!(G(L)->ROpvmain)) {
    lua_settop(L, 0);
    lua_pushnil(L);
    lua_pushinteger(L, (lua_Integer) flashAddr);
    lua_pushinteger(L, flashAddrPhys);
    return 3;
  }

  /* Push the LClosure of the LFS index function */
  Closure *cl = luaF_newLclosure(L, 0, hvalue(gt(L)));
  cl->l.p = G(L)->ROpvmain;
  lua_settop(L, n+1);
  setclvalue(L, L->top-1, cl);

  /* Move it infront of the arguments and call the index function */
  lua_insert(L, 1);
  lua_call(L, n, LUA_MULTRET);

  /* Return it if the response if a single value (the function) */
  if (lua_gettop(L) == 1)
    return 1;

  lua_assert(lua_gettop(L) == 2);

  /* Otherwise add the base address of the LFS, and its size bewteen the */
  /* Unix time and the module list, then return all 4 params. */
  lua_pushinteger(L, (lua_Integer) flashAddr);
  lua_insert(L, 2);
  lua_pushinteger(L, flashAddrPhys);
  lua_insert(L, 3);
  lua_pushinteger(L, cast(FlashHeader *, flashAddr)->flash_size);
  lua_insert(L, 4);
  return 5;
}

#endif
