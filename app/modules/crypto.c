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
#include "lua.h"
#include "lauxlib.h"
#include "lrotable.h"
#include "auxmods.h"
#include "lwip/mem.h"
#include "ssl/ssl_crypto.h"
#include <string.h>

typedef void (*create_ctx_fn)(void *ctx);
typedef void (*update_ctx_fn)(void *ctx, const uint8_t *msg, int len);
typedef void (*finalize_ctx_fn)(uint8_t *digest, void *ctx);

typedef char ensure_int_and_size_t_same[(sizeof(int)==sizeof(size_t)) ? 0 : -1];

typedef struct
{
  const char *    name;
  create_ctx_fn   create;
  update_ctx_fn   update;
  finalize_ctx_fn finalize;
  uint16_t        ctx_size;
  uint16_t        digest_size;
  uint16_t        block_size;
} mech_info_t;

/* None of the functions match the prototype fully due to the void *, and in
   some cases also the int vs size_t len, so wrap declarations in a macro. */
#define MECH(pfx, ds, bs) \
  { #pfx, \
    (create_ctx_fn)pfx ## _Init, \
    (update_ctx_fn)pfx ## _Update, \
    (finalize_ctx_fn)pfx ## _Final, \
    sizeof(pfx ## _CTX), \
    ds, \
    bs }

static const mech_info_t hash_mechs[] =
{
   MECH(MD2,  MD2_SIZE,  16)
  ,MECH(MD5,  MD5_SIZE,  64)
  ,MECH(SHA1, SHA1_SIZE, 64)
#ifndef WITHOUT_SHA2
  ,MECH(SHA256, SHA256_DIGEST_LENGTH, SHA256_BLOCK_LENGTH)
  ,MECH(SHA384, SHA384_DIGEST_LENGTH, SHA384_BLOCK_LENGTH)
  ,MECH(SHA512, SHA512_DIGEST_LENGTH, SHA512_BLOCK_LENGTH)
#endif
};

#undef MECH

static const mech_info_t *get_hash_mech (const char *mech)
{
  if (!mech)
    return 0;

  size_t i;
  for (i = 0; i < (sizeof (hash_mechs) / sizeof (mech_info_t)); ++i)
  {
    const mech_info_t *mi = hash_mechs + i;
    if (strcasecmp (mech, mi->name) == 0)
      return mi;
  }
  return 0;
}

static const char hex[] = "0123456789abcdef";
static void inplace_bin2ascii (char *buf, size_t binlen)
{
  size_t aidx = binlen * 2;
  buf[aidx--] = 0;
  int i;
  for (i = binlen -1; i >= 0; --i)
  {
    buf[aidx--] = hex[buf[i] & 0xf];
    buf[aidx--] = hex[buf[i] >>  4];
  }
}


static int bad_mech (lua_State *L) { return luaL_error (L, "unknown hash mech"); }
static int bad_data (lua_State *L) { return luaL_error (L, "expected string data"); }
static int bad_mem  (lua_State *L) { return luaL_error (L, "insufficient memory"); }


static void alloc_state (lua_State *L, const mech_info_t *mi, void **pctx, uint8_t **pdigest)
{
  *pctx = os_malloc (mi->ctx_size);
  if (!*pctx)
    bad_mem (L);

  // allow for in-place binary->ascii conversion
  *pdigest = os_malloc (mi->digest_size * 2 + 1);
  if (!*pdigest)
  {
    os_free (*pdigest);
    bad_mem (L);
  }
}


// crypto.hash("MD5", str)
static int crypto_hash (lua_State *L)
{
  const mech_info_t *mi = get_hash_mech (luaL_checkstring (L, 1));
  if (!mi)
    return bad_mech (L);
  size_t len = 0;
  const char *data = lua_tolstring (L, 2, &len);
  if (!data)
    return bad_data (L);

  void *ctx;
  uint8_t *digest;
  alloc_state (L, mi, &ctx, &digest);

  mi->create (ctx);
  mi->update (ctx, data, len);
  mi->finalize (digest, ctx);

  inplace_bin2ascii (digest, mi->digest_size);
  lua_pushstring (L, (const char *)digest);

  os_free (digest);
  os_free (ctx);
  return 1;
}


// crypto.hmac("SHA1", str, key) -- note key must be <= block_size
static int crypto_hmac (lua_State *L)
{
  const mech_info_t *mi = get_hash_mech (luaL_checkstring (L, 1));
  if (!mi)
    return bad_mech (L);
  size_t len = 0;
  const char *data = lua_tolstring (L, 2, &len);
  if (!data)
    return bad_data (L);

  size_t klen = 0;
  const char *key = lua_tolstring (L, 3, &klen);
  if (!key)
    return bad_data (L);

  void *ctx;
  uint8_t *digest;
  alloc_state (L, mi, &ctx, &digest);

  const size_t bs = mi->block_size;
  uint8_t k_ipad[bs];
  uint8_t k_opad[bs];

  os_memset (k_ipad, 0x36, bs);
  os_memset (k_opad, 0x5c, bs);
  size_t i;
  for (i = 0; i < klen; ++i)
  {
    k_ipad[i] ^= key[i];
    k_opad[i] ^= key[i];
  }

  mi->create (ctx);
  mi->update (ctx, k_ipad, bs);
  mi->update (ctx, data, len);
  mi->finalize (digest, ctx);

  mi->create (ctx);
  mi->update (ctx, k_opad, bs);
  mi->update (ctx, digest, mi->digest_size);
  mi->finalize (digest, ctx);

  inplace_bin2ascii (digest, mi->digest_size);
  lua_pushstring (L, (const char *)digest);

  os_free (digest);
  os_free (ctx);
  return 1;
}

#define MIN_OPT_LEVEL 2
#include "lrodefs.h"
const LUA_REG_TYPE crypto_map[] =
{
  { LSTRKEY( "hash"   ), LFUNCVAL( crypto_hash ) },
  { LSTRKEY( "hmac"   ), LFUNCVAL( crypto_hmac ) },
  { LNILKEY, LNILVAL }
};

LUALIB_API int luaopen_crypto (lua_State *L)
{
  LREGISTER (L, AUXLIB_CRYPTO, crypto_map);
}
