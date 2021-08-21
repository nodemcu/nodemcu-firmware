#include "module.h"
#include "lauxlib.h"
#include "lmem.h"

#include "sodium.h"

static void check_init(lua_State *L)
{
  if (sodium_init() == -1) {
    luaL_error(L, "sodium_init returned an error");
  }
}

// https://download.libsodium.org/doc/generating_random_data

static int l_randombytes_random(lua_State *L)
{
  check_init(L);
  uint32_t ret = randombytes_random();
  lua_pushnumber(L, (lua_Number)ret);
  return 1;
}

static int l_randombytes_uniform(lua_State *L)
{
  check_init(L);
  uint32_t upper_bound = (uint32_t)luaL_checkinteger(L, 1);
  uint32_t ret = randombytes_uniform(upper_bound);
  lua_pushnumber(L, (lua_Number)ret);
  return 1;
}

static int l_randombytes_buf(lua_State *L)
{
  check_init(L);
  size_t count = (size_t)luaL_checkinteger(L, 1);
  if (count <= LUAL_BUFFERSIZE) {
    luaL_Buffer b;
    luaL_buffinit(L, &b);
    randombytes_buf(luaL_prepbuffer(&b), count);
    luaL_addsize(&b, count);
    luaL_pushresult(&b);
  } else {
    char *buf = (char *)luaM_malloc(L, count);
    randombytes_buf(buf, count);
    lua_pushlstring(L, buf, count);
    luaM_freemem(L, buf, count);
  }
  return 1;
}

// See https://download.libsodium.org/doc/public-key_cryptography/sealed_boxes

static int l_crypto_box_keypair(lua_State *L)
{
  check_init(L);
  unsigned char pk[crypto_box_PUBLICKEYBYTES];
  unsigned char sk[crypto_box_SECRETKEYBYTES];

  int err = crypto_box_keypair(pk, sk);
  if (err) {
    return luaL_error(L, "crypto_box_keypair returned %d", err);
  }
  lua_pushlstring(L, (char *)pk, sizeof(pk));
  lua_pushlstring(L, (char *)sk, sizeof(sk));
  return 2;
}

static const uint8_t * get_pk(lua_State *L, int idx)
{
  check_init(L);
  size_t pk_len;
  const char *pk = luaL_checklstring(L, 2, &pk_len);
  if (pk_len != crypto_box_PUBLICKEYBYTES) {
    luaL_error(L, "Bad public key size!");
  }
  return (const uint8_t *)pk;
}

static const uint8_t * get_sk(lua_State *L, int idx)
{
  check_init(L);
  size_t sk_len;
  const char *sk = luaL_checklstring(L, idx, &sk_len);
  if (sk_len != crypto_box_SECRETKEYBYTES) {
    luaL_error(L, "Bad secret key size!");
  }
  return (const uint8_t *)sk;
}

static int l_crypto_box_seal(lua_State *L)
{
  check_init(L);
  size_t msg_len;
  const uint8_t *msg = (const uint8_t *)luaL_checklstring(L, 1, &msg_len);
  const uint8_t *pk = get_pk(L, 2);

  const size_t ciphertext_len = crypto_box_SEALBYTES + msg_len;
  uint8_t *ciphertext = (uint8_t *)luaM_malloc(L, ciphertext_len);

  int err = crypto_box_seal(ciphertext, msg, msg_len, pk);
  if (err) {
    luaM_freemem(L, ciphertext, ciphertext_len);
    return luaL_error(L, "crypto_box_seal returned %d", err);
  }
  lua_pushlstring(L, (char *)ciphertext, ciphertext_len);
  luaM_freemem(L, ciphertext, ciphertext_len);
  return 1;
}

static int l_crypto_box_seal_open(lua_State *L)
{
  check_init(L);
  size_t ciphertext_len;
  const uint8_t *ciphertext = (const uint8_t *)luaL_checklstring(L, 1, &ciphertext_len);
  const uint8_t *pk = get_pk(L, 2);
  const uint8_t *sk = get_sk(L, 3);

  const size_t decrypted_len = ciphertext_len - crypto_box_SEALBYTES;
  uint8_t *decrypted = (uint8_t *)luaM_malloc(L, decrypted_len);

  int err = crypto_box_seal_open(decrypted, ciphertext, ciphertext_len, pk, sk);
  if (err) {
    lua_pushnil(L);
  } else {
    lua_pushlstring(L, (char *)decrypted, decrypted_len);
  }
  luaM_freemem(L, decrypted, decrypted_len);
  return 1;
}

LROT_BEGIN(random, NULL, 0)
  LROT_FUNCENTRY(random,  l_randombytes_random)
  LROT_FUNCENTRY(uniform, l_randombytes_uniform)
  LROT_FUNCENTRY(buf,     l_randombytes_buf)
LROT_END(random, NULL, 0)

LROT_BEGIN(crypto_box, NULL, 0)
  LROT_FUNCENTRY(keypair,   l_crypto_box_keypair)
  LROT_FUNCENTRY(seal,      l_crypto_box_seal)
  LROT_FUNCENTRY(seal_open, l_crypto_box_seal_open)
LROT_END(crypto_box, NULL, 0)

LROT_BEGIN(sodium, NULL, 0)
  LROT_TABENTRY(random,     random)
  LROT_TABENTRY(crypto_box, crypto_box)
LROT_END(sodium, NULL, 0)

NODEMCU_MODULE(SODIUM, "sodium", sodium, NULL);
