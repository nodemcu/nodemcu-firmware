/*
 * Driver for STM L3G4200D three axis gyroscope
 *
 * Code based on BMP085 driver.
 */
#include "module.h"
#include "lauxlib.h"
#include "platform.h"
#include "c_stdlib.h"
#include "c_string.h"

static const uint32_t i2c_id = 0;
static const uint8_t i2c_addr = 0x69;

static uint8_t r8u(uint32_t id, uint8_t reg) {
    uint8_t ret;

    platform_i2c_send_start(id);
    platform_i2c_send_address(id, i2c_addr, PLATFORM_I2C_DIRECTION_TRANSMITTER);
    platform_i2c_send_byte(id, reg);
    platform_i2c_send_stop(id);
    platform_i2c_send_start(id);
    platform_i2c_send_address(id, i2c_addr, PLATFORM_I2C_DIRECTION_RECEIVER);
    ret = platform_i2c_recv_byte(id, 0);
    platform_i2c_send_stop(id);
    return ret;
}

static void w8u(uint32_t id, uint8_t reg, uint8_t val) {
    platform_i2c_send_start(i2c_id);
    platform_i2c_send_address(i2c_id, i2c_addr, PLATFORM_I2C_DIRECTION_TRANSMITTER);
    platform_i2c_send_byte(i2c_id, reg);
    platform_i2c_send_byte(i2c_id, val);
    platform_i2c_send_stop(i2c_id);
}

static int l3g4200d_setup(lua_State* L) {
    uint8_t  devid;

    devid = r8u(i2c_id, 0xF);

    if (devid != 0xD3) {
        return luaL_error(L, "device not found");
    }

    w8u(i2c_id, 0x20, 0xF);

    return 0;
}

static int l3g4200d_read(lua_State* L) {

    uint8_t data[6];
    int x,y,z;
    int i;

    platform_i2c_send_start(i2c_id);
    platform_i2c_send_address(i2c_id, i2c_addr, PLATFORM_I2C_DIRECTION_TRANSMITTER);
    platform_i2c_send_byte(i2c_id, 0xA8);
    platform_i2c_send_start(i2c_id);
    platform_i2c_send_address(i2c_id, i2c_addr, PLATFORM_I2C_DIRECTION_RECEIVER);

    for (i=0; i<5; i++) {
	data[i] = platform_i2c_recv_byte(i2c_id, 1);
    }
    
    data[5] = platform_i2c_recv_byte(i2c_id, 0);

    platform_i2c_send_stop(i2c_id);

    x = (int16_t) ((data[1] << 8) | data[0]);
    y = (int16_t) ((data[3] << 8) | data[2]);
    z = (int16_t) ((data[5] << 8) | data[4]);

    lua_pushinteger(L, x);
    lua_pushinteger(L, y);
    lua_pushinteger(L, z);

    return 3;
}

static const LUA_REG_TYPE l3g4200d_map[] = {
    { LSTRKEY( "read" ),         LFUNCVAL( l3g4200d_read )},
    { LSTRKEY( "setup" ),        LFUNCVAL( l3g4200d_setup )},
    { LNILKEY, LNILVAL}
};

NODEMCU_MODULE(L3G4200D, "l3g4200d", l3g4200d_map, NULL);
