/************************************************************************
 * NodeMCU unzip wrapper code for uzlib_inflate
 *
 * Note that whilst it would be more straightforward to implement a
 * simple in memory approach, this utility adopts the same streaming
 * callback architecture as app/lua/lflash.c to enable this code to be
 * tested in a pure host development environment
 */
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include "uzlib.h"

/* Test wrapper */
#define DICTIONARY_WINDOW 16384
#define READ_BLOCKSIZE 2048
#define WRITE_BLOCKSIZE 2048
#define WRITE_BLOCKS ((DICTIONARY_WINDOW/WRITE_BLOCKSIZE)+1)
#define FREE(v) if (v) uz_free(v)

typedef uint8_t  uchar;
typedef uint16_t ushort;
typedef uint32_t uint;

struct INPUT {
  FILE    *fin;
  int      len;
  uint8_t  block[READ_BLOCKSIZE];
  uint8_t *inPtr;
  int      bytesRead;
  int      left;
} *in;

typedef struct {
  uint8_t byte[WRITE_BLOCKSIZE];
} outBlock;

struct OUTPUT {
  FILE     *fout;
  outBlock *block[WRITE_BLOCKS];
  int       ndx;
  int       written;
  int       len;
  uint32_t  crc;
  int       breakNdx;
  int     (*fullBlkCB) (void);
} *out;


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

uint8_t get_byte (void) {
  if (--in->left < 0) {
    /* Read next input block */
    int remaining = in->len - in->bytesRead;
    int wanted    = remaining >= READ_BLOCKSIZE ? READ_BLOCKSIZE : remaining;

    if (fread(in->block, 1, wanted, in->fin) != wanted)
      UZLIB_THROW(UZLIB_DATA_ERROR);

    in->bytesRead += wanted;
    in->inPtr      = in->block;
    in->left       = wanted-1;
  }
  return *in->inPtr++;
}


void put_byte (uint8_t value) {
  int offset = out->ndx % WRITE_BLOCKSIZE;  /* counts from 0 */

  out->block[0]->byte[offset++] = value;
  out->ndx++;

  if (offset == WRITE_BLOCKSIZE || out->ndx == out->len) {
    if (out->fullBlkCB)
      out->fullBlkCB();
    /* circular shift the block pointers (redundant on last block, but so what) */
    outBlock *nextBlock  = out->block[WRITE_BLOCKS - 1];
    memmove(out->block+1, out->block, (WRITE_BLOCKS-1)*sizeof(void*));
    out->block[0] = nextBlock;
  }
}


uint8_t recall_byte (uint offset) {
  if(offset > DICTIONARY_WINDOW || offset >= out->ndx)
    UZLIB_THROW(UZLIB_DICT_ERROR);
  /* ndx starts at 1. Need relative to 0 */
  uint n   = out->ndx - offset;
  uint pos = n % WRITE_BLOCKSIZE;
  uint blockNo = out->ndx / WRITE_BLOCKSIZE - n  / WRITE_BLOCKSIZE;
  return out->block[blockNo]->byte[pos];
}


int processOutRec (void) {
  int len = (out->ndx % WRITE_BLOCKSIZE) ? out->ndx % WRITE_BLOCKSIZE :
                                           WRITE_BLOCKSIZE;
  if (fwrite(out->block[0], 1, len, out->fout) != len)
    UZLIB_THROW(UZLIB_DATA_ERROR);

  out->crc = uzlib_crc32(out->block[0], len, out->crc);

  out->written += len;
  if (out->written == out->len) {
    fclose(out->fout);
    out->fullBlkCB = NULL;
  }
  return 1;
}


int main(int argc, char *argv[]) {
  assert (argc==3);
  const char *inFile = argv[1], *outFile = argv[2];
  int i, n, res;
  uint crc;
  void *cxt_not_used;
  assert(sizeof(unsigned int) == 4);

  /* allocate and zero the in and out Blocks */
  assert((in  = uz_malloc(sizeof(*in))) != NULL);
  assert((out = uz_malloc(sizeof(*out))) != NULL);

  memset(in, 0, sizeof(*in));
  memset(out, 0, sizeof(*out));

  /* open input files and probe end to read the expanded length */
  assert((in->fin = fopen(inFile, "rb")) != NULL);
  fseek(in->fin, -4, SEEK_END);
  assert(fread((uchar*)&(out->len), 1, 4, in->fin) == 4);
  in->len = ftell(in->fin);
  fseek(in->fin, 0, SEEK_SET);

  assert((out->fout = fopen(outFile, "wb")) != NULL);

  printf ("Inflating in=%s out=%s\n", inFile, outFile);

  /* Allocate the out buffers (number depends on the unpacked length) */
  n = (out->len > DICTIONARY_WINDOW) ? WRITE_BLOCKS :
                                      1 + (out->len-1) / WRITE_BLOCKSIZE;
  for(i = WRITE_BLOCKS - n + 1;  i <= WRITE_BLOCKS; i++)
    assert((out->block[i % WRITE_BLOCKS] = uz_malloc(sizeof(outBlock))) != NULL);

  out->breakNdx  = (out->len < WRITE_BLOCKSIZE) ? out->len : WRITE_BLOCKSIZE;
  out->fullBlkCB = processOutRec;
  out->crc       = ~0;

  /* Call inflate to do the business */
  res = uzlib_inflate (get_byte, put_byte, recall_byte, in->len, &crc, &cxt_not_used);

  if (res > 0 && crc != ~out->crc)
    res = UZLIB_CHKSUM_ERROR;

  for (i = 0; i < WRITE_BLOCKS; i++)
    FREE(out->block[i]);

  fclose(in->fin);
  FREE(in);
  FREE(out);

  if (res < 0)
    printf("Error during decompression: %d\n", res);

  return (res != 0) ? 1: 0;
}
