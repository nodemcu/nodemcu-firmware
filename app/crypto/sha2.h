#ifndef __SHA2_H__
#define __SHA2_H__

#include <c_types.h>

/**************************************************************************
 * SHA256/384/512 declarations
 **************************************************************************/

#define SHA256_BLOCK_LENGTH  64
#define SHA256_DIGEST_LENGTH 32

typedef struct
{
  uint32_t state[8];
  uint64_t bitcount;
  uint8_t  buffer[SHA256_BLOCK_LENGTH];
} SHA256_CTX;


void SHA256_Init(SHA256_CTX *);
void SHA256_Update(SHA256_CTX *, const uint8_t *msg, size_t len);
void SHA256_Final(uint8_t[SHA256_DIGEST_LENGTH], SHA256_CTX*);

#define SHA384_BLOCK_LENGTH  128
#define SHA384_DIGEST_LENGTH  48

typedef struct
{
  uint64_t state[8];
  uint64_t bitcount[2];
  uint8_t  buffer[SHA384_BLOCK_LENGTH];
} SHA384_CTX;

void SHA384_Init(SHA384_CTX*);
void SHA384_Update(SHA384_CTX*, const uint8_t *msg, size_t len);
void SHA384_Final(uint8_t[SHA384_DIGEST_LENGTH], SHA384_CTX*);

#define SHA512_BLOCK_LENGTH  128
#define SHA512_DIGEST_LENGTH  64
typedef SHA384_CTX SHA512_CTX;

void SHA512_Init(SHA512_CTX*);
void SHA512_Update(SHA512_CTX*, const uint8_t *msg, size_t len);
void SHA512_Final(uint8_t[SHA512_DIGEST_LENGTH], SHA512_CTX*);

#endif
