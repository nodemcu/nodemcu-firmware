/*
 * This implementation draws heavily on the work down by Paul Sokolovsky
 * (https://github.com/pfalcon) and his uzlib library which in turn uses
 * work done by Joergen Ibsen, Simon Tatham and others.  All of this work
 * is under an unrestricted right to use subject to copyright attribution.
 * Two copyright wordings (variants A and B) are following.
 *
 * (c) statement A initTables, copy, literal
 *
 * The remainder of this code has been written by me, Terry Ellison 2018,
 * under the standard NodeMCU MIT licence, but is available to the other
 * contributors to this source under any permissive licence.
 *
 * My primary algorthmic reference is RFC 1951: "DEFLATE Compressed Data
 * Format Specification version 1.3", dated May 1996.
 *
 * Also because the code in this module is drawn from different sources,
 * the different coding practices can be confusing, I have standardised
 * the source by:
 *
 * -  Adopting the 2 indent rule as in the rest of the firmware
 *
 * -  I have replaced the various mix of char, unsigned char and uchar
 *    by the single uchar type; ditto for ushort and uint.
 *
 * -  All internal (non-exported) functions and data are static
 *
 * -  Only exported functions and data have the module prefix.  All
 *    internal (static) variables and fields are lowerCamalCase.
 *
 ***********************************************************************
 * Copyright statement A for Zlib (RFC1950 / RFC1951) compression for PuTTY.

PuTTY is copyright 1997-2014 Simon Tatham.

Portions copyright Robert de Bath, Joris van Rantwijk, Delian
Delchev, Andreas Schultz, Jeroen Massar, Wez Furlong, Nicolas Barry,
Justin Bradford, Ben Harris, Malcolm Smith, Ahmad Khalifa, Markus
Kuhn, Colin Watson, and CORE SDI S.A.

Permission is hereby granted, free of charge, to any person
obtaining a copy of this software and associated documentation files
(the "Software"), to deal in the Software without restriction,
including without limitation the rights to use, copy, modify, merge,
publish, distribute, sublicense, and/or sell copies of the Software,
and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT.  IN NO EVENT SHALL THE COP--YRIGHT HOLDERS BE LIABLE
FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

************************************************************************
Copyright statement B for genlz77 functions:
 *
 * genlz77  -  Generic LZ77 compressor
 *
 * Copyright (c) 2014 by Paul Sokolovsky
 *
 * This software is provided 'as-is', without any express
 * or implied warranty.  In no event will the authors be
 * held liable for any damages arising from the use of
 * this software.
 *
 * Permission is granted to anyone to use this software
 * for any purpose, including commercial applications,
 * and to alter it and redistribute it freely, subject to
 * the following restrictions:
 *
 * 1. The origin of this software must not be
 *    misrepresented; you must not claim that you
 *    wrote the original software. If you use this
 *    software in a product, an acknowledgment in
 *    the product documentation would be appreciated
 *    but is not required.
 *
 * 2. Altered source versions must be plainly marked
 *    as such, and must not be misrepresented as
 *    being the original software.
 *
 * 3. This notice may not be removed or altered from
 *    any source distribution.
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include "uzlib.h"

jmp_buf unwindAddr;

/* Minimum and maximum length of matches to look for, inclusive */
#define MIN_MATCH      3
#define MAX_MATCH      258
/* Max offset of the match to look for, inclusive */
#define MAX_OFFSET     16384 // 32768  //
#define OFFSET16_MASK  0x7FFF
#define NULL_OFFSET    0xFFFF
#if MIN_MATCH < 3
#error "Encoding requires a minium match of 3 bytes"
#endif

#define SIZE(a) (sizeof(a)/sizeof(*a)) /* no of elements in array */
#ifdef __XTENSA__
#define RAM_COPY_BYTE_ARRAY(c,s,sl)  uchar *c = alloca(sl); memcpy(c,s,(sl))
#else
#define RAM_COPY_BYTE_ARRAY(c,s,sl)  uchar *c = s;
#endif
#define FREE(v) if (v) uz_free(v)

typedef uint8_t  uchar;
typedef uint16_t ushort;
typedef uint32_t uint;

