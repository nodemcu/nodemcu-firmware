#define MBEDTLS_SHA1_ALT
#include <limits.h>
#include <string.h>
#include "lauxlib.h"
#include "lmem.h"
#include "mbedtls/sha1.h"
#include "module.h"

#define SHA1_METATABLE "crypto.sha1hasher"

static int crypto_test(lua_State* L) {
    lua_Integer v = luaL_checkinteger(L, 1);

    lua_pushinteger(L, v * 2);
    return 1;
}

static int crypto_sha1test(lua_State* L) {
    size_t size;
    const unsigned char* input = (const unsigned char*)luaL_checklstring(L, 1, &size);
    unsigned char output[20];
    int ret;

    mbedtls_sha1_context* ctx = (mbedtls_sha1_context*)luaM_malloc(L, sizeof(mbedtls_sha1_context));

    mbedtls_sha1_init(ctx);

    if ((ret = mbedtls_sha1_starts_ret(ctx)) != 0)
        goto fail;

    if ((ret = mbedtls_sha1_update_ret(ctx, input, size)) != 0)
        goto fail;

    if ((ret = mbedtls_sha1_finish_ret(ctx, output)) != 0)
        goto fail;

    lua_pushlstring(L, (const char*)output, 20);
    goto exit;
fail:
    lua_pushnil(L);
exit:
    mbedtls_sha1_free(ctx);
    luaM_free(L, ctx);

    return 1;
}

static int crypto_new_hash(lua_State* L) {
    const char* algo = luaL_checkstring(L, 1);
    if (strcmp("SHA1", algo) != 0) {
        luaL_error(L, "Unsupported algorithm: %s", algo);  // returns
    }
    printf("before ud\n");
    /* create a userdatum to store a hashing context address*/
    mbedtls_sha1_context** ppctx = (mbedtls_sha1_context**)lua_newuserdata(L, sizeof(mbedtls_sha1_context*));
    /* set its metatable */
    luaL_getmetatable(L, SHA1_METATABLE);
    lua_setmetatable(L, -2);

    printf("before malloc\n");
    *ppctx = (mbedtls_sha1_context*)luaM_malloc(L, sizeof(mbedtls_sha1_context));
    if (*ppctx == NULL) {
        luaL_error(L, "Out of memory allocating context");
    }
    printf("before sha init\n");
    mbedtls_sha1_init(*ppctx);
    printf("before starts\n");
    if (!mbedtls_sha1_starts_ret(*ppctx)) {
        luaL_error(L, "Error starting sha1 context");
    }
    printf("before return\n");
    return 1;
}

static int crypto_sha1_update(lua_State* L) {
    mbedtls_sha1_context* pctx = (mbedtls_sha1_context*)luaL_checkudata(L, 1, SHA1_METATABLE);
    size_t size;
    const unsigned char* input = (const unsigned char*)luaL_checklstring(L, 2, &size);

    if (!mbedtls_sha1_update_ret(pctx, input, size)) {
        luaL_error(L, "Error updating hash");
    }

    return 0;  // no return value
}

static int crypto_sha1_finalize(lua_State* L) {
    mbedtls_sha1_context* pctx = (mbedtls_sha1_context*)luaL_checkudata(L, 1, SHA1_METATABLE);
    unsigned char output[20];
    if (!mbedtls_sha1_finish_ret(pctx, output)) {
        luaL_error(L, "Error finalizing hash");
    }
    lua_pushlstring(L, (const char*)output, 20);
    return 1;
}

static int crypto_sha1_gc(lua_State* L) {
    mbedtls_sha1_context* pctx = (mbedtls_sha1_context*)luaL_checkudata(L, 1, SHA1_METATABLE);
    if (!pctx)
        return 0;

    mbedtls_sha1_free(pctx);
    luaM_free(L, pctx);
    return 0;
}

int luaopen_crypto(lua_State* L) {
    luaL_newmetatable(L, SHA1_METATABLE);

    /* set its __gc field */
    lua_pushstring(L, "__gc");
    lua_pushcfunction(L, crypto_sha1_gc);
    lua_settable(L, -3);

    lua_pushstring(L, "update");
    lua_pushcfunction(L, crypto_sha1_update);
    lua_settable(L, -3);

    lua_pushstring(L, "finalize");
    lua_pushcfunction(L, crypto_sha1_finalize);
    lua_settable(L, -3);

    return 0;
}

static const LUA_REG_TYPE crypto_map[] = {
    {LSTRKEY("test"), LFUNCVAL(crypto_test)},
    {LSTRKEY("sha1test"), LFUNCVAL(crypto_sha1test)},
    {LSTRKEY("new_hash"), LFUNCVAL(crypto_new_hash)},
    {LNILKEY, LNILVAL}};

NODEMCU_MODULE(CRYPTO, "crypto", crypto_map, luaopen_crypto);
