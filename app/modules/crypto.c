// Module for cryptography

//#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
#include "platform.h"
#include "auxmods.h"
#include "lrotable.h"

#include "c_types.h"
#include "c_stdlib.h"

#include "user_interface.h"

/**
  * hash = crypto.sha1(input)
  *
  *	Calculates raw SHA1 hash of input string.
  * Input is arbitrary string, output is raw 20-byte hash as string.
  */
static int crypto_sha1( lua_State* L )
{
  // We only need a data buffer large enough to match SHA1_CTX in the rom.
  // I *think* this is a 92-byte netbsd struct.
  uint8_t ctx[100];
  uint8_t digest[20];
  // Read the string from lua (with length)
  int len;
  const char* msg = luaL_checklstring(L, 1, &len);
  // Use the SHA* functions in the rom
  SHA1Init(ctx);
  SHA1Update(ctx, msg, len);
  SHA1Final(digest, ctx);

  // Push the result as a lua string
  lua_pushlstring(L, digest, 20);
  return 1;
}

static const char* bytes64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
/**
  * encoded = crypto.base64Encode(raw)
  *
  * Encodes raw binary string as base64 string.
  */
static int crypto_base64_encode( lua_State* L )
{
  // TODO: figure out signature of base64_encode in rom and use that instead.
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

static const char* byteshex = "0123456789abcdef";
/**
  * encoded = crypto.hexEncode(raw)
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
    out[j++] = byteshex[msg[i] >> 4];
    out[j++] = byteshex[msg[i] & 0xf];
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

// Module function map
#define MIN_OPT_LEVEL 2
#include "lrodefs.h"
const LUA_REG_TYPE crypto_map[] =
{
  { LSTRKEY( "sha1" ), LFUNCVAL( crypto_sha1 ) },
  { LSTRKEY( "base64Encode" ), LFUNCVAL( crypto_base64_encode ) },
  { LSTRKEY( "hexEncode" ), LFUNCVAL( crypto_hex_encode ) },
  { LSTRKEY( "mask" ), LFUNCVAL( crypto_mask ) },

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