#ifdef DEBUG_COUNTS
#define DBG_PRINT(...) printf(__VA_ARGS__)
#define DBG_COUNT(n) (debugCounts[n]++)
#define DBG_ADD_COUNT(n,m) (debugCounts[n]+=m)
int debugCounts[20];
#else
#define DBG_PRINT(...)
#define DBG_COUNT(n)
#define DBG_ADD_COUNT(n,m)
#endif

int dbg_break(void) {return 1;}

typedef struct {
  ushort code, extraBits, min, max;
} codeRecord;

struct dynTables {
  ushort *hashChain;
  ushort *hashTable;
  ushort hashMask;
  ushort hashSlots;
  ushort hashBits;
  ushort dictLen;
  const uchar bitrevNibble[16];
  const codeRecord lenCodes[285-257+1];
  const codeRecord distCodes[29-0+1];
} *dynamicTables;

struct outputBuf {
  uchar *buffer;
  uint len, size;
  uint inLen, inNdx;
  uint bits, nBits;
  uint compDisabled;
} *oBuf;


/*
 * Set up the constant tables used to drive the compression
 *
 * Constants are stored in flash memory on the ESP8266 NodeMCU firmware
 * builds, but only word aligned data access are supported in hardare so
 * short and byte accesses are handled by a S/W exception handler and are
 * SLOW.  RAM is also at premium, so these short routines are driven by
 * byte vectors copied into RAM and then used to generate temporary RAM
 * tables, which are the same as the above statically declared versions.
 *
 * This might seem a bit convolved but this runs faster and takes up less
 * memory than the original version.  This code also works fine on the
 * x86-64s so we just use one code variant.
 *
 * Note that fixed Huffman trees as defined in RFC 1951 Sec 3.2.5 are
 * always used. Whilst dynamic trees can give better compression for
 * larger blocks, this comes at a performance hit of having to compute
 * these trees. Fixed trees give better compression performance on short
 * blocks and significantly reduce compression times.
 *
 * The following defines are used to initialise these tables.
 */
#define lenCodes_GEN \
  "\x03\x01\x01\x01\x01\x01\x01\x01\xff\x02\x02\x02\x02\xff\x04\x04\x04\x04" \
  "\xff\x08\x08\x08\x08\xff\x10\x10\x10\x10\xff\x20\x20\x20\x1f\xff\x01\x00"
#define lenCodes_LEN 29
#define distCodes_GEN \
  "\x01\x01\x01\x01\xff\x02\x02\xff\x04\x04\xff\x08\x08\xff\x10\x10\xff" \
  "\x20\x20\xff\x40\x40\xff\x86\x86\xff\x87\x87\xff\x88\x88\xff" \
  "\x89\x89\xff\x8a\x8a\xff\x8b\x8b\xff\x8c\x8c"
#define distCodes_LEN 30
#define BITREV16 "\x0\x8\x4\xc\x2\xa\x6\xe\x1\x9\x5\xd\x3\xb\x7\xf"

static void genCodeRecs (const codeRecord *rec, ushort len,
                    char *init, int initLen,
                    ushort start, ushort m0) {
  DBG_COUNT(0);
  int       i, b=0, m=0, last=m0;
  RAM_COPY_BYTE_ARRAY(c, (uchar *)init,initLen);
  codeRecord *p = (codeRecord *) rec;

  for (i = start; i < start+len; i++, c++) {
    if (*c == 0xFF)
      b++, c++;
    m += (*c & 0x80) ? 2 << (*c & 0x1F) : *c;
    *p++ = (codeRecord) {i, b, last + 1, (last = m)};
  }
}

static void initTables (uint chainLen, uint hashSlots) {
  DBG_COUNT(1);
  uint dynamicSize = sizeof(struct dynTables) +
                     sizeof(struct outputBuf) +
                     chainLen * sizeof(ushort) +
                     hashSlots * sizeof(ushort);
  struct dynTables *dt = uz_malloc(dynamicSize);
  memset(dt, 0, dynamicSize);
  dynamicTables = dt;

  /* Do a single malloc for dymanic tables and assign addresses */
  if(!dt )
    UZLIB_THROW(UZLIB_MEMORY_ERROR);

  memcpy((uchar*)dt->bitrevNibble, BITREV16, 16);
  oBuf          = (struct outputBuf *)(dt+1);
  dt->hashTable = (ushort *)(oBuf+1);
  dt->hashChain = dt->hashTable + hashSlots;
  dt->hashSlots = hashSlots;
  dt->hashMask = hashSlots - 1;

  /* As these are offset rather than pointer, 0 is a valid offset */
  /* (unlike NULL), so 0xFFFF is used to denote an unset value */
  memset(dt->hashTable, -1, sizeof(ushort)*hashSlots);
  memset(dt->hashChain, -1, sizeof(ushort)*chainLen);

  /* Generate the code recors for the lenth and distance code tables */
  genCodeRecs(dt->lenCodes, SIZE(dt->lenCodes),
              lenCodes_GEN, sizeof(lenCodes_GEN),
              257,2);
  ((codeRecord *)(dynamicTables->lenCodes+285-257))->extraBits=0;  /* odd ball entry */
  genCodeRecs(dt->distCodes, SIZE(dt->distCodes),
              distCodes_GEN, sizeof(distCodes_GEN),
              0,0);
}


