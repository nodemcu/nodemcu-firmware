#include "module.h"
#include "lauxlib.h"
#include "platform.h"
#include "c_stdlib.h"
#include "c_string.h"

static const uint32_t bmp085_i2c_id = 0;
static const uint8_t bmp085_i2c_addr = 0x77;

static struct {
    int16_t  AC1;
    int16_t  AC2;
    int16_t  AC3;
    uint16_t AC4;
    uint16_t AC5;
    uint16_t AC6;
    int16_t  B1;
    int16_t  B2;
    int16_t  MB;
    int16_t  MC;
    int16_t  MD;
} bmp085_data;

static uint8_t r8u(uint32_t id, uint8_t reg) {
    uint8_t ret;

    platform_i2c_send_start(id);
    platform_i2c_send_address(id, bmp085_i2c_addr, PLATFORM_I2C_DIRECTION_TRANSMITTER);
    platform_i2c_send_byte(id, reg);
    platform_i2c_send_stop(id);
    platform_i2c_send_start(id);
    platform_i2c_send_address(id, bmp085_i2c_addr, PLATFORM_I2C_DIRECTION_RECEIVER);
    ret = platform_i2c_recv_byte(id, 0);
    platform_i2c_send_stop(id);
    return ret;
}

static uint16_t r16u(uint32_t id, uint8_t reg) {
    uint8_t high = r8u(id, reg);
    uint8_t low  = r8u(id, reg + 1);
    return (high << 8) | low;
}

static int16_t r16(uint32_t id, uint8_t reg) {
    return (int16_t) r16u(id, reg);
}

static int bmp085_setup(lua_State* L) {
    (void)L;

    bmp085_data.AC1 = r16(bmp085_i2c_id, 0xAA);
    bmp085_data.AC2 = r16(bmp085_i2c_id, 0xAC);
    bmp085_data.AC3 = r16(bmp085_i2c_id, 0xAE);
    bmp085_data.AC4 = r16u(bmp085_i2c_id, 0xB0);
    bmp085_data.AC5 = r16u(bmp085_i2c_id, 0xB2);
    bmp085_data.AC6 = r16u(bmp085_i2c_id, 0xB4);
    bmp085_data.B1  = r16(bmp085_i2c_id, 0xB6);
    bmp085_data.B2  = r16(bmp085_i2c_id, 0xB8);
    bmp085_data.MB  = r16(bmp085_i2c_id, 0xBA);
    bmp085_data.MC  = r16(bmp085_i2c_id, 0xBC);
    bmp085_data.MD  = r16(bmp085_i2c_id, 0xBE);

    return 0;
}

static uint32_t bmp085_temperature_raw_b5(void) {
    int16_t t, X1, X2;

    platform_i2c_send_start(bmp085_i2c_id);
    platform_i2c_send_address(bmp085_i2c_id, bmp085_i2c_addr, PLATFORM_I2C_DIRECTION_TRANSMITTER);
    platform_i2c_send_byte(bmp085_i2c_id, 0xF4);
    platform_i2c_send_byte(bmp085_i2c_id, 0x2E);
    platform_i2c_send_stop(bmp085_i2c_id);

    // Wait for device to complete sampling
    os_delay_us(4500);

    t = r16(bmp085_i2c_id, 0xF6);
    X1 = ((t - bmp085_data.AC6) * bmp085_data.AC5) >> 15;
    X2 = (bmp085_data.MC << 11)/ (X1 + bmp085_data.MD);

    return X1 + X2;
}

static int16_t bmp085_temperature(void) {
    return (bmp085_temperature_raw_b5() + 8) >> 4;
}

static int bmp085_lua_temperature(lua_State* L) {
    lua_pushinteger(L, bmp085_temperature());
    return 1;
}

static int32_t bmp085_pressure_raw(int oss) {
    int32_t p;
    int32_t p1, p2, p3;

    platform_i2c_send_start(bmp085_i2c_id);
    platform_i2c_send_address(bmp085_i2c_id, bmp085_i2c_addr, PLATFORM_I2C_DIRECTION_TRANSMITTER);
    platform_i2c_send_byte(bmp085_i2c_id, 0xF4);
    platform_i2c_send_byte(bmp085_i2c_id, 0x34 + 64 * oss);
    platform_i2c_send_stop(bmp085_i2c_id);

    // Wait for device to complete sampling
    switch (oss) {
        case 0: os_delay_us( 4500); break;
        case 1: os_delay_us( 7500); break;
        case 2: os_delay_us(13500); break;
        case 3: os_delay_us(25500); break;
    }

    p1 = r8u(bmp085_i2c_id, 0xF6);
    p2 = r8u(bmp085_i2c_id, 0xF7);
    p3 = r8u(bmp085_i2c_id, 0xF8);
    p = (p1 << 16) | (p2 << 8) | p3;
    p = p >> (8 - oss);

    return p;
}

static int bmp085_lua_pressure_raw(lua_State* L) {
    uint8_t oss = 0;
    int32_t p;

    if (lua_isnumber(L, 1)) {
        oss = luaL_checkinteger(L, 1);
        if (oss > 3)  {
            oss = 3;
        }
    }

    p = bmp085_pressure_raw(oss);
    lua_pushinteger(L, p);
    return 1;
}

static int bmp085_lua_pressure(lua_State* L) {
    uint8_t oss = 0;
    int32_t p;
    int32_t X1, X2, X3, B3, B4, B5, B6, B7;

    if (lua_isnumber(L, 1)) {
        oss = luaL_checkinteger(L, 1);
        if (oss > 3)  {
            oss = 3;
        }
    }

    p = bmp085_pressure_raw(oss);
    B5 = bmp085_temperature_raw_b5();

    B6 = B5 - 4000;
    X1 = ((int32_t)bmp085_data.B2 * ((B6 * B6) >> 12)) >> 11;
    X2 = ((int32_t)bmp085_data.AC2 * B6) >> 11;
    X3 = X1 + X2;
    B3 = ((((int32_t)bmp085_data.AC1 << 2) + X3) * (1 << oss) + 2) >> 2;
    X1 = ((int32_t)bmp085_data.AC3 * B6) >> 13;
    X2 = ((int32_t)bmp085_data.B1 * ((B6 * B6) >> 12)) >> 16;
    X3 = (X1 + X2 + 2) >> 2;
    B4 = ((int32_t)bmp085_data.AC4 * (X3 + 32768)) >> 15;
    B7 = (p - B3) * (50000 / (1 << oss));
    p  = (B7 / B4) << 1;
    X1 = (p >> 8) * (p >> 8);
    X1 = (X1 * 3038) >> 16;
    X2 = (-7357 * p) >> 16;
    p = p + ((X1 + X2 + 3791) >> 4);

    lua_pushinteger(L, p);
    return 1;
}

static const LUA_REG_TYPE bmp085_map[] = {
    { LSTRKEY( "temperature" ),  LFUNCVAL( bmp085_lua_temperature )},
    { LSTRKEY( "pressure" ),     LFUNCVAL( bmp085_lua_pressure )},
    { LSTRKEY( "pressure_raw" ), LFUNCVAL( bmp085_lua_pressure_raw )},
    { LSTRKEY( "setup" ),        LFUNCVAL( bmp085_setup )},
    { LNILKEY, LNILVAL}
};

NODEMCU_MODULE(BMP085, "bmp085", bmp085_map, NULL);
