/***--
** lflashimg.c
** Dump a compiled Proto hiearchy to a RO (FLash) image file
** See Copyright Notice in lua.h
*/

#define LUAC_CROSS_FILE

#include "luac_cross.h"
#include C_HEADER_CTYPE
#include C_HEADER_STDIO
#include C_HEADER_STDLIB
#include C_HEADER_STRING

#define lflashimg_c
#define LUA_CORE
#include "lobject.h"
#include "lstring.h"
#undef LUA_FLASH_STORE
#define LUA_FLASH_STORE
#include "lflash.h"
#include "uzlib.h"

//#define LOCAL_DEBUG

#if INT_MAX != 2147483647
# error "luac.cross requires C toolchain with 4 byte word size"
#endif
#define WORDSIZE ((int) sizeof(int))
#define ALIGN(s) (((s)+(WORDSIZE-1)) & (-(signed) WORDSIZE))
#define WORDSHIFT 2
typedef unsigned int uint;
#define FLASH_WORDS(t) (sizeof(t)/sizeof(FlashAddr))
/*
 *
 * This dumper is a variant of the standard ldump, in that instead of producing a
 * binary loader format that lundump can load, it produces an image file that can be
 * directly mapped or copied into addressable memory. The typical application is on
 * small memory IoT devices which support programmable flash storage such as the
 * ESP8266. A 64 Kb LFS image has 16Kb words and will enable all program-related
 * storage to be accessed directly from flash, leaving the RAM for true R/W
 * application data.
 *
 * The start address of the Lua Flash Store (LFS) is build-dependent, and the cross
 * compiler '-a' option allows the developer to fix the LFS at a defined flash memory
 * address.  Alternatively and by default the cross compilation adopts a position
 * independent image format, which permits the  on-device image loader to load the LFS
 * image at an appropriate base within the flash address space. As all objects in the
 * LFS can be treated as multiples of 4-byte words, also all address fields are both
 * word aligned, and any address references within the LFS are also word-aligned.
 *
 * This version adds gzip compression of the generated LFS image for more efficient
 * over-the-air (OTA) transfer, so the method of tagging address words has been
 * replaced by a scheme which achieves better compression: an additional bitmap
 * has been added to the image, with each bit corresponding to a word in the image
 * and set if the corresponding work is an address.  The addresses are stored as
 * signed relative word offsets.
 *
 * The unloader is documented in lflash.c  Note that his relocation process is
 * skipped for absolute addressed images (which are identified by the
 * FLASH_SIG_ABSOLUTE bit setting in the flash signature).
 *
 * The flash image has a standard header detailed in lflash.h
 *
 * Note that luac.cross may be compiled on any little-endian machine with 32 or 64 bit
 * word length so Flash addresses can't be handled as standard C pointers as size_t
 * and int may not have the same size. Hence addresses with the must be declared as
 * the FlashAddr type rather than typed C pointers and must be accessed through macros.
 *
 * Also note that image built with a given LUA_PACK_TVALUES / LUA_NUNBER_INTEGRAL
 * combination must be loaded into a corresponding firmware build.  Hence these
 * configuration options are also included in the FLash Signature.
 *
 * The Flash image is assembled up by first building the RO stringtable containing
 * all strings used in the compiled proto hierarchy.  This is followed by the Protos.
 *
 * The storage is allocated bottom up using a serial allocator and the algortihm for
 * building the image essentially does a bottom-uo serial enumeration so that any
 * referenced storage has already been allocated in the image, and therefore (with the
 * exception of the Flash Header) all pointer references are backwards.
 *
 * As addresses are 4 byte on the target and either 4 or (typically) 8 bytes on the
 * host so any structures containing address fields (TStrings, TValues, Protos, other
 * address vectors) need repacking.
 */

typedef struct flashts {       /* This is the fixed 32-bit equivalent of TString */
  FlashAddr next;
  lu_byte   tt;
  lu_byte   marked;
  int       hash;
  int       len;
} FlashTS;

#ifndef LUA_MAX_FLASH_SIZE
#define LUA_MAX_FLASH_SIZE  0x10000  //in words
#endif

static uint curOffset = 0;

/*
 * The flashAddrTag is a bit array, one bit per flashImage word denoting
 * whether the corresponding word is a relative address.  The defines
 * are access methods for this bit array.
 */
static uint flashImage[LUA_MAX_FLASH_SIZE + LUA_MAX_FLASH_SIZE/32];
static uint *flashAddrTag = flashImage + LUA_MAX_FLASH_SIZE;

