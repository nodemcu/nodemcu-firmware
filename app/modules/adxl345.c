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

static uint8_t ICACHE_FLASH_ATTR r8u(uint32_t id, uint8_t reg) {
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

static int ICACHE_FLASH_ATTR adxl345_init(lua_State* L) {

    uint32_t sda;
    uint32_t scl;
    uint8_t  devid;

    sda = luaL_checkinteger(L, 1);
    scl = luaL_checkinteger(L, 2);

    luaL_argcheck(L, sda > 0 && scl > 0, 1, "no i2c for D0");

    platform_i2c_setup(adxl345_i2c_id, sda, scl, PLATFORM_I2C_SPEED_SLOW);
    
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

static int ICACHE_FLASH_ATTR adxl345_read(lua_State* L) {

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
    { LSTRKEY( "init" ),         LFUNCVAL( adxl345_init )},
    { LNILKEY, LNILVAL}
};

NODEMCU_MODULE(ADXL345, "adxl345", adxl345_map, NULL);
