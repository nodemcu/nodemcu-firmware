#include <limits.h>
#include <string.h>
#include "lauxlib.h"
#include "lmem.h"
#include "mbedtls/md5.h"
#include "mbedtls/sha1.h"
#include "mbedtls/sha256.h"
#include "mbedtls/sha512.h"
#include "module.h"
#include "platform.h"

#define HASH_METATABLE "crypto.sha1hasher"

typedef void (*hash_init_t)(void* ctx);
typedef int (*hash_starts_ret_t)(void* ctx);
typedef int (*hash_update_ret_t)(void* ctx, const unsigned char* input, size_t ilen);
typedef int (*hash_finish_ret_t)(void* ctx, unsigned char* output);
typedef void (*hash_free_t)(void* ctx);

typedef struct {
    const char* name;
    const size_t size;
    const size_t context_size;
    const hash_init_t init;
    const hash_starts_ret_t starts;
    const hash_update_ret_t update;
    const hash_finish_ret_t finish;
    const hash_free_t free;
} algo_info_t;

typedef struct {
    void* mbedtls_context;
    const algo_info_t* ainfo;
} hash_context_t;

#ifdef CONFIG_CRYPTO_HASH_SHA256
static int sha256_starts_ret(mbedtls_sha256_context* ctx) {
    return mbedtls_sha256_starts_ret(ctx, false);
}
static int sha224_starts_ret(mbedtls_sha256_context* ctx) {
    return mbedtls_sha256_starts_ret(ctx, true);
}
#endif

#ifdef CONFIG_CRYPTO_HASH_SHA512
static int sha512_starts_ret(mbedtls_sha512_context* ctx) {
    return mbedtls_sha512_starts_ret(ctx, false);
}
static int sha384_starts_ret(mbedtls_sha512_context* ctx) {
    return mbedtls_sha512_starts_ret(ctx, true);
}
#endif

static const algo_info_t algorithms[] = {
#ifdef CONFIG_CRYPTO_HASH_SHA1
    {
        "SHA1",
        20,
        sizeof(mbedtls_sha1_context),
        (hash_init_t)mbedtls_sha1_init,
        (hash_starts_ret_t)mbedtls_sha1_starts_ret,
        (hash_update_ret_t)mbedtls_sha1_update_ret,
        (hash_finish_ret_t)mbedtls_sha1_finish_ret,
        (hash_free_t)mbedtls_sha1_free,
    },
#endif
#ifdef CONFIG_CRYPTO_HASH_SHA256
    {
        "SHA256",
        32,
        sizeof(mbedtls_sha256_context),
        (hash_init_t)mbedtls_sha256_init,
        (hash_starts_ret_t)sha256_starts_ret,
        (hash_update_ret_t)mbedtls_sha256_update_ret,
        (hash_finish_ret_t)mbedtls_sha256_finish_ret,
        (hash_free_t)mbedtls_sha256_free,
    },
    {
        "SHA224",
        32,
        sizeof(mbedtls_sha256_context),
        (hash_init_t)mbedtls_sha256_init,
        (hash_starts_ret_t)sha224_starts_ret,
        (hash_update_ret_t)mbedtls_sha256_update_ret,
        (hash_finish_ret_t)mbedtls_sha256_finish_ret,
        (hash_free_t)mbedtls_sha256_free,
    },
#endif
#ifdef CONFIG_CRYPTO_HASH_SHA512
    {
        "SHA512",
        64,
        sizeof(mbedtls_sha512_context),
        (hash_init_t)mbedtls_sha512_init,
        (hash_starts_ret_t)sha512_starts_ret,
        (hash_update_ret_t)mbedtls_sha512_update_ret,
        (hash_finish_ret_t)mbedtls_sha512_finish_ret,
        (hash_free_t)mbedtls_sha512_free,
    },
    {
        "SHA384",
        64,
        sizeof(mbedtls_sha512_context),
        (hash_init_t)mbedtls_sha512_init,
        (hash_starts_ret_t)sha384_starts_ret,
        (hash_update_ret_t)mbedtls_sha512_update_ret,
        (hash_finish_ret_t)mbedtls_sha512_finish_ret,
        (hash_free_t)mbedtls_sha512_free,
    },
#endif
#ifdef CONFIG_CRYPTO_HASH_MD5
    {
        "MD5",
        16,
        sizeof(mbedtls_md5_context),
        (hash_init_t)mbedtls_md5_init,
        (hash_starts_ret_t)mbedtls_md5_starts_ret,
        (hash_update_ret_t)mbedtls_md5_update_ret,
        (hash_finish_ret_t)mbedtls_md5_finish_ret,
        (hash_free_t)mbedtls_md5_free,
    },
#endif
};

