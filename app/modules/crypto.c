// Module for cryptography

//#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
#include "platform.h"
#include "auxmods.h"
#include "lrotable.h"
#include "c_types.h"
#include "c_stdlib.h"
#include "../crypto/digests.h"

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


// Module function map
#define MIN_OPT_LEVEL 2
#include "lrodefs.h"
const LUA_REG_TYPE crypto_map[] =
{
  { LSTRKEY( "sha1" ), LFUNCVAL( crypto_sha1 ) },
  { LSTRKEY( "toBase64" ), LFUNCVAL( crypto_base64_encode ) },
  { LSTRKEY( "toHex" ), LFUNCVAL( crypto_hex_encode ) },
  { LSTRKEY( "mask" ), LFUNCVAL( crypto_mask ) },
  { LSTRKEY( "hash"   ), LFUNCVAL( crypto_lhash ) },
  { LSTRKEY( "hmac"   ), LFUNCVAL( crypto_lhmac ) },

#if LUA_OPTIMIZE_MEMORY > 0

#endif
  { LNILKEY, LNILVAL }
};

LUALIB_API int luaopen_crypto( lua_State *L )
{
#if LUA_OPTIMIZE_MEMORY > 0
  return 0;
#else // #if LUA_OPTIMIZE_MEMORY > 0
  luaL_register( L, AUXLIB_CRYPTO, crypto_map );
  // Add constants

  return 1;
#endif // #if LUA_OPTIMIZE_MEMORY > 0
}
