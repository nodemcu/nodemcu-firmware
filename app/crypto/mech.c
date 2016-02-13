/*
 * Copyright 2016 Dius Computing Pty Ltd. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the
 *   distribution.
 * - Neither the name of the copyright holders nor the names of
 *   its contributors may be used to endorse or promote products derived
 *   from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * @author Johny Mattsson <jmattsson@dius.com.au>
 */

#include "mech.h"
#include "sdk-aes.h"
#include "c_string.h"

/* ----- AES ---------------------------------------------------------- */

static const struct aes_funcs
{
  void *(*init) (const char *key, size_t keylen);
  void (*crypt) (void *ctx, const char *in, char *out);
  void (*deinit) (void *ctx);
} aes_funcs[] =
{
  { aes_encrypt_init, aes_encrypt, aes_encrypt_deinit },
  { aes_decrypt_init, aes_decrypt, aes_decrypt_deinit }
};

static bool do_aes (crypto_op_t *co, bool with_cbc)
{
  const struct aes_funcs *funcs = &aes_funcs[co->op];

  void *ctx = funcs->init (co->key, co->keylen);
  if (!ctx)
    return false;

  char iv[AES_BLOCKSIZE] = { 0 };
  if (with_cbc && co->ivlen)
    c_memcpy (iv, co->iv, co->ivlen < AES_BLOCKSIZE ? co->ivlen : AES_BLOCKSIZE);

  const char *src = co->data;
  char *dst = co->out;

  size_t left = co->datalen;
  while (left)
  {
    char block[AES_BLOCKSIZE] = { 0 };
    size_t n = left > AES_BLOCKSIZE ? AES_BLOCKSIZE : left;
    c_memcpy (block, src, n);

    if (with_cbc && co->op == OP_ENCRYPT)
    {
      const char *xor = (src == co->data) ? iv : dst - AES_BLOCKSIZE;
      int i;
      for (i = 0; i < AES_BLOCKSIZE; ++i)
        block[i] ^= xor[i];
    }

    funcs->crypt (ctx, block, dst);

    if (with_cbc && co->op == OP_DECRYPT)
    {
      const char *xor = (src == co->data) ? iv : src - AES_BLOCKSIZE;
      int i;
      for (i = 0; i < AES_BLOCKSIZE; ++i)
        dst[i] ^= xor[i];
    }

    left -= n;
    src += n;
    dst += n;
  }

  funcs->deinit (ctx);
  return true;
}


static bool do_aes_ecb (crypto_op_t *co)
{
  return do_aes (co, false);
}

static bool do_aes_cbc (crypto_op_t *co)
{
  return do_aes (co, true);
}


/* ----- mechs -------------------------------------------------------- */

static const crypto_mech_t mechs[] =
{
  { "AES-ECB",  do_aes_ecb, AES_BLOCKSIZE },
  { "AES-CBC",  do_aes_cbc, AES_BLOCKSIZE }
};


const crypto_mech_t *crypto_encryption_mech (const char *name)
{
  size_t i;
  for (i = 0; i < sizeof (mechs) / sizeof (mechs[0]); ++i)
  {
    const crypto_mech_t *mech = mechs + i;
    if (strcasecmp (name, mech->name) == 0)
      return mech;
  }
  return 0;
}
