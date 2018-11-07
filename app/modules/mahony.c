#include "../imufilters/mahony/mahony.h"

#include "module.h"
#include "lauxlib.h"
#include "platform.h"
#include "c_stdlib.h"
#include "c_string.h"

///// Mahony.h /////
static int luaMahonyUpdate(lua_State* L) {
    float gx = luaL_checknumber(L, 1);
    float gy = luaL_checknumber(L, 2);
    float gz = luaL_checknumber(L, 3);
    float ax = luaL_checknumber(L, 4);
    float ay = luaL_checknumber(L, 5);
    float az = luaL_checknumber(L, 6);
    float mx = luaL_checknumber(L, 7);
    float my = luaL_checknumber(L, 8);
    float mz = luaL_checknumber(L, 9);
    MahonyUpdate(gx, gy, gz, ax, ay, az, mx, my, mz);
    return 0;
}

static int luaMahonyUpdateIMU(lua_State* L) {
    float gx = luaL_checknumber(L, 1);
    float gy = luaL_checknumber(L, 2);
    float gz = luaL_checknumber(L, 3);
    float ax = luaL_checknumber(L, 4);
    float ay = luaL_checknumber(L, 5);
    float az = luaL_checknumber(L, 6);
    MahonyUpdateIMU(gx, gy, gz, ax, ay, az);
    return 0;
}

static int luaMahonyInit(lua_State* L) {
#define twoKpDef    (120.0f * 0.5f) // 2 * proportional gain
#define twoKiDef    (27.0f * 1.0f)  // 2 * integral gain

    float sampleFrequency = luaL_checknumber(L, 1);
    twoKi = twoKiDef;   // 2 * integral gain (Ki)
    qm0 = 1.0f;
    qm1 = 0.0f;
    qm2 = 0.0f;
    qm3 = 0.0f;
    integralFBx = 0.0f;
    integralFBy = 0.0f;
    integralFBz = 0.0f;
    invSampleFreq = 1.0f / sampleFrequency;
    return 0;
}

static int luaMahonyComputeAngles(lua_State* L) {
    float roll, pitch, yaw;
    MahonyComputeAngles(&roll, &pitch, &yaw);
    lua_pushnumber(L, roll * 57.29578f);
    lua_pushnumber(L, pitch * 57.29578f);
    lua_pushnumber(L, yaw * 57.29578f + 180.0f);
    return 3;
}

static int luaMahonyQuaternions(lua_State* L) {
    float q0, q1, q2, q3;
    MahonyQuaternions(&q0, &q1, &q2, &q3);
    lua_pushnumber(L, q0);
    lua_pushnumber(L, q1);
    lua_pushnumber(L, q2);
    lua_pushnumber(L, q3);
    return 4;
}

static const LUA_REG_TYPE mahony_map[] = {
        { LSTRKEY( "init" ),          LFUNCVAL( luaMahonyInit )},
        { LSTRKEY( "update" ),        LFUNCVAL( luaMahonyUpdate )},
        { LSTRKEY( "updateIMU" ),     LFUNCVAL( luaMahonyUpdateIMU )},
        { LSTRKEY( "computeAngles" ), LFUNCVAL( luaMahonyComputeAngles )},
        { LSTRKEY( "quaternions" ),   LFUNCVAL( luaMahonyQuaternions )},
        { LNILKEY, LNILVAL}
};

NODEMCU_MODULE(MAHONY, "mahony", mahony_map, NULL);
