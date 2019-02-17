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
#include "uzlib.h"

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

#define ALIGN(s)      (((s)+sizeof(size_t)-1) & ((size_t) (- (signed) sizeof(size_t))))
#define ALIGN_BITS(s) (((uint32_t)s) & (sizeof(size_t)-1))
#define ALL_SET       (~0)
#define FLASH_SIZE    LUA_FLASH_STORE
#define FLASH_PAGE_SIZE INTERNAL_FLASH_SECTOR_SIZE
#define FLASH_PAGES   (FLASH_SIZE/FLASH_PAGE_SIZE)
#define READ_BLOCKSIZE      1024
#define WRITE_BLOCKSIZE     2048
#define DICTIONARY_WINDOW  16384
#define WORDSIZE           (sizeof(int))
#define BITS_PER_WORD         32
#define WRITE_BLOCKS       ((DICTIONARY_WINDOW/WRITE_BLOCKSIZE)+1)
#define WRITE_BLOCK_WORDS  (WRITE_BLOCKSIZE/WORDSIZE)

char flash_region_base[FLASH_SIZE] ICACHE_FLASH_RESERVED_ATTR;

struct INPUT {
  int      fd;
  int      len;
  uint8_t  block[READ_BLOCKSIZE];
  uint8_t *inPtr;
  int      bytesRead;
  int      left;
  void    *inflate_state;
} *in;

typedef struct {
  uint8_t byte[WRITE_BLOCKSIZE];
} outBlock;

struct OUTPUT {
  lua_State *L;
  lu_int32  flash_sig;
  int       len;
  outBlock *block[WRITE_BLOCKS];
  outBlock  buffer;
  int       ndx;
  uint32_t  crc;
  int     (*fullBlkCB) (void);
  int       flashLen;
  int       flagsLen;
  int       flagsNdx;
  uint32_t *flags;
  const char *error;
} *out;

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

/* =====================================================================================
 * luaN_init(), luaN_reload_reboot() and luaN_index() are exported via lflash.h.
 * The first is the startup hook used in lstate.c and the last two are
 * implementations of the node.flash API calls.
 */

/*
 * Hook in lstate.c:f_luaopen() to set up ROstrt and ROpvmain if needed
 */
