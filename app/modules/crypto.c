// Module for cryptography

#include <errno.h>
#include "module.h"
#include "lauxlib.h"
#include "platform.h"
#include <stdint.h>
#include <stddef.h>
#include "vfs.h"
#include "../crypto/digests.h"
#include "../crypto/mech.h"
#include "lmem.h"

#include "user_interface.h"

#include "rom.h"

typedef struct {
  const digest_mech_info_t *mech_info;
  void *ctx;
  uint8_t *k_opad;
} digest_user_datum_t;

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

#ifdef LUA_USE_MODULES_ENCODER
static int call_encoder( lua_State* L, const char *function ) {
  if (lua_gettop(L) != 1) {
    luaL_error(L, "%s must have one argument", function);
  }
  lua_getglobal(L, "encoder");
  luaL_checktype(L, -1, LUA_TTABLE);
  lua_getfield(L, -1, function);
  lua_insert(L, 1);    //move function below the argument
  lua_pop(L, 1);       //and dump the encoder rotable from stack.
  lua_call(L,1,1);     // Normal call encoder.xxx(string)
                       // (errors thrown back to caller)
  return 1;
}

static int crypto_base64_encode (lua_State* L) {
  platform_print_deprecation_note("crypto.toBase64", "in the next version");
  return call_encoder(L, "toBase64");
}
static int crypto_hex_encode (lua_State* L) {
  platform_print_deprecation_note("crypto.toHex", "in the next version");
  return call_encoder(L, "toHex");
}
#else
static const char* bytes64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
/**
  * encoded = crypto.toBase64(raw)
  *
  * Encodes raw binary string as base64 string.
  */
static int crypto_base64_encode( lua_State* L )
{
  int len, i;
  const char* msg = luaL_checklstring(L, 1, &len);
  luaL_Buffer out;

  platform_print_deprecation_note("crypto.toBase64", "in the next version");

  luaL_buffinit(L, &out);
  for (i = 0; i < len; i += 3) {
    int a = msg[i];
    int b = (i + 1 < len) ? msg[i + 1] : 0;
    int c = (i + 2 < len) ? msg[i + 2] : 0;
    luaL_addchar(&out, bytes64[a >> 2]);
    luaL_addchar(&out, bytes64[((a & 3) << 4) | (b >> 4)]);
    luaL_addchar(&out, (i + 1 < len) ? bytes64[((b & 15) << 2) | (c >> 6)] : 61);
    luaL_addchar(&out, (i + 2 < len) ? bytes64[(c & 63)] : 61);
  }
  luaL_pushresult(&out);
  return 1;
}

/**
  * encoded = crypto.toHex(raw)
  *
  *	Encodes raw binary string as hex string.
  */
static int crypto_hex_encode( lua_State* L)
{
  int len, i;
  const char* msg = luaL_checklstring(L, 1, &len);
  luaL_Buffer out;

  platform_print_deprecation_note("crypto.toHex", "in the next version");

  luaL_buffinit(L, &out);
  for (i = 0; i < len; i++) {
    luaL_addchar(&out, crypto_hexbytes[msg[i] >> 4]);
    luaL_addchar(&out, crypto_hexbytes[msg[i] & 0xf]);
  }
  luaL_pushresult(&out);
  return 1;
}
#endif
/**
  * masked = crypto.mask(message, mask)
  *
  * Apply a mask (repeated if shorter than message) as XOR to each byte.
  */
static int crypto_mask( lua_State* L )
{
  int len, mask_len, i;
  const char* msg = luaL_checklstring(L, 1, &len);
  const char* mask = luaL_checklstring(L, 2, &mask_len);
  luaL_Buffer b;

  if(mask_len <= 0)
    return luaL_error(L, "invalid argument: mask");

  luaL_buffinit(L, &b);
  for (i = 0; i < len; i++) {
    luaL_addchar(&b, msg[i] ^ mask[i % mask_len]);
  }

  luaL_pushresult(&b);
  return 1;
}

static inline int bad_mech (lua_State *L) { return luaL_error (L, "unknown hash mech"); }
static inline int bad_mem  (lua_State *L) { return luaL_error (L, "insufficient memory"); }
static inline int bad_file (lua_State *L) { return luaL_error (L, "file does not exist"); }

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

/* General Usage for extensible hash functions:
 * sha = crypto.new_hash("MD5")
 * sha.update("Data")
 * sha.update("Data2")
 * strdigest = crypto.toHex(sha.finalize())
 */

#define WANT_HASH 0
#define WANT_HMAC 1

static int crypto_new_hash_hmac (lua_State *L, int what)
{
  // get pointer to relevant hash_mechs table entry in app/crypto/digest.c.  Note that
  // the size of the table needed is dependent on the the digest type
  const digest_mech_info_t *mi = crypto_digest_mech (luaL_checkstring (L, 1));
  if (!mi)
    return bad_mech (L);

  size_t len = 0, k_opad_len = 0, udlen;
  const char *key = NULL;
  uint8_t *k_opad = NULL;

  if (what == WANT_HMAC)
  { // The key and k_opad are only used for HMAC; these default to NULLs for HASH
    key = luaL_checklstring (L, 2, &len);
    k_opad_len = mi->block_size;
  }

  // create a userdatum with specific metatable.  This comprises the ud header,
  // the encrypto context block, and an optional HMAC block as a single allocation
  // unit
  udlen = sizeof(digest_user_datum_t) + mi->ctx_size + k_opad_len;
  digest_user_datum_t *dudat = (digest_user_datum_t *)lua_newuserdata(L, udlen);
  luaL_getmetatable(L, "crypto.hash");  // and set its metatable to the crypto.hash table
  lua_setmetatable(L, -2);

  void *ctx = dudat + 1;  // The context block immediately follows the digest_user_datum
  mi->create (ctx);

  if (what == WANT_HMAC) {
    // The k_opad block immediately follows the context block
    k_opad = (char *)ctx + mi->ctx_size;
    crypto_hmac_begin (ctx, mi, key, len, k_opad);
  }

  // Set pointers to the mechanics and CTX
  dudat->mech_info = mi;
  dudat->ctx       = ctx;
  dudat->k_opad    = k_opad;

  return 1; // Pass userdata object back
}