#define _TW(v) (v)>>5
#define _TB(v) (1<<((v)&0x1F))
#define setFlashAddrTag(v) flashAddrTag[_TW(v)] |= _TB(v)
#define getFlashAddrTag(v) ((flashAddrTag[_TW(v)]&_TB(v)) != 0)

#define fatal luac_fatal
#ifdef _MSC_VER
extern void __declspec( noreturn ) luac_fatal( const char* message );
#else
extern void __attribute__((noreturn)) luac_fatal(const char* message);
#endif

#ifdef LOCAL_DEBUG
#define DBG_PRINT(...) printf(__VA_ARGS__)
#else
#define DBG_PRINT(...) ((void)0)
#endif

/*
 *  Serial allocator.  Throw a luac-style out of memory error is allocaiton fails.
 */
static void *flashAlloc(lua_State* L, size_t n) {
  void *p = (void *)(flashImage + curOffset);
  curOffset += ALIGN(n)>>WORDSHIFT;
  if (curOffset > LUA_MAX_FLASH_SIZE) {
    fatal("Out of Flash memory");
  }
  return p;
}

/*
 * Convert an absolute address pointing inside the flash image to offset form.
 * This macro form also takes the lvalue destination so that this can be tagged
 * as a relocatable address.
 */
#define toFlashAddr(l, pd, s) _toFlashAddr(l, &(pd), s)
static void _toFlashAddr(lua_State* L, FlashAddr *a, void *p) {
  uint doffset = cast(char *, a) - cast(char *,flashImage);
  lua_assert(!(doffset & (WORDSIZE-1)));  // check word aligned
  doffset >>= WORDSHIFT;                  // and convert to a word offset
  lua_assert(doffset <= curOffset);
  if (p) {
    uint poffset = cast(char *, p) - cast(char *,flashImage);
    lua_assert(!(poffset & (WORDSIZE-1)));
    poffset >>= WORDSHIFT;
    lua_assert(poffset <= curOffset);
    flashImage[doffset] = poffset;     // Set the pointer to the offset
    setFlashAddrTag(doffset);          // And tag as an address
  } /* else leave clear */             // Special case for NULL pointer
}

/*
 * Convert an image address in offset form back to (host) absolute form
 */
static void *fromFashAddr(FlashAddr a) {
  return a ? cast(void *, flashImage + a) : NULL;
}


/*
 * Add a TS found in the Proto Load to the table at the ToS
 */
static void addTS(lua_State *L, TString *ts) {
  lua_assert(ts->tsv.tt==LUA_TSTRING);
  lua_pushnil(L);
  setsvalue(L, L->top-1, ts);
  lua_pushinteger(L, 1);
  lua_rawset(L, -3);
  DBG_PRINT("Adding string: %s\n",getstr(ts));
}


/*
 * Enumerate all of the Protos in the Proto hiearchy and scan contents to collect
 * all referenced strings in a Lua Array at ToS.
 */
static void scanProtoStrings(lua_State *L, const Proto* f) {
  /* Table at L->Top[-1] is used to collect the strings */
  int i;

  if (f->source)
    addTS(L, f->source);

#ifdef LUA_OPTIMIZE_DEBUG
  if (f->packedlineinfo)
    addTS(L, luaS_new(L, cast(const char *, f->packedlineinfo)));
#endif

  for (i = 0; i < f->sizek; i++) {
    if (ttisstring(f->k + i))
      addTS(L, rawtsvalue(f->k + i));
  }
  for (i = 0; i < f->sizeupvalues; i++) addTS(L, f->upvalues[i]);
  for (i = 0; i < f->sizelocvars; i++)  addTS(L, f->locvars[i].varname);
  for (i = 0; i < f->sizep; i++)        scanProtoStrings(L, f->p[i]);
}


/*
 * Use the collected strings table to build the new ROstrt in the Flash Image
 *
 * The input is an array of {"SomeString" = 1, ...} on the ToS.
 * The output is an array of {"SomeString" = FlashOffset("SomeString"), ...} on ToS
 */
