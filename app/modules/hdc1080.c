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


#define HDC1080_TEMPERATURE_REGISTER	0X00
#define HDC1080_HUMIDITY_REGISTER	0X01
#define HDC1080_CONFIG_REGISTER		0X02


static int hdc1080_setup(lua_State* L) {

	// Configure Sensor
    platform_i2c_send_start(hdc1080_i2c_id);
    platform_i2c_send_address(hdc1080_i2c_id, hdc1080_i2c_addr, PLATFORM_I2C_DIRECTION_TRANSMITTER);
    platform_i2c_send_byte(hdc1080_i2c_id, HDC1080_CONFIG_REGISTER); 
    platform_i2c_send_byte(hdc1080_i2c_id, 0x05); //Bit[10] to 1 for 11 bit resolution , Set Bit[9:8] to 01 for 11 bit resolution.
    platform_i2c_send_byte(hdc1080_i2c_id, 0x00);
    platform_i2c_send_stop(hdc1080_i2c_id);
    
    return 0;
}

static int hdc1080_read(lua_State* L) {

    uint8_t data[2];
    
    #ifdef LUA_NUMBER_INTEGRAL
    	int temp;
    	int humidity;
    #else
    	float temp;
    	float humidity;
    #endif
    
    int i;

    platform_i2c_send_start(hdc1080_i2c_id);
    platform_i2c_send_address(hdc1080_i2c_id, hdc1080_i2c_addr, PLATFORM_I2C_DIRECTION_TRANSMITTER);
    platform_i2c_send_byte(hdc1080_i2c_id, HDC1080_TEMPERATURE_REGISTER);
    platform_i2c_send_stop(hdc1080_i2c_id);

    os_delay_us(7000);

    platform_i2c_send_start(hdc1080_i2c_id);
    platform_i2c_send_address(hdc1080_i2c_id, hdc1080_i2c_addr, PLATFORM_I2C_DIRECTION_RECEIVER);

    for (i=0; i<2; i++) {
		data[i] = platform_i2c_recv_byte(hdc1080_i2c_id, 1);
    }

    platform_i2c_send_stop(hdc1080_i2c_id);

	#ifdef LUA_NUMBER_INTEGRAL
    	temp = ((((data[0]<<8)|data[1])*165)>>16)-40;
    	lua_pushinteger(L, (int)temp);
    #else
    	temp = ((float)((data[0]<<8)|data[1])/(float)pow(2,16))*165.0f-40.0f;
    	lua_pushnumber(L, temp);
    #endif
    
    
    platform_i2c_send_start(hdc1080_i2c_id);
    platform_i2c_send_address(hdc1080_i2c_id, hdc1080_i2c_addr, PLATFORM_I2C_DIRECTION_TRANSMITTER);
    platform_i2c_send_byte(hdc1080_i2c_id, HDC1080_HUMIDITY_REGISTER);
    platform_i2c_send_stop(hdc1080_i2c_id);

    os_delay_us(7000);

    platform_i2c_send_start(hdc1080_i2c_id);
    platform_i2c_send_address(hdc1080_i2c_id, hdc1080_i2c_addr, PLATFORM_I2C_DIRECTION_RECEIVER);

    for (i=0; i<2; i++) {
		data[i] = platform_i2c_recv_byte(hdc1080_i2c_id, 1);
    }

    platform_i2c_send_stop(hdc1080_i2c_id);

	#ifdef LUA_NUMBER_INTEGRAL
    	humidity = ((((data[0]<<8)|data[1]))*100)>>16;
    	lua_pushinteger(L, (int)humidity);
    #else
    	humidity = ((float)((data[0]<<8)|data[1])/(float)pow(2,16))*100.0f;
    	lua_pushnumber(L, humidity);
    #endif
    
    return 2;
}

static const LUA_REG_TYPE hdc1080_map[] = {
    { LSTRKEY( "read"  ),        LFUNCVAL( hdc1080_read )},
    { LSTRKEY( "setup" ),        LFUNCVAL( hdc1080_setup )},
    { LNILKEY, LNILVAL}
};

NODEMCU_MODULE(HDC1080, "hdc1080", hdc1080_map, NULL);
