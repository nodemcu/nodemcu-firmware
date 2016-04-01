#ifndef _MECH_H_
#define _MECH_H_

#include "c_types.h"

typedef struct
{
  const char *key;
  size_t keylen;
  const char *iv;
  size_t ivlen;
  const char *data;
  size_t datalen;
  char *out;
  size_t outlen;
  enum { OP_ENCRYPT, OP_DECRYPT } op;
} crypto_op_t;


typedef struct
{
  const char *name;
  bool (*run) (crypto_op_t *op);
  uint16_t block_size;
} crypto_mech_t;


const crypto_mech_t *crypto_encryption_mech (const char *name);

#endif