/*
 * Routines to output bit streams and byte streams to the output buffer
 */
void resizeBuffer(void) {
  uchar *nb;
  DBG_COUNT(2);
  /* The outbuf is given an initial size estimate but if we are running */
  /* out of space then extropolate size using current compression */
  double newEstimate = (((double) oBuf->len)*oBuf->inLen) / oBuf->inNdx;
  oBuf->size = 128 + (uint) newEstimate;
  if (!(nb = realloc(oBuf->buffer, oBuf->size)))
    UZLIB_THROW(UZLIB_MEMORY_ERROR);
  oBuf->buffer = nb;
}

void outBits(ushort bits, int nBits) {
  DBG_COUNT(3);
  oBuf->bits  |= bits << oBuf->nBits;
  oBuf->nBits += nBits;

  if (oBuf->len >= oBuf->size - sizeof(bits))
    resizeBuffer();

  while (oBuf->nBits >= 8) {
    DBG_PRINT("%02x-", oBuf->bits & 0xFF);
    oBuf->buffer[oBuf->len++] = oBuf->bits & 0xFF;
    oBuf->bits >>= 8;
    oBuf->nBits -= 8;
  }
}

void outBitsRev(uchar bits, int nBits) {
  DBG_COUNT(4);
  /* Note that bit reversal only operates on an 8-bit bits field */
  uchar bitsRev = (dynamicTables->bitrevNibble[bits & 0x0f]<<4) |
                  dynamicTables->bitrevNibble[bits>>4];
  outBits(bitsRev, nBits);
}

void outBytes(void *bytes, int nBytes) {
  DBG_COUNT(5);
  int i;
  if (oBuf->len >= oBuf->size - nBytes)
    resizeBuffer();

  /* Note that byte output dumps any bits data so the caller must */
  /* flush this first, if necessary */
  oBuf->nBits = oBuf->bits  = 0;
  for (i = 0; i < nBytes; i++) {
    DBG_PRINT("%02x-", *((uchar*)bytes+i));
    oBuf->buffer[oBuf->len++] = *((uchar*)bytes+i);
  }
}

/*
 * Output an literal byte as an 8 or 9 bit code
 */
void literal (uchar c) {
  DBG_COUNT(6);
  DBG_PRINT("sym: %02x   %c\n", c, c);
  if (oBuf->compDisabled) {
    /* We're in an uncompressed block, so just output the byte. */
    outBits(c, 8);
  } else if (c <= 143) {
    /* 0 through 143 are 8 bits long starting at 00110000. */
    outBitsRev(0x30 + c, 8);
  } else {
    /* 144 through 255 are 9 bits long starting at 110010000. */
    outBits(1, 1);
    outBitsRev(0x90 - 144 + c, 8);
  }
}

/*
 * Output a dictionary (distance, length) pars as bitstream codes
 */
