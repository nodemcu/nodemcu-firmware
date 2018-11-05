#include "module.h"
#include "lauxlib.h"
#include "lmem.h"

#include "qrcodegen.h"

// qrcodegen.encodeText(text, minVer, maxVer, ecl, mask, boostEcl)
// all params except text are optional
static int encodeText(lua_State *L)
{
  const char *text = luaL_checkstring(L, 1);

  int min_ver = luaL_optint(L, 2, 1);
  luaL_argcheck(L, min_ver >= qrcodegen_VERSION_MIN && min_ver <= qrcodegen_VERSION_MAX,
                2, "minVer must be 1-40");

  int max_ver = luaL_optint(L, 3, 40);
  luaL_argcheck(L, max_ver >= min_ver && max_ver <= qrcodegen_VERSION_MAX,
                3, "maxVer must be 1-40");

  int ecl = luaL_optint(L, 4, qrcodegen_Ecc_LOW);
  int mask = luaL_optint(L, 5, qrcodegen_Mask_AUTO);
  bool boost = lua_toboolean(L, 6);

  const size_t buf_len = qrcodegen_BUFFER_LEN_FOR_VERSION(max_ver);
  uint8_t *result = (uint8_t *)luaM_malloc(L, buf_len * 2); 
  uint8_t *tempbuf = result + buf_len;

  bool ok = qrcodegen_encodeText(text, tempbuf, result, ecl, min_ver, max_ver, mask, boost);
  if (ok) {
    lua_pushlstring(L, (const char *)result, buf_len);
  } else {
    lua_pushnil(L);
  }
  luaM_freemem(L, result, buf_len * 2);
  return 1;
}

static int getSize(lua_State *L)
{
  const uint8_t* data = (const uint8_t *)luaL_checkstring(L, 1);
  lua_pushinteger(L, qrcodegen_getSize(data));
  return 1;
}

static int getPixel(lua_State *L)
{
  const uint8_t* data = (const uint8_t *)luaL_checkstring(L, 1);
  int x = luaL_checkint(L, 2);
  int y = luaL_checkint(L, 3);
  lua_pushboolean(L, qrcodegen_getModule(data, x, y));
  return 1;
}

static const LUA_REG_TYPE qrcodegen_map[] = {
  { LSTRKEY("encodeText"), LFUNCVAL(encodeText) },
  { LSTRKEY("getSize"), LFUNCVAL(getSize) },
  { LSTRKEY("getPixel"), LFUNCVAL(getPixel) },
  { LSTRKEY("LOW"), LNUMVAL(qrcodegen_Ecc_LOW) },
  { LSTRKEY("MEDIUM"), LNUMVAL(qrcodegen_Ecc_MEDIUM) },
  { LSTRKEY("QUARTILE"), LNUMVAL(qrcodegen_Ecc_QUARTILE) },
  { LSTRKEY("HIGH"), LNUMVAL(qrcodegen_Ecc_HIGH) },
  { LSTRKEY("AUTO"), LNUMVAL(qrcodegen_Mask_AUTO) },
  { LNILKEY, LNILVAL }
};

NODEMCU_MODULE(QRCODEGEN, "qrcodegen", qrcodegen_map, NULL);