/* crypto.new_hash("MECHTYPE") */
static int crypto_new_hash (lua_State *L)
{
  return crypto_new_hash_hmac (L, WANT_HASH);
}

/* crypto.new_hmac("MECHTYPE", "KEY") */
static int crypto_new_hmac (lua_State *L)
{
  return crypto_new_hash_hmac (L, WANT_HMAC);
}


/* Called as object, params:
   1 - userdata "this"
   2 - new string to add to the hash state  */
static int crypto_hash_update (lua_State *L)
{
  NODE_DBG("enter crypto_hash_update.\n");
  digest_user_datum_t *dudat;
  size_t sl;

  dudat = (digest_user_datum_t *)luaL_checkudata(L, 1, "crypto.hash");

  const digest_mech_info_t *mi = dudat->mech_info;

  size_t len = 0;
  const char *data = luaL_checklstring (L, 2, &len);

  mi->update (dudat->ctx, data, len);

  return 0;  // No return value
}

/* Called as object, no params. Returns digest of default size. */
static int crypto_hash_finalize (lua_State *L)
{
  NODE_DBG("enter crypto_hash_update.\n");
  digest_user_datum_t *dudat;
  size_t sl;

  dudat = (digest_user_datum_t *)luaL_checkudata(L, 1, "crypto.hash");

  const digest_mech_info_t *mi = dudat->mech_info;

  uint8_t digest[mi->digest_size]; // Allocate as local
  if (dudat->k_opad)
    crypto_hmac_finalize (dudat->ctx, mi, dudat->k_opad, digest);
  else
    mi->finalize (digest, dudat->ctx);

  lua_pushlstring (L, digest, sizeof (digest));
  return 1;
}

static sint32_t vfs_read_wrap (int fd, void *ptr, size_t len)
{
  return vfs_read (fd, ptr, len);
}

/* rawdigest = crypto.hash("MD5", filename)
 * strdigest = crypto.toHex(rawdigest)
 */
static int crypto_flhash (lua_State *L)
{
  const digest_mech_info_t *mi = crypto_digest_mech (luaL_checkstring (L, 1));
  if (!mi)
    return bad_mech (L);
  const char *filename = luaL_checkstring (L, 2);

  // Open the file
  int file_fd = vfs_open (filename, "r");
  if(!file_fd) {
    return bad_file(L);
  }

  // Compute hash
  uint8_t digest[mi->digest_size];
  int returncode = crypto_fhash (mi, &vfs_read_wrap, file_fd, digest);

  // Finish up
  vfs_close(file_fd);

  if (returncode == ENOMEM)
    return bad_mem (L);
  else if (returncode == EINVAL)
    return bad_mech(L);
  else
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
  size_t klen, dlen, ivlen, bs = mech->block_size;

  const char *key = luaL_checklstring (L, 2, &klen);
  const char *data = luaL_checklstring (L, 3, &dlen);
  const char *iv = luaL_optlstring (L, 4, "", &ivlen);

  size_t outlen = ((dlen + bs -1) / bs) * bs;

  char *buf = luaM_newvector(L, outlen, char);

  crypto_op_t op = {
    key, klen,
    iv, ivlen,
    data, dlen,
    buf, outlen,
    enc ? OP_ENCRYPT : OP_DECRYPT
  };

  int status = mech->run (&op);

  lua_pushlstring (L, buf, outlen);  /* discarded on error but what the hell */
  luaN_freearray(L, buf, outlen);

  return status ? 1 : luaL_error (L, "crypto op failed");

}

static int lcrypto_encrypt (lua_State *L)
{
  return crypto_encdec (L, true);
}

static int lcrypto_decrypt (lua_State *L)
{
  return crypto_encdec (L, false);
}

// Hash function map

LROT_BEGIN(crypto_hash_map, NULL, LROT_MASK_INDEX)
  LROT_TABENTRY( __index, crypto_hash_map )
  LROT_FUNCENTRY( update, crypto_hash_update )
  LROT_FUNCENTRY( finalize, crypto_hash_finalize )
LROT_END(crypto_hash_map, NULL, LROT_MASK_INDEX)



// Module function map
LROT_BEGIN(crypto, NULL, 0)
  LROT_FUNCENTRY( sha1, crypto_sha1 )
  LROT_FUNCENTRY( toBase64, crypto_base64_encode )
  LROT_FUNCENTRY( toHex, crypto_hex_encode )
  LROT_FUNCENTRY( mask, crypto_mask )
  LROT_FUNCENTRY( hash, crypto_lhash )
  LROT_FUNCENTRY( fhash, crypto_flhash )
  LROT_FUNCENTRY( new_hash, crypto_new_hash )
  LROT_FUNCENTRY( hmac, crypto_lhmac )
  LROT_FUNCENTRY( new_hmac, crypto_new_hmac )
  LROT_FUNCENTRY( encrypt, lcrypto_encrypt )
  LROT_FUNCENTRY( decrypt, lcrypto_decrypt )
LROT_END(crypto, NULL, 0)


int luaopen_crypto ( lua_State *L )
{
  luaL_rometatable(L, "crypto.hash", LROT_TABLEREF(crypto_hash_map));
  return 0;
}

NODEMCU_MODULE(CRYPTO, "crypto", crypto, luaopen_crypto);