LUAI_FUNC void luaN_init (lua_State *L) {
  curOffset       = 0;
  flashAddr       = flash_region_base;
  flashAddrPhys   = platform_flash_mapped2phys((uint32_t)flashAddr);
  flashSector     = platform_flash_get_sector_of_address(flashAddrPhys);
  FlashHeader *fh = cast(FlashHeader *, flashAddr);

  /*
   * For the LFS to be valid, its signature has to be correct for this build
   * variant, the ROhash and main proto fields must be defined and the main proto
   * address be within the LFS address bounds. (This last check is primarily to
   * detect the direct imaging of an absolute LFS with the wrong base address.
   */

  if (fh->flash_sig == 0 || fh->flash_sig == ~0 ) {
    NODE_ERR("No LFS image loaded\n");
    return;
  }

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

//extern void software_reset(void);
static int loadLFS (lua_State *L);
static int loadLFSgc (lua_State *L);
static int procFirstPass (void);

/*
 * Library function called by node.flashreload(filename).
 */
LUALIB_API int luaN_reload_reboot (lua_State *L) {
  // luaL_dbgbreak();
  const char *fn = lua_tostring(L, 1), *msg = "";
  int status;
 /*
  * Do a protected call of loadLFS.
  *
  * -  This will normally rewrite the LFS and reboot, with no return.
  * -  If an error occurs then it is sent to the UART.
  * -  If this occured in the 1st pass, the previous LFS is unchanged so it is
  *    safe to return to the calling Lua.
  * -  If in the 1st pass, then the ESP is rebooted.
  */
  status = lua_cpcall(L, &loadLFS, cast(void *,fn));

  if (!out || out->fullBlkCB == procFirstPass) {
   /*
    * Never entered the 2nd pass, so it is safe to return the error.  Note
    * that I've gone to some trouble to ensure that all dynamically allocated
    * working areas have been freed, so that we have no memory leaks.
    */
    if (status == LUA_ERRMEM)
      msg = "Memory allocation error";
    else if (out && out->error)
      msg = out->error;
    else
      msg = "Unknown Error";

   /* We can clean up and return error */
    lua_cpcall(L, &loadLFSgc, NULL);
    lua_settop(L, 0);
    lua_pushstring(L, msg);
    return 1;
  }

  if (status == 0) {
    /* Successful LFS rewrite */
    msg = "LFS region updated.  Restarting.";
  } else {
    /* We have errored during the second pass so clear the LFS and reboot */
    if (status == LUA_ERRMEM)
      msg = "Memory allocation error";
    else if (out->error)
      msg = out->error;
    else
      msg = "Unknown Error";

    flashErase(0,-1);
  }
  NODE_ERR(msg);

  while (1) {}  // Force WDT as the ROM software_reset() doesn't seem to work
  return 0;
}


/*
 * If the arg is a valid LFS module name then return the LClosure
 * pointing to it. Otherwise return:
 *  -  The Unix time that the LFS was built
 *  -  The base address and length of the LFS
 *  -  An array of the module names in the LFS
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
/* =====================================================================================
 * The following routines use my uzlib which was based on pfalcon's inflate and
 * deflate routines.  The standard NodeMCU make also makes two host tools uz_zip
 * and uz_unzip which also use these and luac.cross uses the deflate. As discussed
 * below, The main action routine loadLFS() calls uzlib_inflate() to do the actual
 * stream inflation but uses three supplied CBs to abstract input and output
 * stream handling.
 *
 * ESP8266 RAM limitations and heap fragmentation are a key implementation
 * constraint and hence these routines use a number of ~2K buffers (11) as
 * working storage.
 *
 * The inflate is done twice, in order to limit storage use and avoid forward /
 * backward reference issues.  However this has a major advantage that the LFS
 * is scanned with the headers, CRC, etc. validated BEFORE the write to flash
 * is started, so the only real chance of failure during the second pass
 * write is if a power fail occurs during the pass.
 */

static void flash_error(const char *err) {
  if (out)
    out->error = err;
  if (in && in->inflate_state)
    uz_free(in->inflate_state);
  lua_pushnil(out->L);   /* can't use it on a cpcall anyway */
  lua_error(out->L);
}

/*
 * uzlib_inflate does a stream inflate on an RFC 1951 encoded data stream.
 * It uses three application-specific CBs passed in the call to do the work:
 *
 * -  get_byte()     CB to return next byte in input stream
 * -  put_byte()     CB to output byte to output buffer
 * -  recall_byte()  CB to output byte to retrieve a historic byte from
 *                   the output buffer.
 *
 *  Note that put_byte() also triggers secondary CBs to do further processing.
 */
static uint8_t get_byte (void) {
  if (--in->left < 0) {
    /* Read next input block */
    int remaining = in->len - in->bytesRead;
    int wanted    = remaining >= READ_BLOCKSIZE ? READ_BLOCKSIZE : remaining;

    if (vfs_read(in->fd, in->block, wanted) != wanted)
      flash_error("read error on LFS image file");

    system_soft_wdt_feed();

    in->bytesRead += wanted;
    in->inPtr      = in->block;
    in->left       = wanted-1;
  }
  return *in->inPtr++;
}


static void put_byte (uint8_t value) {
  int offset = out->ndx % WRITE_BLOCKSIZE;  /* counts from 0 */

  out->block[0]->byte[offset++] = value;
  out->ndx++;

  if (offset == WRITE_BLOCKSIZE || out->ndx == out->len) {
    if (out->fullBlkCB)
      out->fullBlkCB();
    /* circular shift the block pointers (redundant on last block, but so what) */
    outBlock *nextBlock  = out->block[WRITE_BLOCKS - 1];
    memmove(out->block+1, out->block, (WRITE_BLOCKS-1)*sizeof(void*));
    out->block[0] = nextBlock ;
  }
}


static uint8_t recall_byte (uint offset) {
  if(offset > DICTIONARY_WINDOW || offset >= out->ndx)
    flash_error("invalid dictionary offset on inflate");
  /* ndx starts at 1. Need relative to 0 */
  uint n   = out->ndx - offset;
  uint pos = n % WRITE_BLOCKSIZE;
  uint blockNo = out->ndx / WRITE_BLOCKSIZE - n  / WRITE_BLOCKSIZE;
  return out->block[blockNo]->byte[pos];
}

/*
 * On the first pass the break index is set to call this process at the end
 * of each completed output buffer.
 *  -  On the first call, the Flash Header is checked.
 *  -  On each call the CRC is rolled up for that buffer.
 *  -  Once the flags array is in-buffer this is also captured.
 * This logic is slightly complicated by the last buffer is typically short.
 */
int procFirstPass (void) {
  int len = (out->ndx % WRITE_BLOCKSIZE) ?
               out->ndx % WRITE_BLOCKSIZE : WRITE_BLOCKSIZE;
  if (out->ndx <= WRITE_BLOCKSIZE) {
    uint32_t fl;
    /* Process the flash header and cache the FlashHeader fields we need */
    FlashHeader *fh = cast(FlashHeader *, out->block[0]);
    out->flashLen   = fh->flash_size;                         /* in bytes */
    out->flagsLen   = (out->len-fh->flash_size)/WORDSIZE;     /* in words */
    out->flash_sig  = fh->flash_sig;

    if ((fh->flash_sig & FLASH_FORMAT_MASK) != FLASH_FORMAT_VERSION)
      flash_error("Incorrect LFS header version");
    if ((fh->flash_sig & FLASH_SIG_B2_MASK) != FLASH_SIG_B2)
      flash_error("Incorrect LFS build type");
    if ((fh->flash_sig & ~FLASH_SIG_ABSOLUTE) != FLASH_SIG)
      flash_error("incorrect LFS header signature");
    if (fh->flash_size > FLASH_SIZE)
      flash_error("LFS Image too big for configured LFS region");
    if ((fh->flash_size & 0x3) ||
         fh->flash_size > FLASH_SIZE ||
         out->flagsLen != 1 + (out->flashLen/WORDSIZE - 1) / BITS_PER_WORD)
      flash_error("LFS length mismatch");
    out->flags = luaM_newvector(out->L, out->flagsLen, uint);
  }

  /* update running CRC */
  out->crc = uzlib_crc32(out->block[0], len, out->crc);

  /* copy out any flag vector */
  if (out->ndx > out->flashLen) {
    int start = out->flashLen - (out->ndx - len);
    if (start < 0) start = 0;
    memcpy(out->flags + out->flagsNdx, out->block[0]->byte + start, len - start);
    out->flagsNdx += (len -start) / WORDSIZE;  /* flashLen and len are word aligned */
  }

  return 1;
}


int procSecondPass (void) {
 /*
  * The length rules are different for the second pass since this only processes
  * upto the flashLen and not the full image.  This also works in word units.
  * (We've already validated these are word multiples.)
  */
  int i, len = (out->ndx > out->flashLen) ?
                  (out->flashLen % WRITE_BLOCKSIZE) / WORDSIZE :
                  WRITE_BLOCKSIZE / WORDSIZE;
  uint32_t *buf = (uint32_t *) out->buffer.byte, flags;
 /*
  * Relocate all the addresses tagged in out->flags.  This can't be done in
  * place because the out->blocks are still in use as dictionary content so
  * first copy the block to a working buffer and do the relocation in this.
  */
  memcpy(out->buffer.byte, out->block[0]->byte, WRITE_BLOCKSIZE);
  for (i=0; i<len; i++,flags>>=1 ) {
    if ((i&31)==0)
      flags = out->flags[out->flagsNdx++];
    if (flags&1)
      buf[i] = WORDSIZE*buf[i] + cast(uint32_t, flashAddr);
  }
 /*
  * On first block, set the flash_sig has the in progress bit set and this
  * is not cleared until end.
  */
  if (out->ndx <= WRITE_BLOCKSIZE)
    buf[0] = out->flash_sig | FLASH_SIG_IN_PROGRESS;

  flashBlock(buf, len*WORDSIZE);

  if (out->ndx >= out->flashLen) {
    /* we're done so disable CB and rewrite flash sig to complete flash */
    flashSetPosition(0);
    flashBlock(&out->flash_sig, WORDSIZE);
    out->fullBlkCB = NULL;
  }
}

/*
 * loadLFS)() is protected called from luaN_reload_reboot so that it can recover
 * from out of memory and other thrown errors.  loadLFSgc() GCs any resources.
 */
static int loadLFS (lua_State *L) {
  const char *fn = cast(const char *, lua_touserdata(L, 1));
  int i, n, res;
  uint32_t crc;

  /* Allocate and zero in and out structures */

  in = NULL; out = NULL;
  in  = luaM_new(L, struct INPUT);
  memset(in, 0, sizeof(*in));
  out = luaM_new(L, struct OUTPUT);
  memset(out, 0, sizeof(*out));
  out->L         = L;
  out->fullBlkCB = procFirstPass;
  out->crc       = ~0;

  /* Open LFS image/ file, read unpacked length from last 4 byte and rewind */
  if (!(in->fd = vfs_open(fn, "r")))
    flash_error("LFS image file not found");
  in->len = vfs_size(in->fd);
  if (in->len <= 200 ||        /* size of an empty luac output */
      vfs_lseek(in->fd, in->len-4, VFS_SEEK_SET) != in->len-4 ||
      vfs_read(in->fd, &out->len, sizeof(uint)) != sizeof(uint))
    flash_error("read error on LFS image file");
  vfs_lseek(in->fd, 0, VFS_SEEK_SET);

  /* Allocate the out buffers */
  for(i = 0;  i <= WRITE_BLOCKS; i++)
    out->block[i] = luaM_new(L, outBlock);

  /* first inflate pass */
  if (uzlib_inflate (get_byte, put_byte, recall_byte,
                     in->len, &crc, &in->inflate_state) < 0)
    flash_error("read error on LFS image file");

  if (crc != ~out->crc)
    flash_error("checksum error on LFS image file");

  out->fullBlkCB = procSecondPass;
  out->flagsNdx  = 0;
  out->ndx       = 0;
  in->bytesRead  = in->left = 0;
 /*
  * Once we have completed the 1st pass then the LFS image has passed the
  * basic signature, crc and length checks, so now we can reset the counts
  * to do the actual write to flash on the second pass.
  */
  vfs_lseek(in->fd, 0, VFS_SEEK_SET);
  flashErase(0,(out->flashLen - 1)/FLASH_PAGE_SIZE);
  flashSetPosition(0);

  if (uzlib_inflate(get_byte, put_byte, recall_byte,
                    in->len, &crc, &in->inflate_state) != UZLIB_OK)
  if (res < 0) {
    const char *err[] = {"Data_error during decompression",
                         "Chksum_error during decompression",
                         "Dictionary error during decompression"
                         "Memory_error during decompression"};
    flash_error(err[UZLIB_DATA_ERROR - res]);
  }
  return 0;
}


static int loadLFSgc (lua_State *L) {
  int i;
  if (out) {
    for (i = 0; i < WRITE_BLOCKS; i++)
      if (out->block[i])
        luaM_free(L, out->block[i]);
    if (out->flags)
      luaM_freearray(L, out->flags, out->flagsLen, uint32_t);
    luaM_free(L, out);
  }
  if (in) {
    if (in->fd)
      vfs_close(in->fd);
    luaM_free(L, in);
  }
  return 0;
}
#endif
