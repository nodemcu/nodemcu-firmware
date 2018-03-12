/*
 * Driver for Honeywell HMC5883L compass IC
 *
 * Code based on BMP085 driver.
 */
#include "module.h"
#include "lauxlib.h"
#include "platform.h"
#include "c_stdlib.h"
#include "c_string.h"

static const uint32_t hmc5883_i2c_id = 0;
static const uint8_t hmc5883_i2c_addr = 0x1E;

static uint8_t r8u(uint32_t id, uint8_t reg) {
    uint8_t ret;

    platform_i2c_send_start(id);
    platform_i2c_send_address(id, hmc5883_i2c_addr, PLATFORM_I2C_DIRECTION_TRANSMITTER);
    platform_i2c_send_byte(id, reg);
    platform_i2c_send_stop(id);
    platform_i2c_send_start(id);
    platform_i2c_send_address(id, hmc5883_i2c_addr, PLATFORM_I2C_DIRECTION_RECEIVER);
    ret = platform_i2c_recv_byte(id, 0);
    platform_i2c_send_stop(id);
    return ret;
}

static void w8u(uint32_t id, uint8_t reg, uint8_t val) {
    platform_i2c_send_start(hmc5883_i2c_id);
    platform_i2c_send_address(hmc5883_i2c_id, hmc5883_i2c_addr, PLATFORM_I2C_DIRECTION_TRANSMITTER);
    platform_i2c_send_byte(hmc5883_i2c_id, reg);
    platform_i2c_send_byte(hmc5883_i2c_id, val);
    platform_i2c_send_stop(hmc5883_i2c_id);
}

static int hmc5883_setup(lua_State* L) {
    uint8_t  devid_a, devid_b, devid_c;

    devid_a = r8u(hmc5883_i2c_id, 10);
    devid_b = r8u(hmc5883_i2c_id, 11);
    devid_c = r8u(hmc5883_i2c_id, 12);

    if ((devid_a != 0x48) || (devid_b != 0x34) || (devid_c != 0x33)) {
        return luaL_error(L, "device not found");
    }

    // 8 sample average, 15 Hz update rate, normal measurement
    w8u(hmc5883_i2c_id, 0x00, 0x70);

    // Gain = 5
    w8u(hmc5883_i2c_id, 0x01, 0xA0);

    // Continuous measurement
    w8u(hmc5883_i2c_id, 0x02, 0x00);

    return 0;
}

static int hmc5883_read(lua_State* L) {

    uint8_t data[6];
    int x,y,z;
    int i;

    platform_i2c_send_start(hmc5883_i2c_id);
    platform_i2c_send_address(hmc5883_i2c_id, hmc5883_i2c_addr, PLATFORM_I2C_DIRECTION_TRANSMITTER);
    platform_i2c_send_byte(hmc5883_i2c_id, 0x03);
    platform_i2c_send_stop(hmc5883_i2c_id);
    platform_i2c_send_start(hmc5883_i2c_id);
    platform_i2c_send_address(hmc5883_i2c_id, hmc5883_i2c_addr, PLATFORM_I2C_DIRECTION_RECEIVER);

    for (i=0; i<5; i++) {
	data[i] = platform_i2c_recv_byte(hmc5883_i2c_id, 1);
    }
    
    data[5] = platform_i2c_recv_byte(hmc5883_i2c_id, 0);

    platform_i2c_send_stop(hmc5883_i2c_id);

    x = (int16_t) ((data[0] << 8) | data[1]);
    z = (int16_t) ((data[2] << 8) | data[3]);
    y = (int16_t) ((data[4] << 8) | data[5]);

    lua_pushinteger(L, x);
    lua_pushinteger(L, y);
    lua_pushinteger(L, z);

    return 3;
}

static const LUA_REG_TYPE hmc5883_map[] = {
    { LSTRKEY( "read" ),         LFUNCVAL( hmc5883_read )},
    { LSTRKEY( "setup" ),        LFUNCVAL( hmc5883_setup )},
    { LNILKEY, LNILVAL}
};

NODEMCU_MODULE(HMC5883L, "hmc5883l", hmc5883_map, NULL);