void copy (int distance, int len) {
  DBG_COUNT(7);
  const codeRecord *lenCodes  = dynamicTables->lenCodes, *l;
  const codeRecord *distCodes = dynamicTables->distCodes, *d;
  int i, j, k;

  assert(!oBuf->compDisabled);

  while (len > 0) {
   /*
    * We can transmit matches of lengths 3 through 258
    * inclusive. So if len exceeds 258, we must transmit in
    * several steps, with 258 or less in each step.
    *
    * Specifically: if len >= 261, we can transmit 258 and be
    * sure of having at least 3 left for the next step. And if
    * len <= 258, we can just transmit len. But if len == 259
    * or 260, we must transmit len-3.
    */
    int thislen = (len > 260 ? 258 : len <= 258 ? len : len - 3);
    len -= thislen;
    /*
     * Binary-search to find which length code we're
     * transmitting.
     */
    i = -1;
    j = lenCodes_LEN;
    while (1) {
      assert(j - i >= 2);
      k = (j + i) / 2;
      if (thislen < lenCodes[k].min)
        j = k;
      else if (thislen > lenCodes[k].max)
        i = k;
      else {
        l = &lenCodes[k];
        break;                 /* found it! */
      }
    }
    /*
     * Transmit the length code. 256-279 are seven bits
     * starting at 0000000; 280-287 are eight bits starting at
     * 11000000.
     */
    if (l->code <= 279) {
      outBitsRev((l->code - 256) * 2, 7);
    } else {
      outBitsRev(0xc0 - 280 + l->code, 8);
    }
    /*
     * Transmit the extra bits.
     */
    if (l->extraBits)
      outBits(thislen - l->min, l->extraBits);
    /*
     * Binary-search to find which distance code we're
     * transmitting.
     */
    i = -1;
    j = distCodes_LEN;
    while (1) {
      assert(j - i >= 2);
      k = (j + i) / 2;
      if (distance < distCodes[k].min)
        j = k;
      else if (distance > distCodes[k].max)
        i = k;
      else {
        d = &distCodes[k];
        break;                 /* found it! */
      }
    }

    /*
     * Transmit the distance code. Five bits starting at 00000.
     */
    outBitsRev(d->code * 8, 5);

    /*
     * Transmit the extra bits.
     */
    if (d->extraBits)
      outBits(distance - d->min, d->extraBits);
  }
}

/*
 * Block compression uses a hashTable to index into a set of search
 * chainList, where each chain links together the triples of chars within
 * the dictionary (the last MAX_OFFSET bytes of the input buffer) with
 * the same hash index. So for compressing a file of 200Kb, say, with a
 * 16K dictionary (the largest that we can inflate within the memory
 * constraints of the ESP8266), the chainList is 16K slots long, and the
 * hashTable is 4K slots long, so a typical chain will have 4 links.
 *
 * These two tables use 16-bit ushort offsets rather than pointers to
 * save memory (essential on the ESP8266).
 *
 * As per RFC 1951 sec 4, we also implement a "lazy match" procedure
 */

void uzlibCompressBlock(const uchar *src, uint srcLen) {
  int i, j, k, l;
  uint hashMask     = dynamicTables->hashMask;
  ushort *hashChain = dynamicTables->hashChain;
  ushort *hashTable = dynamicTables->hashTable;
  uint hashShift    = 24 - dynamicTables->hashBits;
  uint lastOffset   = 0, lastLen = 0;
  oBuf->inLen       = srcLen;          /* used for output buffer resizing */
  DBG_COUNT(9);

  for (i = 0; i <= ((int)srcLen) - MIN_MATCH; i++) {
   /*
    * Calculate a hash on the next three chars using the liblzf hash
    * function, then use this via the hashTable to index into the chain
    * of triples within the dictionary window which have the same hash.
    *
    * Note that using 16-bit offsets requires a little manipulation to
    * handle wrap-around and recover the correct offset, but all other
    * working uses uint offsets simply because the compiler generates
    * faster (and smaller in the case of the ESP8266) code.
    *
    * Also note that this code also works for any tail 2 literals; the
    * hash will access beyond the array and will be incorrect, but
    * these can't match and will flush the last cache.
    */
    const uchar *this = src + i, *comp;
    uint base        = i & ~OFFSET16_MASK;
    uint iOffset     = i - base;
    uint maxLen      = srcLen - i;
    uint matchLen    = MIN_MATCH - 1;
    uint matchOffset = 0;
    uint v          = (this[0] << 16) | (this[1] << 8) | this[2];
    uint hash       = ((v >> hashShift) - v) & hashMask;
    uint nextOffset = hashTable[hash];
    oBuf->inNdx = i;                   /* used for output buffer resizing */
    DBG_COUNT(10);

    if (maxLen>MAX_MATCH)
      maxLen = MAX_MATCH;

    hashTable[hash] = iOffset;
    hashChain[iOffset & (MAX_OFFSET-1)] = nextOffset;

    for (l = 0; nextOffset != NULL_OFFSET && l<60; l++) {
      DBG_COUNT(11);

      /* handle the case where base has bumped */
      j = base + nextOffset - ((nextOffset < iOffset) ? 0 : (OFFSET16_MASK + 1));

      if (i - j > MAX_OFFSET)
        break;

      for (k = 0, comp = src + j; this[k] == comp[k] && k < maxLen; k++)
        {}
      DBG_ADD_COUNT(12, k);

      if (k > matchLen) {
         matchOffset = i - j;
         matchLen = k;
      }
      nextOffset = hashChain[nextOffset & (MAX_OFFSET-1)];
    }

    if (lastOffset) {
      if (matchOffset == 0 || lastLen >= matchLen  ) {
        /* ignore this match (or not) and process last */
        DBG_COUNT(14);
        copy(lastOffset, lastLen);
        DBG_PRINT("dic: %6x %6x %6x\n", i-1, lastLen, lastOffset);
        i += lastLen - 1 - 1;
        lastOffset = lastLen = 0;
      } else {
        /* ignore last match and emit a symbol instead; cache this one */
        DBG_COUNT(15);
        literal(this[-1]);
        lastOffset = matchOffset;
        lastLen = matchLen;
      }
    } else { /* no last match */
      if (matchOffset) {
        DBG_COUNT(16);
        /* cache this one */
        lastOffset = matchOffset;
        lastLen = matchLen;
      } else {
        DBG_COUNT(17);
        /* emit a symbol; last already clear */
        literal(this[0]);
      }
    }
  }

  if (lastOffset) {                     /* flush cached match if any */
    copy(lastOffset, lastLen);
    DBG_PRINT("dic: %6x %6x %6x\n", i, lastLen, lastOffset);
    i += lastLen - 1;
  }
  while (i < srcLen)
    literal(src[i++]);                  /* flush the last few bytes if needed */
}