const int NUM_ALGORITHMS = sizeof(algorithms) / sizeof(algo_info_t);

static int crypto_new_hash(lua_State* L) {
    const char* algo = luaL_checkstring(L, 1);
    const algo_info_t* ainfo = NULL;

    for (int i = 0; i < NUM_ALGORITHMS; i++) {
        if (strcasecmp(algo, algorithms[i].name) == 0) {
            ainfo = &algorithms[i];
            break;
        }
    }

    if (ainfo == NULL) {
        luaL_error(L, "Unsupported algorithm: %s", algo);  // returns
    }

    hash_context_t* phctx = (hash_context_t*)lua_newuserdata(L, sizeof(hash_context_t));
    luaL_getmetatable(L, HASH_METATABLE);
    lua_setmetatable(L, -2);
    phctx->ainfo = ainfo;
    phctx->mbedtls_context = luaM_malloc(L, ainfo->context_size);
    if (phctx->mbedtls_context == NULL) {
        luaL_error(L, "Out of memory allocating context");
    }

    ainfo->init(phctx->mbedtls_context);
    if (ainfo->starts(phctx->mbedtls_context) != 0) {
        luaL_error(L, "Error starting context");
    }
    return 1;
}

static int crypto_hash_update(lua_State* L) {
    hash_context_t* phctx = (hash_context_t*)luaL_checkudata(L, 1, HASH_METATABLE);
    size_t size;
    const unsigned char* input = (const unsigned char*)luaL_checklstring(L, 2, &size);

    if (phctx->ainfo->update(phctx->mbedtls_context, input, size) != 0) {
        luaL_error(L, "Error updating hash");
    }

    return 0;  // no return value
}

static int crypto_hash_finalize(lua_State* L) {
    hash_context_t* phctx = (hash_context_t*)luaL_checkudata(L, 1, HASH_METATABLE);
    unsigned char output[phctx->ainfo->size];
    if (phctx->ainfo->finish(phctx->mbedtls_context, output) != 0) {
        luaL_error(L, "Error finalizing hash");
    }
    lua_pushlstring(L, (const char*)output, phctx->ainfo->size);
    return 1;
}

static int crypto_hash_gc(lua_State* L) {
    hash_context_t* phctx = (hash_context_t*)luaL_checkudata(L, 1, HASH_METATABLE);
    if (phctx->mbedtls_context == NULL)
        return 0;

    phctx->ainfo->free(phctx->mbedtls_context);
    luaM_free(L, phctx->mbedtls_context);
    return 0;
}

const char crypto_hexbytes[] = "0123456789abcdef";
/**
  * encoded = crypto.toHex(raw)
  *
  *	Encodes raw binary string as hex string.
  */
static int crypto_hex_encode(lua_State* L) {
    size_t len;
    const char* msg = luaL_checklstring(L, 1, &len);
    char* out = (char*)luaM_malloc(L, len * 2);
    int i, j = 0;
    for (i = 0; i < len; i++) {
        out[j++] = crypto_hexbytes[msg[i] >> 4];
        out[j++] = crypto_hexbytes[msg[i] & 0x0F];
    }
    lua_pushlstring(L, out, len * 2);
    luaM_free(L, out);
    return 1;
}

static const LUA_REG_TYPE crypto_hasher_map[] = {
    {LSTRKEY("update"), LFUNCVAL(crypto_hash_update)},
    {LSTRKEY("finalize"), LFUNCVAL(crypto_hash_finalize)},
    {LSTRKEY("__gc"), LFUNCVAL(crypto_hash_gc)},
    {LSTRKEY("__index"), LROVAL(crypto_hasher_map)},
    {LNILKEY, LNILVAL}};

static const LUA_REG_TYPE crypto_map[] = {
    {LSTRKEY("new_hash"), LFUNCVAL(crypto_new_hash)},
    {LSTRKEY("toHex"), LFUNCVAL(crypto_hex_encode)},
    {LNILKEY, LNILVAL}};

int luaopen_crypto(lua_State* L) {
    luaL_rometatable(L, HASH_METATABLE, (void*)crypto_hasher_map);  // create metatable for crypto.hash

    return 0;
}

NODEMCU_MODULE(CRYPTO, "crypto", crypto_map, luaopen_crypto);
