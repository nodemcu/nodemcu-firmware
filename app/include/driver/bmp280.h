#ifndef _bmp280_h
#define _bmp280_h

#include "c_types.h"
/*
	BMP280.h
	Bosch BMP280 pressure sensor for nodemcu by @xlfe

	This code uses the "beerware" license. You can do anything
	you like with this code. No really, anything. If you find it useful,
	buy me a (ginger) beer someday.
 */

	//Setup the i2c bus and make sure we can communicate with the device and place it into the correct mode
    static int BMP280_init(u8_t sda, u8_t scl);

	static int BMP280_reset();
	static int BMP280_config();

	static int BMP280_readBytes (u8_t reg_addr, u8_t *dst, u8_t length);
	static int BMP280_writeBytes(u8_t reg_addr, u8_t *src, u8_t length);

	static int BMP280_get_calibration_data();

	static int _bmp_glow(u8_t direction, u16_t duration_ms);
//BMP address with SDO tied to ground is 0x76
//       and with SDO tied to Vddio  is 0x77
// 7-bit address

#define BMP280_ADDR 0x76

#define BMP280_CHIPID_ADDR 0xD0
#define BMP280_CHIPID_VAL 0x58

#define BMP280_RESET_ADDR 0xE0
#define BMP280_RESET_VAL 0xB6

#define BMP280_CONFIG_START 0xF3
#define BMP280_CALIB_START 0x88
#define BMP280_DATA_START 0xF7

//us
#define BMP280_FORCED_SAMPLE_DELAY 10000


#define BMP280_MODE 1
#define BMP280_TOSAMP 1
#define BMP280_POSAMP 1

#define BMP280_CTRL_MEAS_ADDR 0xF4
#define BMP280_CTRL_MEAS_VAL (BMP280_MODE | BMP280_TOSAMP << 2 | BMP280_POSAMP << 5)

#define BMP280_I2C_BUS_ID 0

#endif
