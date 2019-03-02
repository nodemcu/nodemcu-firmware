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

#define HASH_METATABLE "crypto.hasher"

// The following function typedefs aim to generalize mbedtls functions
// so that we can use the same code independent of what hashing algorithm
typedef void (*hash_init_t)(void* ctx);
typedef int (*hash_starts_ret_t)(void* ctx);
typedef int (*hash_update_ret_t)(void* ctx, const unsigned char* input, size_t ilen);
typedef int (*hash_finish_ret_t)(void* ctx, unsigned char* output);
typedef void (*hash_free_t)(void* ctx);

// algo_info_t describes a hashing algorithm and the mbedtls functions
// for initializing, hashing data, finalizing and freeing resources.
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

// hash_context_t contains information about an ongoing hash operation
typedef struct {
    void* mbedtls_context;
    const algo_info_t* ainfo;
} hash_context_t;

// if SHA256+SHA224 are enabled, the following two functions
// allow to call mbedtls appropriately depending on the algorithm
#ifdef CONFIG_CRYPTO_HASH_SHA256
static int sha256_starts_ret(mbedtls_sha256_context* ctx) {
    return mbedtls_sha256_starts_ret(ctx, false);  // false=SHA256
}
static int sha224_starts_ret(mbedtls_sha256_context* ctx) {
    return mbedtls_sha256_starts_ret(ctx, true);  // true=SHA224
}
#endif

// if SHA512+SHA384 are enabled, the following two functions
// allow to call mbedtls appropriately depending on the algorithm
#ifdef CONFIG_CRYPTO_HASH_SHA512
static int sha512_starts_ret(mbedtls_sha512_context* ctx) {
    return mbedtls_sha512_starts_ret(ctx, false);  // false=SHA512
}
static int sha384_starts_ret(mbedtls_sha512_context* ctx) {
    return mbedtls_sha512_starts_ret(ctx, true);  // true=SHA384
}
#endif

// the constant algorithms array below contains a table of functions and other
// information about each enabled hashing algorithm
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

//NUM_ALGORITHMS contains the actual number of enabled algorithms
const int NUM_ALGORITHMS = sizeof(algorithms) / sizeof(algo_info_t);

// crypto_new_hash (LUA: hasher = crypto.new_hash(algo)) allocates
// a hashing context for the requested algorithm
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

    // Instantiate a hasher object as a Lua userdata object
    // it will contain a pointer to a hash_context_t structure in which
    // we will store the mbedtls context information and also
    // what hashing algorithm this context is for.
    hash_context_t* phctx = (hash_context_t*)lua_newuserdata(L, sizeof(hash_context_t));
    luaL_getmetatable(L, HASH_METATABLE);
    lua_setmetatable(L, -2);

    phctx->ainfo = ainfo;                                          // save a pointer to the algorithm function table and information
    phctx->mbedtls_context = luaM_malloc(L, ainfo->context_size);  // make some space for the mbedtls context
    if (phctx->mbedtls_context == NULL) {
        luaL_error(L, "Out of memory allocating context");
    }

    ainfo->init(phctx->mbedtls_context);  // initialize the hashing function
    if (ainfo->starts(phctx->mbedtls_context) != 0) {
        luaL_error(L, "Error starting context");
    }
    return 1;  // one object returned, the hasher userdata object.
}

// crypto_hash_update (LUA: hasher:update(data)) submits data
// to be hashed.
static int crypto_hash_update(lua_State* L) {
    // retrieve the hashing context:
    hash_context_t* phctx = (hash_context_t*)luaL_checkudata(L, 1, HASH_METATABLE);

    size_t size;  // size of the input string
    // retrieve the input string:
    const unsigned char* input = (const unsigned char*)luaL_checklstring(L, 2, &size);

    // call the update hashing function:
    if (phctx->ainfo->update(phctx->mbedtls_context, input, size) != 0) {
        luaL_error(L, "Error updating hash");
    }

    return 0;  // no return value
}

// crypto_hash_finalize (LUA: hasher:finalize()) returns the hash result
// as a binary string.
static int crypto_hash_finalize(lua_State* L) {
    // retrieve the hashing context:
    hash_context_t* phctx = (hash_context_t*)luaL_checkudata(L, 1, HASH_METATABLE);

    // reserve some space to retrieve the output hash, according to the current algorithm
    unsigned char output[phctx->ainfo->size];

    // call the hash finish function to retrieve the result
    if (phctx->ainfo->finish(phctx->mbedtls_context, output) != 0) {
        luaL_error(L, "Error finalizing hash");
    }

    // pack the output into a lua string
    lua_pushlstring(L, (const char*)output, phctx->ainfo->size);

    return 1;  // 1 result returned, the hash.
}

// crypto_hash_gc is called automatically by LUA when the hasher object is
// dereferenced, in order to free resources associated with the hashing process.
static int crypto_hash_gc(lua_State* L) {
    // retrieve the hashing context:
    hash_context_t* phctx = (hash_context_t*)luaL_checkudata(L, 1, HASH_METATABLE);

    // if the mbedtls context is NULL, it means allocation failed in new_hash(), so nothing to do.
    if (phctx->mbedtls_context == NULL)
        return 0;

    // free mbedtls-related resources for this hash operation:
    phctx->ainfo->free(phctx->mbedtls_context);

    // free the memory allocated to store the mbedtls context:
    luaM_freemem(L, phctx->mbedtls_context, phctx->ainfo->context_size);
    return 0;
}

// The following table defines methods of the hasher object
static const LUA_REG_TYPE crypto_hasher_map[] = {
    {LSTRKEY("update"), LFUNCVAL(crypto_hash_update)},
    {LSTRKEY("finalize"), LFUNCVAL(crypto_hash_finalize)},
    {LSTRKEY("__gc"), LFUNCVAL(crypto_hash_gc)},
    {LSTRKEY("__index"), LROVAL(crypto_hasher_map)},
    {LNILKEY, LNILVAL}};

// This table defines the functions of the crypto module:
static const LUA_REG_TYPE crypto_map[] = {
    {LSTRKEY("new_hash"), LFUNCVAL(crypto_new_hash)},
    {LNILKEY, LNILVAL}};

// luaopen_crypto is the crypto module initialization function
int luaopen_crypto(lua_State* L) {
    luaL_rometatable(L, HASH_METATABLE, (void*)crypto_hasher_map);  // create metatable for crypto.hash

    return 0;
}

// define the crypto NodeMCU module
NODEMCU_MODULE(CRYPTO, "crypto", crypto_map, luaopen_crypto);
