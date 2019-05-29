/*
 * Number
 * Convert number to bytes
 *
 * author: feilongphone@gmail.com
 */
#include "lauxlib.h"
#include "module.h"

typedef unsigned char Byte;
typedef unsigned char *BytePoint;

typedef __int16_t int16;
typedef __int32_t int32;
typedef __int64_t int64;
typedef __uint16_t uint16;
typedef __uint32_t uint32;
typedef __uint64_t uint64;

static void __toNumber(lua_State *L, void *point, size_t length)
{
    const BytePoint numberPoint = (BytePoint)point;

    for (int i = 0; i < length; i++)
    {
        *(numberPoint + length - i - 1) = luaL_checknumber(L, i + 1);
    }
}

static void __toByte(lua_State *L, void *point, size_t length)
{
    const BytePoint numberPoint = (BytePoint)point - 1;

    for (int i = length; i > 0; i--)
    {
        lua_pushinteger(L, *(numberPoint + i));
    }
}

#define SIZETONUMBERNAME(TYPE) TYPE##ByteToNumber
#define TONUMBER(TYPE)                                \
    static int SIZETONUMBERNAME(TYPE)(lua_State * L)  \
    {                                                 \
        TYPE result = 0;                              \
                                                      \
        __toNumber(L, (void *)&result, sizeof(TYPE)); \
                                                      \
        lua_pushnumber(L, result);                    \
        return 1;                                     \
    }

#define SIZETOBYTENAME(TYPE) TYPE##ToByte
#define TOBYTE(TYPE)                                          \
    static int SIZETOBYTENAME(TYPE)(lua_State * L)            \
    {                                                         \
        const TYPE numberTemp = (TYPE)luaL_checknumber(L, 1); \
        __toByte(L, (void *)&numberTemp, sizeof(TYPE));       \
                                                              \
        return sizeof(TYPE);                                  \
    }

//////////////////////
TONUMBER(float)
TONUMBER(double)
TONUMBER(uint16)
TONUMBER(uint32)
TONUMBER(uint64)
TONUMBER(int16)
TONUMBER(int32)
TONUMBER(int64)
//////////////////////
TOBYTE(float)
TOBYTE(double)
TOBYTE(uint16)
TOBYTE(uint32)
TOBYTE(uint64)
TOBYTE(int16)
TOBYTE(int32)
TOBYTE(int64)
//////////////////////

#define LROT_FUNCENTRY_TONUMBER(NAME, TYPE) \
    LROT_FUNCENTRY(from##NAME##BytesToNumber, SIZETONUMBERNAME(TYPE))
#define LROT_FUNCENTRY_TOBYTE(NAME, TYPE) \
    LROT_FUNCENTRY(from##NAME##ToBytes, SIZETOBYTENAME(TYPE))

LROT_BEGIN(number)
LROT_FUNCENTRY_TONUMBER(Float, float)
LROT_FUNCENTRY_TONUMBER(Double, double)
LROT_FUNCENTRY_TONUMBER(Uint16, uint16)
LROT_FUNCENTRY_TONUMBER(Uint32, uint32)
LROT_FUNCENTRY_TONUMBER(Uint64, uint64)
LROT_FUNCENTRY_TONUMBER(Int16, int16)
LROT_FUNCENTRY_TONUMBER(Int32, int32)
LROT_FUNCENTRY_TONUMBER(Int64, int64)
/////////////////////
LROT_FUNCENTRY_TOBYTE(Float, float)
LROT_FUNCENTRY_TOBYTE(Double, double)
LROT_FUNCENTRY_TOBYTE(Uint16, uint16)
LROT_FUNCENTRY_TOBYTE(Uint32, uint32)
LROT_FUNCENTRY_TOBYTE(Uint64, uint64)
LROT_FUNCENTRY_TOBYTE(Int16, int16)
LROT_FUNCENTRY_TOBYTE(Int32, int32)
LROT_FUNCENTRY_TOBYTE(Int64, int64)
LROT_END(number, NULL, 0)

NODEMCU_MODULE(NUMBER, "number", number, NULL);
