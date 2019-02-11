/************************************************************************
 * NodeMCU zip wrapper code for uzlib_compress
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include "uzlib.h"
#define fwriterec(r) fwrite(&(r), sizeof(r), 1, fout);
#define BAD_FILE (-1)

int main (int argc, char **argv) {
  const char *in = argv[1], *out = argv[2];
  if (argc!=3)
    return 1;
  printf ("Compressing in=%s out=%s\n", in, out);
  FILE *fin, *fout;
  int status = -1;
  uint32_t iLen, oLen;
  uint8_t *iBuf, *oBuf;

  if (!(fin = fopen(in, "rb")) || fseek(fin, 0, SEEK_END) ||
      (iLen = ftell(fin)) <= 0  || fseek(fin, 0, SEEK_SET))
    return 1;
  if ((fout = fopen(out, "wb")) == NULL ||
      (iBuf = (uint8_t *) uz_malloc(iLen)) == NULL ||
      fread(iBuf, 1, iLen, fin) != iLen)
    return 1;

  if (uzlib_compress (&oBuf, &oLen, iBuf, iLen) == UZLIB_OK &&
      oLen == fwrite(oBuf, oLen, 1, fout))
    status = UZLIB_OK;
  uz_free(iBuf);
  if (oBuf) uz_free(oBuf);

  fclose(fin);
  fclose(fout);

  if (status == UZLIB_OK)
    unlink(out);

  return (status == UZLIB_OK) ? 1: 0;
}

