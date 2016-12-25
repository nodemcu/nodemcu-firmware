/*
 * Copyright (c) 2015, DiUS Computing Pty Ltd (jmattsson@dius.com.au)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTOR(S) ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTOR(S) BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */
#include "vfs.h"
#include "digests.h"
#include "user_config.h"
#include "rom.h"
#include "osapi.h"
#include "mem.h"
#include <string.h>
#include <c_errno.h>

#ifdef MD2_ENABLE
#include "ssl/ssl_crypto.h"
#endif

#ifdef SHA2_ENABLE
#include "sha2.h"
#endif

typedef char ensure_int_and_size_t_same[(sizeof(int)==sizeof(size_t)) ? 0 : -1];

/* None of the functions match the prototype fully due to the void *, and in
   some cases also the int vs size_t len, so wrap declarations in a macro. */
#define MECH(pfx, u, ds, bs) \
  { #pfx, \
    (create_ctx_fn)pfx ## u ## Init, \
    (update_ctx_fn)pfx ## u ## Update, \
    (finalize_ctx_fn)pfx ## u ## Final, \
    sizeof(pfx ## _CTX), \
    ds, \
    bs }

static const digest_mech_info_t hash_mechs[] ICACHE_RODATA_ATTR =
{
#ifdef MD2_ENABLE
   MECH(MD2, _ , MD2_SIZE,  16),
#endif
   MECH(MD5,   , MD5_DIGEST_LENGTH,  64)
  ,MECH(SHA1,  , SHA1_DIGEST_LENGTH, 64)
#ifdef SHA2_ENABLE
  ,MECH(SHA256, _ , SHA256_DIGEST_LENGTH, SHA256_BLOCK_LENGTH)
  ,MECH(SHA384, _ , SHA384_DIGEST_LENGTH, SHA384_BLOCK_LENGTH)
  ,MECH(SHA512, _ , SHA512_DIGEST_LENGTH, SHA512_BLOCK_LENGTH)
#endif
};

#undef MECH

const digest_mech_info_t *ICACHE_FLASH_ATTR crypto_digest_mech (const char *mech)
{
  if (!mech)
    return 0;

  size_t i;
  for (i = 0; i < (sizeof (hash_mechs) / sizeof (digest_mech_info_t)); ++i)
  {
    const digest_mech_info_t *mi = hash_mechs + i;
    if (strcasecmp (mech, mi->name) == 0)
      return mi;
  }
  return 0;
}

const char crypto_hexbytes[] = "0123456789abcdef";

// note: supports in-place encoding
void ICACHE_FLASH_ATTR crypto_encode_asciihex (const char *bin, size_t binlen, char *outbuf)
{
  size_t aidx = binlen * 2 -1;
  int i;
  for (i = binlen -1; i >= 0; --i)
  {
    outbuf[aidx--] = crypto_hexbytes[bin[i] & 0xf];
    outbuf[aidx--] = crypto_hexbytes[bin[i] >>  4];
  }
}


int ICACHE_FLASH_ATTR crypto_hash (const digest_mech_info_t *mi,
  const char *data, size_t data_len,
  uint8_t *digest)
{
  if (!mi)
    return EINVAL;

  void *ctx = (void *)os_malloc (mi->ctx_size);
  if (!ctx)
    return ENOMEM;

  mi->create (ctx);
  mi->update (ctx, data, data_len);
  mi->finalize (digest, ctx);

  os_free (ctx);
  return 0;
}


int ICACHE_FLASH_ATTR crypto_fhash (const digest_mech_info_t *mi,
  read_fn read, int readarg,
  uint8_t *digest)
{
  if (!mi)
    return EINVAL;

  // Initialise
  void *ctx = (void *)os_malloc (mi->ctx_size);
  if (!ctx)
    return ENOMEM;
  mi->create (ctx);

  // Hash bytes from file in blocks
  uint8_t* buffer = (uint8_t*)os_malloc (mi->block_size);
  if (!buffer)
    return ENOMEM;
  
  int read_len = 0;
  do {
    read_len = read(readarg, buffer, mi->block_size);
    if (read_len > 0) {
      mi->update (ctx, buffer, read_len);
    }
  } while (read_len == mi->block_size);

  // Finish up
  mi->finalize (digest, ctx);

  os_free (buffer);
  os_free (ctx);
  return 0;
}


void crypto_hmac_begin (void *ctx, const digest_mech_info_t *mi,
  const char *key, size_t key_len, uint8_t *k_opad)
{
  // If key too long, it needs to be hashed before use
  char tmp[mi->digest_size];
  if (key_len > mi->block_size)
  {
    mi->update (ctx, key, key_len);
    mi->finalize (tmp, ctx);
    key = tmp;
    key_len = mi->digest_size;
    mi->create (ctx); // refresh
  }

  const size_t bs = mi->block_size;
  uint8_t k_ipad[bs];

  os_memset (k_ipad, 0x36, bs);
  os_memset (k_opad, 0x5c, bs);
  size_t i;
  for (i = 0; i < key_len; ++i)
  {
    k_ipad[i] ^= key[i];
    k_opad[i] ^= key[i];
  }

  mi->update (ctx, k_ipad, bs);
}


void crypto_hmac_finalize (void *ctx, const digest_mech_info_t *mi,
  const uint8_t *k_opad, uint8_t *digest)
{
  mi->finalize (digest, ctx);

  mi->create (ctx);
  mi->update (ctx, k_opad, mi->block_size);
  mi->update (ctx, digest, mi->digest_size);
  mi->finalize (digest, ctx);
}


int crypto_hmac (const digest_mech_info_t *mi,
   const char *data, size_t data_len,
   const char *key, size_t key_len,
   uint8_t *digest)
{
  if (!mi)
    return EINVAL;

  struct {
    uint8_t ctx[mi->ctx_size];
    uint8_t k_opad[mi->block_size];
  } *tmp = os_malloc (sizeof (*tmp));
  if (!tmp)
    return ENOMEM;

  mi->create (tmp->ctx);
  crypto_hmac_begin (tmp->ctx, mi, key, key_len, tmp->k_opad);
  mi->update (tmp->ctx, data, data_len);
  crypto_hmac_finalize (tmp->ctx, mi, tmp->k_opad, digest);

  os_free (tmp);
  return 0;
}
