/*
 * Adaptation of Sparkfun's BMP180 code form https://github.com/sparkfun/BMP180_Breakout
 */
#include "c_stdlib.h"
#include "lualib.h"
#include "ets_sys.h"
#include "os_type.h"
#include "mem.h"
#include "osapi.h"
#include "user_interface.h"

#include "espconn.h"
#include "driver/i2c_master.h"

#include <stdlib.h>

#include "driver/bmpx80.h"
//#include <stdio.h>
#include "c_types.h"
#include "c_string.h"

#include "c_math.h"
#include "lauxlib.h"
#include "auxmods.h"
#include "lrotable.h"
#include "platform.h"

double _y0,_y1,x0,x1,x2,y2,p2,p1,p0,c5,c6,mc,md;
u8_t bmp_id =0;
#define BMP_SDA 4
#define BMP_SCL 5
#define BMP280 1

static int ICACHE_FLASH_ATTR
        BMPx80_begin() {
    platform_i2c_setup(bmp_id, BMP_SDA, BMP_SCL, PLATFORM_I2C_SPEED_SLOW);
    return 1;
}

static double ICACHE_FLASH_ATTR bmp_pow(double a,double b) {
    return pow(a,b);
}


static int ICACHE_FLASH_ATTR
        BMPx80_calibrate() {


//    #ifdef BMP280
//        u8_t chipid;
//        if (BMPx80_readByte(0xD0,&chipid)) {
//            if (chipid == 0x58) {
//                return 1;
//            }
//            return 0;
//        }
//    #else

    double c3, c4, b1;
    s16_t AC1, AC2, AC3, VB1, VB2, MB, MC, MD;
    u16_t AC4, AC5, AC6;

    if (
        BMPx80_readInt(0xAA,&AC1) &&
        BMPx80_readInt(0xAC,&AC2) &&
        BMPx80_readInt(0xAE,&AC3) &&
        BMPx80_readUInt(0xB0,&AC4) &&
        BMPx80_readUInt(0xB2,&AC5) &&
        BMPx80_readUInt(0xB4,&AC6) &&
        BMPx80_readInt(0xB6,&VB1) &&
        BMPx80_readInt(0xB8,&VB2) &&
        BMPx80_readInt(0xBA,&MB) &&
        BMPx80_readInt(0xBC,&MC) &&
        BMPx80_readInt(0xBE,&MD)
    ){

        // All reads completed successfully!
        // Compute floating-point polynominals:
        
        c3 = 160.0 * bmp_pow(2,-15) * AC3;
        c4 = bmp_pow(10,-3) * bmp_pow(2,-15) * AC4;
        b1 = bmp_pow(160,2) * bmp_pow(2,-30) * VB1;
        c5 = (bmp_pow(2,-15) / 160) * AC5;
        c6 = AC6;
        mc = (bmp_pow(2,11) / bmp_pow(160,2)) * MC;
        md = MD / 160.0;
        x0 = AC1;
        x1 = 160.0 * bmp_pow(2,-13) * AC2;
        x2 = bmp_pow(160,2) * bmp_pow(2,-25) * VB2;
        _y0 = c4 * bmp_pow(2,15);
        _y1 = c4 * c3;
        y2 = c4 * b1;
        p0 = (3791.0 - 8.0) / 1600.0;
        p1 = 1.0 - 7357.0 * bmp_pow(2,-20);
        p2 = 3038.0 * 100.0 * bmp_pow(2,-36);
        
        // Success!
        return(1);
    }
    else
    {
        // Error reading calibration data; bad component or connection?
        return(0);
    }

//    #endif
}


static int ICACHE_FLASH_ATTR
        BMPx80_readByte(char address, u8_t *value)
// Read a signed integer (one bytes) from device
// address: register to start reading (plus subsequent register)
// value: external variable to store data (function modifies value)
{

    unsigned char data[2];

    data[0] = address;
    if (BMPx80_readBytes(data,1))
    {
        value[0] = ((u8_t)data[0]);
        return(1);
    }
    return(0);
}



static int ICACHE_FLASH_ATTR
        BMPx80_readInt(char address, s16_t *value)
// Read a signed integer (two bytes) from device
// address: register to start reading (plus subsequent register)
// value: external variable to store data (function modifies value)
{

    unsigned char data[2];

    data[0] = address;
    if (BMPx80_readBytes(data,2))
    {

        value = (s16_t*)(data[0]<<8 | data[1]);
        //if (*value & 0x8000) *value |= 0xFFFF0000; // sign extend if negative
        return(1);
    }
    return(0);
}



