/*
 * Driver for Analog Devices ADXL345 accelerometer.
 *
 * Code based on BMP085 driver.
 */
#include "module.h"
#include "lauxlib.h"
#include "platform.h"
#include "c_stdlib.h"
#include "c_string.h"

static const uint32_t adxl345_i2c_id = 0;
static const uint8_t adxl345_i2c_addr = 0x53;

static uint8_t r8u(uint32_t id, uint8_t reg) {
    uint8_t ret;

    platform_i2c_send_start(id);
    platform_i2c_send_address(id, adxl345_i2c_addr, PLATFORM_I2C_DIRECTION_TRANSMITTER);
    platform_i2c_send_byte(id, reg);
    platform_i2c_send_stop(id);
    platform_i2c_send_start(id);
    platform_i2c_send_address(id, adxl345_i2c_addr, PLATFORM_I2C_DIRECTION_RECEIVER);
    ret = platform_i2c_recv_byte(id, 0);
    platform_i2c_send_stop(id);
    return ret;
}

static int adxl345_setup(lua_State* L) {
    uint8_t  devid;

    devid = r8u(adxl345_i2c_id, 0x00);

    if (devid != 229) {
        return luaL_error(L, "device not found");
    }

    // Enable sensor
    platform_i2c_send_start(adxl345_i2c_id);
    platform_i2c_send_address(adxl345_i2c_id, adxl345_i2c_addr, PLATFORM_I2C_DIRECTION_TRANSMITTER);
    platform_i2c_send_byte(adxl345_i2c_id, 0x2D);
    platform_i2c_send_byte(adxl345_i2c_id, 0x08);
    platform_i2c_send_stop(adxl345_i2c_id);

    return 0;
}

static int adxl345_read(lua_State* L) {

    uint8_t data[6];
    int x,y,z;
    int i;

    platform_i2c_send_start(adxl345_i2c_id);
    platform_i2c_send_address(adxl345_i2c_id, adxl345_i2c_addr, PLATFORM_I2C_DIRECTION_TRANSMITTER);
    platform_i2c_send_byte(adxl345_i2c_id, 0x32);
    platform_i2c_send_start(adxl345_i2c_id);
    platform_i2c_send_address(adxl345_i2c_id, adxl345_i2c_addr, PLATFORM_I2C_DIRECTION_RECEIVER);

    for (i=0; i<5; i++) {
	data[i] = platform_i2c_recv_byte(adxl345_i2c_id, 1);
    }
    
    data[5] = platform_i2c_recv_byte(adxl345_i2c_id, 0);

    platform_i2c_send_stop(adxl345_i2c_id);

    x = (int16_t) ((data[1] << 8) | data[0]);
    y = (int16_t) ((data[3] << 8) | data[2]);
    z = (int16_t) ((data[5] << 8) | data[4]);

    lua_pushinteger(L, x);
    lua_pushinteger(L, y);
    lua_pushinteger(L, z);

    return 3;
}

static const LUA_REG_TYPE adxl345_map[] = {
    { LSTRKEY( "read" ),         LFUNCVAL( adxl345_read )},
    { LSTRKEY( "setup" ),        LFUNCVAL( adxl345_setup )},
    { LNILKEY, LNILVAL}
};

NODEMCU_MODULE(ADXL345, "adxl345", adxl345_map, NULL);
