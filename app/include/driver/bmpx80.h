//
//  cmd_bmpx80.h
//  From Sparkfun BMP180 library
//
//
//

#ifndef _cmd_bmpx80_h
#define _cmd_bmpx80_h
#include "c_types.h"
/*
	SFE_BMP180.h
	Bosch BMP180 pressure sensor library for the Arduino microcontroller
	Mike Grusin, SparkFun Electronics
	Uses floating-point equations from the Weather Station Data Logger project
	http://wmrx00.sourceforge.net/
	http://wmrx00.sourceforge.net/Arduino/BMP085-Calcs.pdf
	Forked from BMP085 library by M.Grusin
	version 1.0 2013/09/20 initial version
	
	Our example code uses the "beerware" license. You can do anything
	you like with this code. No really, anything. If you find it useful,
	buy me a (root) beer someday.
 */


    static int ICACHE_FLASH_ATTR
    BMPx80_begin();
    // call pressure.begin() to initialize BMP180 before use
    // returns 1 if success, 0 if failure (bad component or I2C bus shorted?)

    static int ICACHE_FLASH_ATTR
    BMPx80_calibrate();

    static int ICACHE_FLASH_ATTR
    BMPx80_startTemperature(void);
    // command BMP180 to start a temperature measurement
    // returns (number of ms to wait) for success, 0 for fail

    static int ICACHE_FLASH_ATTR
    BMPx80_getTemperature(double *T);
    // return temperature measurement from previous startTemperature command
    // places returned value in T variable (deg C)
    // returns 1 for success, 0 for fail

    static int ICACHE_FLASH_ATTR
    BMPx80_startPressure(char oversampling);
    // command BMP180 to start a pressure measurement
    // oversampling: 0 - 3 for oversampling value
    // returns (number of ms to wait) for success, 0 for fail

    static int ICACHE_FLASH_ATTR
    BMPx80_getPressure(double *P, double *T);
    // return absolute pressure measurement from previous startPressure command
    // note: requires previous temperature measurement in variable T
    // places returned value in P variable (mbar)
    // returns 1 for success, 0 for fail

    static double ICACHE_FLASH_ATTR
    BMPx80_sealevel(double P, double A);
    // convert absolute pressure to sea-level pressure (as used in weather data)
    // P: absolute pressure (mbar)
    // A: current altitude (meters)
    // returns sealevel pressure in mbar

    static double ICACHE_FLASH_ATTR
    BMPx80_altitude(double P, double P0);
    // convert absolute pressure to altitude (given baseline pressure; sea-level, runway, etc.)
    // P: absolute pressure (mbar)
    // P0: fixed baseline pressure (mbar)
    // returns signed altitude in meters

    static int ICACHE_FLASH_ATTR
    BMPx80_readByte(char address, u8_t *value);

    static int ICACHE_FLASH_ATTR
    BMPx80_readInt(char address, s16_t *value);
    // read an signed int (16 bits) from a BMP180 register
    // address: BMP180 register address
    // value: external signed int for returned value (16 bits)
    // returns 1 for success, 0 for fail, with result in value

    static int ICACHE_FLASH_ATTR
    BMPx80_readUInt(char address, u16_t *value);
    // read an unsigned int (16 bits) from a BMP180 register
    // address: BMP180 register address
    // value: external unsigned int for returned value (16 bits)
    // returns 1 for success, 0 for fail, with result in value

    static int ICACHE_FLASH_ATTR
    BMPx80_readBytes(unsigned char *values, char length);
    // read a number of bytes from a BMP180 register
    // values: array of char with register address in first location [0]
    // length: number of bytes to read back
    // returns 1 for success, 0 for fail, with read bytes in values[] array

    static int ICACHE_FLASH_ATTR
    BMPx80_writeBytes(unsigned char *values, char length);
    // write a number of bytes to a BMP180 register (and consecutive subsequent registers)
    // values: array of char with register address in first location [0]
    // length: number of bytes to write
    // returns 1 for success, 0 for fail
    



//#define BMP180_ADDR 0x77 // 7-bit address
#define BMP_ADDR 0x76 // 7-bit address

#define	BMP180_REG_CONTROL 0xF4
#define	BMP180_REG_RESULT 0xF6

#define	BMP180_COMMAND_TEMPERATURE 0x2E
#define	BMP180_COMMAND_PRESSURE0 0x34
#define	BMP180_COMMAND_PRESSURE1 0x74
#define	BMP180_COMMAND_PRESSURE2 0xB4
#define	BMP180_COMMAND_PRESSURE3 0xF4

#endif