static int ICACHE_FLASH_ATTR
    BMPx80_readUInt(char address, u16_t *value)
// Read an unsigned integer (two bytes) from device
// address: register to start reading (plus subsequent register)
// value: external variable to store data (function modifies value)
{
    unsigned char data[2];
    
    data[0] = address;
    if (BMPx80_readBytes(data,2))
    {

//        data[0] << 8
//        data[1]

        value = (u16_t*)( data[0]<<8 | data[1] );
        return(1);
    }
    value = 0;
    return(0);
}

//BMP address is 0x77


static int ICACHE_FLASH_ATTR
    BMPx80_readBytes(unsigned char *values, char length)
// Read an array of bytes from device
// values: external array to hold data. Put starting register in values[0].
// length: number of bytes to read
{
    char x,data;
    
    platform_i2c_send_start( bmp_id );
    platform_i2c_send_address( bmp_id, (u16) BMP_ADDR, PLATFORM_I2C_DIRECTION_TRANSMITTER);

    if( platform_i2c_send_byte( bmp_id, values[0] ) == 0 ) {
        return 0;
    }

//    platform_i2c_send_stop( bmp_id );
    platform_i2c_send_start( bmp_id );

    platform_i2c_send_address( bmp_id, (u16) BMP_ADDR, PLATFORM_I2C_DIRECTION_RECEIVER);

    for( x = 0; x < length; x ++ ) {
        values[x] = platform_i2c_recv_byte( bmp_id, x < length - 1 );
    }
    platform_i2c_send_stop( bmp_id );
    return 1;

}


static int ICACHE_FLASH_ATTR
    BMPx80_writeBytes(unsigned char *values, char length)
// Write an array of bytes to device
// values: external array of data to write. Put starting register in values[0].
// length: number of bytes to write
{
    char x,data;

    platform_i2c_send_start( bmp_id );
    platform_i2c_send_address( bmp_id, (u16) BMP_ADDR, PLATFORM_I2C_DIRECTION_TRANSMITTER);

    for( x = 0; x < length; x ++ ) {
        if (platform_i2c_send_byte(bmp_id, values[x]) == 0) {
            return 0;
        }
    }

    platform_i2c_send_stop( bmp_id );
    return 1;

}


static int ICACHE_FLASH_ATTR
    BMPx80_startTemperature(void)
// Begin a temperature reading.
// Will return delay in ms to wait, or 0 if I2C error
{
    unsigned char data[2], result;
    
    data[0] = BMP180_REG_CONTROL;
    data[1] = BMP180_COMMAND_TEMPERATURE;
    result = BMPx80_writeBytes(data, 2);
    if (result) // good write?
        return(5); // return the delay in ms (rounded up) to wait before retrieving data
    else
        return(0); // or return 0 if there was a problem communicating with the BMP
}


static int ICACHE_FLASH_ATTR
    BMPx80_getTemperature(double *T)
// Retrieve a previously-started temperature reading.
// Requires begin() to be called once prior to retrieve calibration parameters.
// Requires startTemperature() to have been called prior and sufficient time elapsed.
// T: external variable to hold result.
// Returns 1 if successful, 0 if I2C error.
{
    unsigned char data[2];
    char result;
    double tu, a;
    
    data[0] = BMP180_REG_RESULT;
    
    result = BMPx80_readBytes(data, 2);
    if (result) // good read, calculate temperature
    {
        tu = (data[0] * 256.0) + data[1];
        a = c5 * (tu - c6);
        T[0] = a + (mc / (a + md));
        
     }
    return(result);
}


static int ICACHE_FLASH_ATTR
    BMPx80_startPressure(char oversampling)
// Begin a pressure reading.
// Oversampling: 0 to 3, higher numbers are slower, higher-res outputs.
// Will return delay in ms to wait, or 0 if I2C error.
{
    unsigned char data[2], result, delay;
    
    data[0] = BMP180_REG_CONTROL;
    
    switch (oversampling)
    {
        case 0:
            data[1] = BMP180_COMMAND_PRESSURE0;
            delay = 5;
            break;
        case 1:
            data[1] = BMP180_COMMAND_PRESSURE1;
            delay = 8;
            break;
        case 2:
            data[1] = BMP180_COMMAND_PRESSURE2;
            delay = 14;
            break;
        case 3:
            data[1] = BMP180_COMMAND_PRESSURE3;
            delay = 26;
            break;
        default:
            data[1] = BMP180_COMMAND_PRESSURE0;
            delay = 5;
            break;
    }
    result = BMPx80_writeBytes(data, 2);
    if (result) // good write?
        return(delay); // return the delay in ms (rounded up) to wait before retrieving data
    else
        return(0); // or return 0 if there was a problem communicating with the BMP
}