static void createROstrt(lua_State *L, FlashHeader *fh) {

  /* Table at L->Top[-1] on input is hash used to collect the strings */
  /* Count the number of strings.  Can't use objlen as this is a hash */
  fh->nROuse  = 0;
  lua_pushnil(L);  /* first key */
  while (lua_next(L, -2) != 0) {
    fh->nROuse++;
    DBG_PRINT("Found: %s\n",getstr(rawtsvalue(L->top-2)));
    lua_pop(L, 1);                           // dump the value
  }
  fh->nROsize = 2<<luaO_log2(fh->nROuse);
  FlashAddr *hashTab = flashAlloc(L, fh->nROsize * WORDSIZE);
  toFlashAddr(L, fh->pROhash, hashTab);

  /* Now iterate over the strings to be added to the RO string table and build it */
  lua_newtable(L);                           // add output table
  lua_pushnil(L);                            // First key
  while (lua_next(L, -3) != 0) {             // replaces key, pushes value
    TString *ts   = rawtsvalue(L->top - 2);  // key.ts
    const char *p = getstr(ts);              // C string of key
    uint hash     = ts->tsv.hash;            // hash of key
    size_t len  = ts->tsv.len;               // and length

    DBG_PRINT("2nd pass: %s\n",p);

    FlashAddr *e  = hashTab + lmod(hash, fh->nROsize);
    FlashTS *last = cast(FlashTS *, fromFashAddr(*e));
    FlashTS *fts  = cast(FlashTS *, flashAlloc(L, sizeof(FlashTS)));
    toFlashAddr(L, *e, fts);                 // add reference to TS to lookup vector
    toFlashAddr(L, fts->next, last);         // and chain to previous entry if any
    fts->tt     = LUA_TSTRING;               // Set as String
    fts->marked = bitmask(LFSBIT);           // LFS string with no Whitebits set
    fts->hash   = hash;                      // add hash
    fts->len    = len;                       // and length
    memcpy(flashAlloc(L, len+1), p, len+1);  // copy string
                                             // include the trailing null char
    lua_pop(L, 1);                           // Junk the value
    lua_pushvalue(L, -1);                    // Dup the key as rawset dumps its copy
    lua_pushinteger(L, cast(FlashAddr*,fts)-flashImage);  // Value is new TS offset.
    lua_rawset(L, -4);                       // Add to new table
  }
  /* At this point the old hash is done to derefence for GC */
  lua_remove(L, -2);
}

/*
 * Convert a TString reference in the host G(L)->strt entry into the corresponding
 * TString address in the flashImage using the lookup table at ToS
 */
static void *resolveTString(lua_State* L, TString *s) {
  if (!s)
    return NULL;
  lua_pushnil(L);
  setsvalue(L, L->top-1, s);
  lua_rawget(L, -2);
  lua_assert(!lua_isnil(L, -1));
  void *ts = fromFashAddr(lua_tointeger(L, -1));
  lua_pop(L, 1);
  return ts;
}

/*
 * In order to simplify repacking of structures from the host format to that target
 * format, this simple copy routine is data-driven by a simple format specifier.
 *   n       Number of consecutive records to be processed
 *   fmt     A string of A, I, S, V specifiers spanning the record.
 *   src     Source of record
 *   returns Address of destination record
 */
#if defined(LUA_PACK_TVALUES)
#define TARGET_TV_SIZE (sizeof(lua_Number)+sizeof(lu_int32))
#else
#define TARGET_TV_SIZE (2*sizeof(lua_Number))
#endif

static void *flashCopy(lua_State* L, int n, const char *fmt, void *src) {
  /* ToS is the string address mapping table */
  if (n == 0)
    return NULL;
  int i, recsize;
  void *newts;
  /* A bit of a botch because fmt is either "V" or a string of WORDSIZE specifiers */
  /* The size 8 / 12 / 16 bytes for integer builds, packed TV and default TVs resp */

  if (fmt[0]=='V') {
    lua_assert(fmt[1] == 0);   /* V formats must be singetons */
    recsize = TARGET_TV_SIZE;
  } else {
    recsize = WORDSIZE * strlen(fmt);
  }

  uint *d  = cast(uint *, flashAlloc(L, n * recsize));
  uint *dest = d;
  uint *s  = cast(uint *, src);

  for (i = 0; i < n; i++) {
    const char *p = fmt;
    while (*p) {
      /* All input address types (A,S,V) are aligned to size_t boundaries */
      if (*p != 'I' && ((size_t)s)&(sizeof(size_t)-1))
        s++;

      switch (*p++) {
        case 'A':
          toFlashAddr(L, *d, *cast(void**, s));
          s += FLASH_WORDS(size_t);
          d++;
          break;
        case 'I':
          *d++ = *s++;
          break;
        case 'H':
          *d++ = (*s++) & 0;
          break;
        case 'S':
          newts = resolveTString(L, *cast(TString **, s));
          toFlashAddr(L, *d, newts);
          s += FLASH_WORDS(size_t);
          d++;
          break;
        case 'V':
          /* This code has to work for both Integer and Float build variants */
          memset(d, 0, TARGET_TV_SIZE);
          TValue *sv = cast(TValue *, s);
          /* The value is 0, 4 or 8 bytes depending on type */
          if (ttisstring(sv)) {
            toFlashAddr(L, *d, resolveTString(L, rawtsvalue(sv)));
          } else if (ttisnumber(sv)) {
            *cast(lua_Number*,d) = *cast(lua_Number*,s);
          } else if (!ttisnil(sv)){
            /* all other types are 4 byte */
            lua_assert(!iscollectable(sv));
            *cast(uint *,d) = *cast(uint *,s);
          }
          *cast(int *,cast(lua_Number*,d)+1) = ttype(sv);
          s += FLASH_WORDS(TValue);
          d += TARGET_TV_SIZE/WORDSIZE;
         break;
        default:
          lua_assert (0);
      }
    }
  }
  return dest;
}

