// Module for cryptography

#include "module.h"
#include "lauxlib.h"
#include "platform.h"
#include "c_types.h"
#include "c_stdlib.h"
#include "../crypto/digests.h"
#include "../crypto/mech.h"

#include "user_interface.h"

#include "rom.h"

/**
  * hash = crypto.sha1(input)
  *
  *	Calculates raw SHA1 hash of input string.
  * Input is arbitrary string, output is raw 20-byte hash as string.
  */
static int crypto_sha1( lua_State* L )
{
  SHA1_CTX ctx;
  uint8_t digest[20];
  // Read the string from lua (with length)
  int len;
  const char* msg = luaL_checklstring(L, 1, &len);
  // Use the SHA* functions in the rom
  SHA1Init(&ctx);
  SHA1Update(&ctx, msg, len);
  SHA1Final(digest, &ctx);

  // Push the result as a lua string
  lua_pushlstring(L, digest, 20);
  return 1;
}


static const char* bytes64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
/**
  * encoded = crypto.toBase64(raw)
  *
  * Encodes raw binary string as base64 string.
  */
static int crypto_base64_encode( lua_State* L )
{
  int len;
  const char* msg = luaL_checklstring(L, 1, &len);
  int blen = (len + 2) / 3 * 4;
  char* out = (char*)c_malloc(blen);
  int j = 0, i;
  for (i = 0; i < len; i += 3) {
    int a = msg[i];
    int b = (i + 1 < len) ? msg[i + 1] : 0;
    int c = (i + 2 < len) ? msg[i + 2] : 0;
    out[j++] = bytes64[a >> 2];
    out[j++] = bytes64[((a & 3) << 4) | (b >> 4)];
    out[j++] = (i + 1 < len) ? bytes64[((b & 15) << 2) | (c >> 6)] : 61;
    out[j++] = (i + 2 < len) ? bytes64[(c & 63)] : 61;
  }
  lua_pushlstring(L, out, j);
  c_free(out);
  return 1;
}

/**
  * encoded = crypto.toHex(raw)
  *
  *	Encodes raw binary string as hex string.
  */
static int crypto_hex_encode( lua_State* L)
{
  int len;
  const char* msg = luaL_checklstring(L, 1, &len);
  char* out = (char*)c_malloc(len * 2);
  int i, j = 0;
  for (i = 0; i < len; i++) {
    out[j++] = crypto_hexbytes[msg[i] >> 4];
    out[j++] = crypto_hexbytes[msg[i] & 0xf];
  }
  lua_pushlstring(L, out, len*2);
  c_free(out);
  return 1;
}

/**
  * masked = crypto.mask(message, mask)
  *
  * Apply a mask (repeated if shorter than message) as XOR to each byte.
  */
static int crypto_mask( lua_State* L )
{
  int len, mask_len;
  const char* msg = luaL_checklstring(L, 1, &len);
  const char* mask = luaL_checklstring(L, 2, &mask_len);
  int i;
  char* copy = (char*)c_malloc(len);
  for (i = 0; i < len; i++) {
    copy[i] = msg[i] ^ mask[i % 4];
  }
  lua_pushlstring(L, copy, len);
  c_free(copy);
  return 1;
}


static inline int bad_mech (lua_State *L) { return luaL_error (L, "unknown hash mech"); }
static inline int bad_mem  (lua_State *L) { return luaL_error (L, "insufficient memory"); }


/* rawdigest = crypto.hash("MD5", str)
 * strdigest = crypto.toHex(rawdigest)
 */
static int crypto_lhash (lua_State *L)
{
  const digest_mech_info_t *mi = crypto_digest_mech (luaL_checkstring (L, 1));
  if (!mi)
    return bad_mech (L);
  size_t len = 0;
  const char *data = luaL_checklstring (L, 2, &len);

  uint8_t digest[mi->digest_size];
  if (crypto_hash (mi, data, len, digest) != 0)
    return bad_mem (L);

  lua_pushlstring (L, digest, sizeof (digest));
  return 1;
}


/* rawsignature = crypto.hmac("SHA1", str, key)
 * strsignature = crypto.toHex(rawsignature)
 */
static int crypto_lhmac (lua_State *L)
{
  const digest_mech_info_t *mi = crypto_digest_mech (luaL_checkstring (L, 1));
  if (!mi)
    return bad_mech (L);
  size_t len = 0;
  const char *data = luaL_checklstring (L, 2, &len);
  size_t klen = 0;
  const char *key = luaL_checklstring (L, 3, &klen);

  uint8_t digest[mi->digest_size];
  if (crypto_hmac (mi, data, len, key, klen, digest) != 0)
    return bad_mem (L);

  lua_pushlstring (L, digest, sizeof (digest));
  return 1;
}



static const crypto_mech_t *get_mech (lua_State *L, int idx)
{
  const char *name = luaL_checkstring (L, idx);

  const crypto_mech_t *mech = crypto_encryption_mech (name);
  if (mech)
    return mech;

  luaL_error (L, "unknown cipher: %s", name);
  __builtin_unreachable ();
}

static int crypto_encdec (lua_State *L, bool enc)
{
  const crypto_mech_t *mech = get_mech (L, 1);
  size_t klen;
  const char *key = luaL_checklstring (L, 2, &klen);
  size_t dlen;
  const char *data = luaL_checklstring (L, 3, &dlen);

  size_t ivlen;
  const char *iv = luaL_optlstring (L, 4, "", &ivlen);

  size_t bs = mech->block_size;
  size_t outlen = ((dlen + bs -1) / bs) * bs;
  char *buf = (char *)os_zalloc (outlen);
  if (!buf)
    return luaL_error (L, "crypto init failed");

  crypto_op_t op =
  {
    key, klen,
    iv, ivlen,
    data, dlen,
    buf, outlen,
    enc ? OP_ENCRYPT : OP_DECRYPT
  };
  if (!mech->run (&op))
  {
    os_free (buf);
    return luaL_error (L, "crypto op failed");
  }
  else
  {
    lua_pushlstring (L, buf, outlen);
    // note: if lua_pushlstring runs out of memory, we leak buf :(
    os_free (buf);
    return 1;
  }
}

static int lcrypto_encrypt (lua_State *L)
{
  return crypto_encdec (L, true);
}

static int lcrypto_decrypt (lua_State *L)
{
  return crypto_encdec (L, false);
}


// Module function map
static const LUA_REG_TYPE crypto_map[] = {
  { LSTRKEY( "sha1" ),     LFUNCVAL( crypto_sha1 ) },
  { LSTRKEY( "toBase64" ), LFUNCVAL( crypto_base64_encode ) },
  { LSTRKEY( "toHex" ),    LFUNCVAL( crypto_hex_encode ) },
  { LSTRKEY( "mask" ),     LFUNCVAL( crypto_mask ) },
  { LSTRKEY( "hash"   ),   LFUNCVAL( crypto_lhash ) },
  { LSTRKEY( "hmac"   ),   LFUNCVAL( crypto_lhmac ) },
  { LSTRKEY( "encrypt" ),  LFUNCVAL( lcrypto_encrypt ) },
  { LSTRKEY( "decrypt" ),  LFUNCVAL( lcrypto_decrypt ) },
  { LNILKEY, LNILVAL }
};

NODEMCU_MODULE(CRYPTO, "crypto", crypto_map, NULL);