static int ICACHE_FLASH_ATTR
    BMPx80_getPressure(double *P, double *T)
// Retrieve a previously started pressure reading, calculate abolute pressure in mbars.
// Requires begin() to be called once prior to retrieve calibration parameters.
// Requires startPressure() to have been called prior and sufficient time elapsed.
// Requires recent temperature reading to accurately calculate pressure.

// P: external variable to hold pressure.
// T: previously-calculated temperature.
// Returns 1 for success, 0 for I2C error.

// Note that calculated pressure value is absolute mbars, to compensate for altitude call sealevel().
{
    unsigned char data[3];
    char result;
    double pu,s,x,y,z;
    
    data[0] = BMP180_REG_RESULT;
    
    result = BMPx80_readBytes(data, 3);
    if (result) // good read, calculate pressure
    {
        pu = (data[0] * 256.0) + data[1] + (data[2]/256.0);
        
        //example from Bosch datasheet
        //pu = 23843;
        
        //example from http://wmrx00.sourceforge.net/Arduino/BMP085-Calcs.pdf, pu = 0x982FC0;
        //pu = (0x98 * 256.0) + 0x2F + (0xC0/256.0);
        
        s = T[0] - 25.0;
        x = (x2 * bmp_pow(s,2)) + (x1 * s) + x0;
        y = (y2 * bmp_pow(s,2)) + (_y1 * s) + _y0;
        z = (pu - x) / y;
        P[0] = (p2 * bmp_pow(z,2)) + (p1 * z) + p0;
        
    }
    return(result);
}


static double ICACHE_FLASH_ATTR
    BMPx80_sealevel(double P, double A)
// Given a pressure P (mb) taken at a specific altitude (meters),
// return the equivalent pressure (mb) at sea level.
// This produces pressure readings that can be used for weather measurements.
{
    return(P/bmp_pow(1-(A/44330.0),5.255));
}


static double ICACHE_FLASH_ATTR
    BMPx80_altitude(double P, double P0)
// Given a pressure measurement P (mb) and the pressure at a baseline P0 (mb),
// return altitude (meters) above baseline.
{
    return(44330.0*(1-bmp_pow(P/P0,1/5.255)));
}



// Lua: bmp.setup( id )
static int ICACHE_FLASH_ATTR bmp_setup( lua_State *L ) {

    if (BMPx80_begin() == 0) {
        return luaL_error( L, "Unable to init the BMPx80" );
    }

    if (BMPx80_calibrate() == 0){
        return luaL_error( L, "Unable to read calibration data from the BMPx80" );
    }

    lua_pushinteger(L,_y0);
    lua_pushinteger(L,_y1);

    return 2;
}

// Lua: bmp.sample()
static int ICACHE_FLASH_ATTR bmp_sample( lua_State *L ) {

    if(BMPx80_startTemperature()) {
        os_delay_us(50000);
    } else {
        return luaL_error( L, "Unable to start temp reading on BMPx80");
    }
    
    double T;
    if (BMPx80_getTemperature(&T)){
        lua_pushinteger(L,T);
        return 1;
    } else {
        return luaL_error(L,"Unable to read temp");
    }
    
    return;
    
}


// Module function map
#define MIN_OPT_LEVEL   2
#include "lrodefs.h"
const LUA_REG_TYPE bmp_map[] =
{
    { LSTRKEY( "setup" ),  LFUNCVAL(bmp_setup) },
    { LSTRKEY( "sample" ),  LFUNCVAL(bmp_sample) },

#if LUA_OPTIMIZE_MEMORY > 0

#endif
    { LNILKEY, LNILVAL }
};

LUALIB_API int ICACHE_FLASH_ATTR luaopen_bmp( lua_State *L )
{
#if LUA_OPTIMIZE_MEMORY > 0
    return 0;
#else // #if LUA_OPTIMIZE_MEMORY > 0
    luaL_register( L, AUXLIB_BMP, bmp_map );
    
    // Add the constants
    
    return 1;
#endif // #if LUA_OPTIMIZE_MEMORY > 0
}