/* The debug optimised version has a different Proto layout */
#ifdef LUA_OPTIMIZE_DEBUG
#define PROTO_COPY_MASK  "AHAAAAAASIIIIIIIAI"
#else
#define PROTO_COPY_MASK  "AHAAAAAASIIIIIIIIAI"
#endif

/*
 * Do the actual prototype copy.
 */
static void *functionToFlash(lua_State* L, const Proto* orig) {
  Proto f;
  int i;

  memcpy (&f, orig, sizeof(Proto));
  f.gclist = NULL;
  f.next = NULL;
  l_setbit(f.marked, LFSBIT);   /* OK to set the LFSBIT on a stack-cloned copy */

  if (f.sizep) {                /* clone included Protos */
    Proto **p = luaM_newvector(L, f.sizep, Proto *);
    for (i=0; i<f.sizep; i++)
      p[i] = cast(Proto *, functionToFlash(L, f.p[i]));
    f.p = cast(Proto **, flashCopy(L, f.sizep, "A", p));
    luaM_freearray(L, p, f.sizep, Proto *);
  }
  f.k = cast(TValue *, flashCopy(L, f.sizek, "V", f.k));
  f.code = cast(Instruction *, flashCopy(L, f.sizecode, "I", f.code));

#ifdef LUA_OPTIMIZE_DEBUG
  if (f.packedlineinfo) {
    TString *ts=luaS_new(L, cast(const char *,f.packedlineinfo));
    f.packedlineinfo = cast(unsigned char *, resolveTString(L, ts)) + sizeof (FlashTS);
  }
#else
  f.lineinfo = cast(int *, flashCopy(L, f.sizelineinfo, "I", f.lineinfo));
#endif
  f.locvars = cast(struct LocVar *, flashCopy(L, f.sizelocvars, "SII", f.locvars));
  f.upvalues = cast(TString **, flashCopy(L, f.sizeupvalues, "S", f.upvalues));
  return cast(void *, flashCopy(L, 1, PROTO_COPY_MASK, &f));
}

uint dumpToFlashImage (lua_State* L, const Proto *main, lua_Writer w,
                       void* data, int strip,
                       lu_int32 address, lu_int32 maxSize) {
// parameter strip is ignored for now
  FlashHeader *fh = cast(FlashHeader *, flashAlloc(L, sizeof(FlashHeader)));
  int i, status;
  lua_newtable(L);
  scanProtoStrings(L, main);
  createROstrt(L,  fh);
  toFlashAddr(L, fh->mainProto, functionToFlash(L, main));

  fh->flash_sig = FLASH_SIG + (address ? FLASH_SIG_ABSOLUTE : 0);
  fh->flash_size = curOffset*WORDSIZE;
  if (fh->flash_size>maxSize) {
    fatal ("The image is too large for specfied LFS size");
  }
  if (address) {  /* in absolute mode convert addresses to mapped address */
    for (i = 0 ; i < curOffset; i++)
      if (getFlashAddrTag(i))
        flashImage[i] = 4*flashImage[i] + address;
    lua_unlock(L);
    status = w(L, flashImage, fh->flash_size, data);
  } else { /* compressed PI mode */
   /*
    * In image mode, shift the relocation bitmap down directly above
    * the used flashimage.  This consolidated array is then gzipped.
    */
    uint oLen;
    uint8_t *oBuf;

    int bmLen = sizeof(uint)*((curOffset+31)/32);      /* 32 flags to a word */
    memmove(flashImage+curOffset, flashAddrTag, bmLen);
    status = uzlib_compress (&oBuf, &oLen,
                             (const uint8_t *)flashImage, bmLen+fh->flash_size);
    if (status != UZLIB_OK) {
      luac_fatal("Out of memory during image compression");
    }
    lua_unlock(L);
 #if 0
    status = w(L, flashImage, bmLen+fh->flash_size, data);
 #else
    status = w(L, oBuf, oLen, data);
    free(oBuf);
 #endif

  }
  lua_lock(L);
  return status;
}
