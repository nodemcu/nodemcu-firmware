/*
 * Float
 * Convert float to bytes
 *
 * author: feilongphone@gmail.com
 */
#include "lauxlib.h"
#include "module.h"

typedef unsigned char Byte;
typedef unsigned char *BytePoint;

static void toNumber(lua_State *L, void *point, size_t length)
{
    const BytePoint numberPoint = (BytePoint)point;

    for (int i = 0; i < length; i++)
    {
        *(numberPoint + length - i - 1) = luaL_checknumber(L, i + 1);
    }
}

static int FloatByteToNumber(lua_State *L)
{
    float result = 0;

    toNumber(L, (void *)&result, sizeof(float));

    lua_pushnumber(L, result);
    return 1;
}

static int DoubleByteToNumber(lua_State *L)
{
    double result = 0;

    toNumber(L, (void *)&result, sizeof(double));

    lua_pushnumber(L, result);
    return 1;
}

static void toByte(lua_State *L, void *point, size_t length)
{
    const BytePoint numberPoint = (BytePoint)point - 1;

    for (int i = length; i > 0; i--)
    {
        lua_pushinteger(L, *(numberPoint + i));
    }
}

static int FloatToByte(lua_State *L)
{
    const lua_Number number = luaL_checknumber(L, 1);

    const float numberTemp = number;
    toByte(L, (void *)&numberTemp, sizeof(float));

    return sizeof(float);
}

static int DoubleToByte(lua_State *L)
{
    const lua_Number number = luaL_checknumber(L, 1);

    const double numberTemp = number;
    toByte(L, (void *)&numberTemp, sizeof(double));

    return sizeof(double);
}

LROT_BEGIN(float_point)
LROT_FUNCENTRY(fromFloatBytestoNumber, FloatByteToNumber)
LROT_FUNCENTRY(fromDoubleBytestoNumber, DoubleByteToNumber)
LROT_FUNCENTRY(fromFloattoByteArray, FloatToByte)
LROT_FUNCENTRY(fromDoubletoByteArray, DoubleToByte)
LROT_END(float_point, NULL, 0)

NODEMCU_MODULE(FLOAT, "float", float_point, NULL);
