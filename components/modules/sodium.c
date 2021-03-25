#include "module.h"
#include "lauxlib.h"
#include "lmem.h"

#include "sodium_module.h"
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

// See https://download.libsodium.org/doc/hashing/generic_hashing

static void get_key_and_outlen(lua_State* L, int idx, const uint8_t **key, size_t* key_len, int* out_len)
{
  *key = (const uint8_t *)luaL_optlstring(L, idx, NULL, key_len);
  if (*key && (*key_len < crypto_generichash_KEYBYTES_MIN || *key_len > crypto_generichash_KEYBYTES_MAX)) {
    luaL_error(L, "key length must be between %d and %d",
      crypto_generichash_KEYBYTES_MIN,
      crypto_generichash_KEYBYTES_MAX);
  }

  *out_len = luaL_optint(L, idx + 1, crypto_generichash_BYTES);
  if (*out_len < crypto_generichash_BYTES_MIN || *out_len > crypto_generichash_BYTES_MAX) {
    luaL_error(L, "outlen must be between %d and %d",
      crypto_generichash_BYTES_MIN,
      crypto_generichash_BYTES_MAX);
  }
}

// sodium.generichash(data, [key [, outlen]])
int l_sodium_generichash(lua_State* L)
{
  check_init(L);
  size_t data_len;
  const uint8_t *data = (const uint8_t *)luaL_checklstring(L, 1, &data_len);
  size_t key_len;
  const uint8_t *key;
  int out_len;
  get_key_and_outlen(L, 2, &key, &key_len, &out_len);
  
  uint8_t out[crypto_generichash_BYTES_MAX];
  int err = crypto_generichash(out, out_len, data, data_len, key, key_len);
  if (err) {
    lua_pushnil(L);
  } else {
    lua_pushlstring(L, (char *)out, out_len);
  }
  return 1;
}

static const char generichash_state_mt[] = "sodium.hashstate";

// Bah why does crypto_generichash_state not store outlen?
typedef struct
{
  crypto_generichash_state crypto_state;
  int out_len;
} sodium_generichash_state;

// sodium.generichash_init([key [, outlen]])
int l_sodium_generichash_init(lua_State* L)
{
  check_init(L);
  size_t key_len;
  const uint8_t *key;
  int out_len;
  get_key_and_outlen(L, 1, &key, &key_len, &out_len);

  sodium_generichash_state *state = (sodium_generichash_state *)lua_newuserdata(L, sizeof(sodium_generichash_state));
  luaL_getmetatable(L, generichash_state_mt);
  lua_setmetatable(L, -2);

  int err = crypto_generichash_init(&state->crypto_state, key, key_len, out_len);
  if (err) {
    return luaL_error(L, "crypto_generichash_init failed with %d", err);
  }
  state->out_len = out_len;
  return 1;
}

static int l_sodium_generichash_update(lua_State* L)
{
  sodium_generichash_state *state = (sodium_generichash_state *)luaL_checkudata(L, 1, generichash_state_mt);
  size_t data_len;
  const uint8_t* data = (const uint8_t*)luaL_checklstring(L, 2, &data_len);
  crypto_generichash_update(&state->crypto_state, data, data_len);
  return 0;
}

static int l_sodium_generichash_final(lua_State* L)
{
  sodium_generichash_state *state = (sodium_generichash_state *)luaL_checkudata(L, 1, generichash_state_mt);
  uint8_t out[crypto_generichash_BYTES_MAX];
  int err = crypto_generichash_final(&state->crypto_state, out, state->out_len);
  if (err) {
    return luaL_error(L, "crypto_generichash_final failed with %d", err);
  }
  lua_pushlstring(L, (char *)out, state->out_len);
  return 1;
}

LROT_BEGIN(random)
  LROT_FUNCENTRY(random,  l_randombytes_random)
  LROT_FUNCENTRY(uniform, l_randombytes_uniform)
  LROT_FUNCENTRY(buf,     l_randombytes_buf)
LROT_END(random, NULL, 0)

LROT_BEGIN(crypto_box)
  LROT_FUNCENTRY(keypair,   l_crypto_box_keypair)
  LROT_FUNCENTRY(seal,      l_crypto_box_seal)
  LROT_FUNCENTRY(seal_open, l_crypto_box_seal_open)
LROT_END(crypto_box, NULL, 0)

LROT_BEGIN(sodium)
  LROT_TABENTRY(random,     random)
  LROT_TABENTRY(crypto_box, crypto_box)
  LROT_FUNCENTRY(generichash, l_sodium_generichash)
  LROT_FUNCENTRY(generichash_init, l_sodium_generichash_init)
LROT_END(sodium, NULL, 0)

LROT_BEGIN(generichash_state)
  LROT_FUNCENTRY(update, l_sodium_generichash_update)
  LROT_FUNCENTRY(final, l_sodium_generichash_final)
  LROT_FUNCENTRY(finalize, l_sodium_generichash_final) // for compat with crypto hasher API
  LROT_TABENTRY(__index, generichash_state)
LROT_END(generichash_state, NULL, 0)

static int luaopen_sodium(lua_State *L)
{
  luaL_rometatable(L, generichash_state_mt, (void *)generichash_state_map);
  return 0;
}

NODEMCU_MODULE(SODIUM, "sodium", sodium, luaopen_sodium);
