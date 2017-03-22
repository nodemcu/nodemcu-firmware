/*
 * Driver for TI Texas Instruments HDC1080 Temperature/Humidity Sensor.
 * Code By Metin KOC 
 * Sixfab Inc. metin@sixfab.com
 * Code based on ADXL345 driver.
 */
#include "module.h"
#include "lauxlib.h"
#include "platform.h"
#include "c_stdlib.h"
#include "c_string.h"
#include "c_math.h"

static const uint32_t hdc1080_i2c_id = 0;
static const uint8_t hdc1080_i2c_addr = 0x40;

#define HDC1080_CONFIG_REGISTER		0X02
#define HDC1080_TEMPERATURE_REGISTER	0X00
#define HDC1080_HUMIDITY_REGISTER	0X01


static int ICACHE_FLASH_ATTR hdc1080_init(lua_State* L) {

    uint32_t sda;
    uint32_t scl;
    uint8_t  devid;

    sda = luaL_checkinteger(L, 1);
    scl = luaL_checkinteger(L, 2);

    luaL_argcheck(L, sda > 0 && scl > 0, 1, "no i2c for D0");

    platform_i2c_setup(hdc1080_i2c_id, sda, scl, PLATFORM_I2C_SPEED_SLOW);
    
    // Configure Sensor
    platform_i2c_send_start(hdc1080_i2c_id);
    platform_i2c_send_address(hdc1080_i2c_id, hdc1080_i2c_addr, PLATFORM_I2C_DIRECTION_TRANSMITTER);
    platform_i2c_send_byte(hdc1080_i2c_id, HDC1080_CONFIG_REGISTER); 
    platform_i2c_send_byte(hdc1080_i2c_id, 0x00);
    platform_i2c_send_byte(hdc1080_i2c_id, 0x00);
    platform_i2c_send_stop(hdc1080_i2c_id);

    return 0;
}

static int ICACHE_FLASH_ATTR hdc1080_readTemperature(lua_State* L) {

    uint8_t data[2];
    int temp;
    int i;

    platform_i2c_send_start(hdc1080_i2c_id);
    platform_i2c_send_address(hdc1080_i2c_id, hdc1080_i2c_addr, PLATFORM_I2C_DIRECTION_TRANSMITTER);
    platform_i2c_send_byte(hdc1080_i2c_id, HDC1080_TEMPERATURE_REGISTER);
    platform_i2c_send_stop(hdc1080_i2c_id);

    os_delay_us(20000);

    platform_i2c_send_start(hdc1080_i2c_id);
    platform_i2c_send_address(hdc1080_i2c_id, hdc1080_i2c_addr, PLATFORM_I2C_DIRECTION_RECEIVER);

    for (i=0; i<2; i++) {
	data[i] = platform_i2c_recv_byte(hdc1080_i2c_id, 1);
    }

    platform_i2c_send_stop(hdc1080_i2c_id);

    temp = ((data[0]*256+data[1])/pow(2,16))*165-40;

    lua_pushinteger(L, temp);

    return 1;
}

static int ICACHE_FLASH_ATTR hdc1080_readHumidity(lua_State* L) {

    uint8_t data[2];
    int humidity;
    int i;

    platform_i2c_send_start(hdc1080_i2c_id);
    platform_i2c_send_address(hdc1080_i2c_id, hdc1080_i2c_addr, PLATFORM_I2C_DIRECTION_TRANSMITTER);
    platform_i2c_send_byte(hdc1080_i2c_id, HDC1080_HUMIDITY_REGISTER);
    platform_i2c_send_stop(hdc1080_i2c_id);

    os_delay_us(20000);

    platform_i2c_send_start(hdc1080_i2c_id);
    platform_i2c_send_address(hdc1080_i2c_id, hdc1080_i2c_addr, PLATFORM_I2C_DIRECTION_RECEIVER);

    for (i=0; i<2; i++) {
	data[i] = platform_i2c_recv_byte(hdc1080_i2c_id, 1);
    }

    platform_i2c_send_stop(hdc1080_i2c_id);

    humidity = ((data[0]*256+data[1])/pow(2,16))*100;

    lua_pushinteger(L, humidity);

    return 1;
}

static const LUA_REG_TYPE hdc1080_map[] = {
    { LSTRKEY( "readTemperature" ),         LFUNCVAL( hdc1080_readTemperature )},
    { LSTRKEY( "readHumidity" ),         LFUNCVAL( hdc1080_readHumidity )},
    { LSTRKEY( "init" ),         LFUNCVAL( hdc1080_init )},
    { LNILKEY, LNILVAL}
};

NODEMCU_MODULE(HDC1080, "hdc1080", hdc1080_map, NULL);