/*
 * This compress wrapper treats the input stream as a single block for
 * compression using the default Static huffman block encoding
 */
int uzlib_compress (uchar **dest, uint *destLen, const uchar *src, uint srcLen) {
  uint crc = ~uzlib_crc32(src, srcLen, ~0);
  uint chainLen = srcLen < MAX_OFFSET ? srcLen : MAX_OFFSET;
  uint hashSlots, i, j;
  int status;

  uint FLG_MTIME[] = {0x00088b1f, 0};
  ushort XFL_OS = 0x0304;

  /* The hash table has 4K slots for a 16K chain and scaling down */
  /* accordingly, for an average chain length of 4 links or thereabouts */
  for (i = 256, j = 8 - 2; i < chainLen; i <<= 1)
    j++;
  hashSlots = i >> 2;

  if ((status = UZLIB_SETJMP(unwindAddr)) == 0) {
    initTables(chainLen, hashSlots);
    oBuf->size = srcLen/5;    /* initial guess of a 5x compression ratio */
    oBuf->buffer = uz_malloc(oBuf->size);
    dynamicTables->hashSlots = hashSlots;
    dynamicTables->hashBits = j;
    if(!oBuf->buffer ) {
      status = UZLIB_MEMORY_ERROR;
    } else {
      /* Output gzip and block headers */
      outBytes(FLG_MTIME, sizeof(FLG_MTIME));
      outBytes(&XFL_OS, sizeof(XFL_OS));
      outBits(1, 1); /* Final block */
      outBits(1, 2); /* Static huffman block */

      uzlibCompressBlock(src, srcLen);  /* Do the compress */

      /* Output block finish */
      outBits(0, 7); /* close block */
      outBits(0, 7); /* Make sure all bits are flushed */
      outBytes(&crc, sizeof(crc));
      outBytes(&srcLen, sizeof(srcLen));
      status = UZLIB_OK;
    }
  } else {
    status = UZLIB_OK;
  }

  for (i=0; i<20;i++) DBG_PRINT("count %u = %u\n",i,debugCounts[i]);

  if (status == UZLIB_OK) {
    uchar *trimBuf = realloc(oBuf->buffer, oBuf->len);
    *dest = trimBuf ? trimBuf : oBuf->buffer;
    *destLen = oBuf->len;
  } else {
    *dest = NULL;
    *destLen = 0;
    FREE(oBuf->buffer);
  }

  FREE(dynamicTables);

  return status;
}
