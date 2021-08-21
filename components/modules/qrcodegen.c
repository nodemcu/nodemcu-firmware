#include "module.h"
#include "lauxlib.h"
#include "common.h"
#include "lmem.h"

#include "qrcodegen.h"

// qrcodegen.encodeText(text, [{minver=int, maxver=int, ecl=int, mask=int, boostecl=bool}])
// all params except text are optional
static int encodeText(lua_State *L)
{
  lua_settop(L, 2);
  const char *text = luaL_checkstring(L, 1);

  int min_ver = opt_checkint_range(L, "minver", qrcodegen_VERSION_MIN, 
                                   qrcodegen_VERSION_MIN, qrcodegen_VERSION_MAX);

  int max_ver = opt_checkint_range(L, "maxver", qrcodegen_VERSION_MAX,
                                   min_ver, qrcodegen_VERSION_MAX);

  int ecl = opt_checkint_range(L, "ecl", qrcodegen_Ecc_LOW,
                               qrcodegen_Ecc_LOW, qrcodegen_Ecc_HIGH);

  int mask = opt_checkint_range(L, "mask", qrcodegen_Mask_AUTO,
                                qrcodegen_Mask_AUTO, qrcodegen_Mask_7);

  bool boost = opt_checkbool(L, "boostecl", false);

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

LROT_BEGIN(qrcodegen, NULL, 0)
  LROT_FUNCENTRY(encodeText, encodeText)
  LROT_FUNCENTRY(getSize,    getSize)
  LROT_FUNCENTRY(getPixel,   getPixel)
  LROT_NUMENTRY(LOW,         qrcodegen_Ecc_LOW)
  LROT_NUMENTRY(MEDIUM,      qrcodegen_Ecc_MEDIUM)
  LROT_NUMENTRY(QUARTILE,    qrcodegen_Ecc_QUARTILE)
  LROT_NUMENTRY(HIGH,        qrcodegen_Ecc_HIGH)
  LROT_NUMENTRY(AUTO,        qrcodegen_Mask_AUTO)
LROT_END(qrcodegen, NULL, 0)

NODEMCU_MODULE(QRCODEGEN, "qrcodegen", qrcodegen, NULL);
