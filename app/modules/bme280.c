// ***************************************************************************
// BMP280 module for ESP8266 with nodeMCU
// 
// Written by Lukas Voborsky, @voborsky
// 
// MIT license, http://opensource.org/licenses/MIT
// ***************************************************************************

//#define NODE_DEBUG

#include "module.h"
#include "lauxlib.h"
#include "platform.h"
#include "c_math.h"

/****************************************************/
/**\name	registers definition  */
/***************************************************/
#define BME280_REGISTER_CONTROL               (0xF4)
#define BME280_REGISTER_CONTROL_HUM           (0xF2)
#define BME280_REGISTER_CONFIG                (0xF5)
#define BME280_REGISTER_CHIPID                (0xD0)
#define BME280_REGISTER_VERSION               (0xD1)
#define BME280_REGISTER_SOFTRESET             (0xE0)
#define BME280_REGISTER_CAL26                 (0xE1)
#define BME280_REGISTER_PRESS                 (0xF7)	// 0xF7-0xF9
#define BME280_REGISTER_TEMP                  (0xFA)	// 0xFA-0xFC
#define BME280_REGISTER_HUM                   (0xFD)	// 0xFD-0xFE

#define BME280_REGISTER_DIG_T                 (0x88)	// 0x88-0x8D ( 6)
#define BME280_REGISTER_DIG_P                 (0x8E)	// 0x8E-0x9F (18)
#define BME280_REGISTER_DIG_H1                (0xA1)	// 0xA1      ( 1)
#define BME280_REGISTER_DIG_H2                (0xE1)	// 0xE1-0xE7 ( 7)
/****************************************************/
/**\name	I2C ADDRESS DEFINITIONS  */
/***************************************************/
#define BME280_I2C_ADDRESS1                  (0x76)
#define BME280_I2C_ADDRESS2                  (0x77)
/****************************************************/
/**\name	POWER MODE DEFINITIONS  */
/***************************************************/
/* Sensor Specific constants */
#define BME280_SLEEP_MODE                    (0x00)
#define BME280_FORCED_MODE                   (0x01)
#define BME280_NORMAL_MODE                   (0x03)
#define BME280_SOFT_RESET_CODE               (0xB6)
/****************************************************/
/**\name	OVER SAMPLING DEFINITIONS  */
/***************************************************/
#define BME280_OVERSAMP_1X                    (0x01)
#define BME280_OVERSAMP_2X                    (0x02)
#define BME280_OVERSAMP_4X                    (0x03)
#define BME280_OVERSAMP_8X                    (0x04)
#define BME280_OVERSAMP_16X                   (0x05)
/****************************************************/
/**\name	STANDBY TIME DEFINITIONS  */
/***************************************************/
#define BME280_STANDBY_TIME_1_MS              (0x00)
#define BME280_STANDBY_TIME_63_MS             (0x01)
#define BME280_STANDBY_TIME_125_MS            (0x02)
#define BME280_STANDBY_TIME_250_MS            (0x03)
#define BME280_STANDBY_TIME_500_MS            (0x04)
#define BME280_STANDBY_TIME_1000_MS           (0x05)
#define BME280_STANDBY_TIME_10_MS             (0x06)
#define BME280_STANDBY_TIME_20_MS             (0x07)
/****************************************************/
/**\name	FILTER DEFINITIONS  */
/***************************************************/
#define BME280_FILTER_COEFF_OFF               (0x00)
#define BME280_FILTER_COEFF_2                 (0x01)
#define BME280_FILTER_COEFF_4                 (0x02)
#define BME280_FILTER_COEFF_8                 (0x03)
#define BME280_FILTER_COEFF_16                (0x04)
/****************************************************/
/**\data type definition  */
/***************************************************/
#define BME280_S32_t int32_t
#define BME280_U32_t uint32_t
#define BME280_S64_t int64_t

#define BME280_SAMPLING_DELAY  113 //maximum measurement time in ms for maximum oversampling for all measures = 1.25 + 2.3*16 + 2.3*16 + 0.575 + 2.3*16 + 0.575 ms

// #define r16s(reg)  ((int16_t)r16u(reg))
// #define r16sLE(reg)  ((int16_t)r16uLE(reg))

// #define bme280_adc_P(void) r24u(BME280_REGISTER_PRESS)
// #define bme280_adc_T(void) r24u(BME280_REGISTER_TEMP)
// #define bme280_adc_H(void) r16u(BME280_REGISTER_HUM)

static const uint32_t bme280_i2c_id = 0;

static uint8_t bme280_i2c_addr = BME280_I2C_ADDRESS1;
static uint8_t bme280_isbme = 0; // 1 if the chip is BME280, 0 for BMP280
static uint8_t bme280_mode = 0; // stores oversampling settings
static uint8_t bme280_ossh = 0; // stores humidity oversampling settings
os_timer_t bme280_timer; // timer for forced mode readout
int lua_connected_readout_ref; // callback when readout is ready

static struct {
	uint16_t  dig_T1;
	int16_t   dig_T2;
	int16_t   dig_T3;
	uint16_t  dig_P1;
	int16_t   dig_P2;
	int16_t   dig_P3;
	int16_t   dig_P4;
	int16_t   dig_P5;
	int16_t   dig_P6;
	int16_t   dig_P7;
	int16_t   dig_P8;
	int16_t   dig_P9;
	uint8_t   dig_H1;
	int16_t   dig_H2;
	uint8_t   dig_H3;
	int16_t   dig_H4;
	int16_t   dig_H5;
	int8_t    dig_H6;
} bme280_data;

static BME280_S32_t bme280_t_fine;
static uint32_t bme280_h = 0;
static double bme280_hc = 1.0;

// return 0 if good
static int r8u_n(uint8_t reg, int n, uint8_t *buf) {
	int i;

	platform_i2c_send_start(bme280_i2c_id);
	platform_i2c_send_address(bme280_i2c_id, bme280_i2c_addr, PLATFORM_I2C_DIRECTION_TRANSMITTER);
	platform_i2c_send_byte(bme280_i2c_id, reg);
//	platform_i2c_send_stop(bme280_i2c_id);	// doco says not needed

	platform_i2c_send_start(bme280_i2c_id);
	platform_i2c_send_address(bme280_i2c_id, bme280_i2c_addr, PLATFORM_I2C_DIRECTION_RECEIVER);

	while (n-- > 0)
		*buf++ = platform_i2c_recv_byte(bme280_i2c_id, n > 0);
	platform_i2c_send_stop(bme280_i2c_id);

	return 0;
}

static uint8_t w8u(uint8_t reg, uint8_t val) {
	platform_i2c_send_start(bme280_i2c_id);
	platform_i2c_send_address(bme280_i2c_id, bme280_i2c_addr, PLATFORM_I2C_DIRECTION_TRANSMITTER);
	platform_i2c_send_byte(bme280_i2c_id, reg);
	platform_i2c_send_byte(bme280_i2c_id, val);
	platform_i2c_send_stop(bme280_i2c_id);
}

static uint8_t r8u(uint8_t reg) {
	uint8_t ret[1];
	r8u_n(reg, 1, ret);
	return ret[0];
}

// Returns temperature in DegC, resolution is 0.01 DegC. Output value of “5123” equals 51.23 DegC.  
// t_fine carries fine temperature as global value 
static BME280_S32_t bme280_compensate_T(BME280_S32_t adc_T) {
	BME280_S32_t var1, var2, T; 
	var1  = ((((adc_T>>3) - ((BME280_S32_t)bme280_data.dig_T1<<1))) * ((BME280_S32_t)bme280_data.dig_T2)) >> 11; 
	var2  = (((((adc_T>>4) - ((BME280_S32_t)bme280_data.dig_T1)) * ((adc_T>>4) - ((BME280_S32_t)bme280_data.dig_T1))) >> 12) *  
		((BME280_S32_t)bme280_data.dig_T3)) >> 14; 
	bme280_t_fine = var1 + var2; 
	T  = (bme280_t_fine * 5 + 128) >> 8; 
	return T; 
}

// Returns pressure in Pa as unsigned 32 bit integer in Q24.8 format (24 integer bits and 8 fractional bits). 
// Output value of “24674867” represents 24674867/256 = 96386.2 Pa = 963.862 hPa 
static BME280_U32_t bme280_compensate_P(BME280_S32_t adc_P) {
	BME280_S64_t var1, var2, p; 
	var1 = ((BME280_S64_t)bme280_t_fine) - 128000; 
	var2 = var1 * var1 * (BME280_S64_t)bme280_data.dig_P6; 
	var2 = var2 + ((var1*(BME280_S64_t)bme280_data.dig_P5)<<17); 
	var2 = var2 + (((BME280_S64_t)bme280_data.dig_P4)<<35); 
	var1 = ((var1 * var1 * (BME280_S64_t)bme280_data.dig_P3)>>8) + ((var1 * (BME280_S64_t)bme280_data.dig_P2)<<12); 
	var1 = (((((BME280_S64_t)1)<<47)+var1))*((BME280_S64_t)bme280_data.dig_P1)>>33; 
	if (var1 == 0) { 
		return 0; // avoid exception caused by division by zero 
	} 
	p = 1048576-adc_P; 
	p = (((p<<31)-var2)*3125)/var1; 
	var1 = (((BME280_S64_t)bme280_data.dig_P9) * (p>>13) * (p>>13)) >> 25; 
	var2 = (((BME280_S64_t)bme280_data.dig_P8) * p) >> 19; 
	p = ((p + var1 + var2) >> 8) + (((BME280_S64_t)bme280_data.dig_P7)<<4); 
	p = (p * 10) >> 8;
	return (BME280_U32_t)p; 
} 
 
// Returns humidity in %RH as unsigned 32 bit integer in Q22.10 format (22 integer and 10 fractional bits). 
// Output value of “47445” represents 47445/1024 = 46.333 %RH 
static BME280_U32_t bme280_compensate_H(BME280_S32_t adc_H) {
	BME280_S32_t v_x1_u32r; 

	v_x1_u32r = (bme280_t_fine - ((BME280_S32_t)76800)); 
	v_x1_u32r = (((((adc_H << 14) - (((BME280_S32_t)bme280_data.dig_H4) << 20) - (((BME280_S32_t)bme280_data.dig_H5) * v_x1_u32r)) + 
		((BME280_S32_t)16384)) >> 15) * (((((((v_x1_u32r * ((BME280_S32_t)bme280_data.dig_H6)) >> 10) * (((v_x1_u32r * 
		((BME280_S32_t)bme280_data.dig_H3)) >> 11) + ((BME280_S32_t)32768))) >> 10) + ((BME280_S32_t)2097152)) * 
		((BME280_S32_t)bme280_data.dig_H2) + 8192) >> 14)); 
	v_x1_u32r = (v_x1_u32r - (((((v_x1_u32r >> 15) * (v_x1_u32r >> 15)) >> 7) * ((BME280_S32_t)bme280_data.dig_H1)) >> 4)); 
	v_x1_u32r = (v_x1_u32r < 0 ? 0 : v_x1_u32r);  
	v_x1_u32r = (v_x1_u32r > 419430400 ? 419430400 : v_x1_u32r);   
	v_x1_u32r = v_x1_u32r>>12;
	return (BME280_U32_t)((v_x1_u32r * 1000)>>10); 
} 

static double ln(double x) {
	double y = (x-1)/(x+1);
	double y2 = y*y;
	double r = 0;
	for (int8_t i=33; i>0; i-=2) { //we've got the power
		r = 1.0/(double)i + y2 * r;
	}
	return 2*y*r;
}

static double bme280_qfe2qnh(int32_t qfe, int32_t h) {
	double hc;
	if (bme280_h == h) {
		hc = bme280_hc;
	} else {
		hc = pow((double)(1.0 - 2.25577e-5 * h), (double)(-5.25588));
		bme280_hc = hc; bme280_h = h;
	}
	double qnh = (double)qfe * hc;
	return qnh;
}

static int bme280_lua_setup(lua_State* L) {
	uint8_t config;
	uint8_t ack;
	uint8_t full_init;

	uint8_t const bit3 = 0b111;
	uint8_t const bit2 = 0b11;

	bme280_mode = (!lua_isnumber(L, 4)?BME280_NORMAL_MODE:(luaL_checkinteger(L, 4)&bit2)) // 4-th parameter: power mode
		| ((!lua_isnumber(L, 2)?BME280_OVERSAMP_16X:(luaL_checkinteger(L, 2)&bit3)) << 2) // 2-nd parameter: pressure oversampling
		| ((!lua_isnumber(L, 1)?BME280_OVERSAMP_16X:(luaL_checkinteger(L, 1)&bit3)) << 5); // 1-st parameter: temperature oversampling
		
	bme280_ossh = (!lua_isnumber(L, 3))?BME280_OVERSAMP_16X:(luaL_checkinteger(L, 3)&bit3); // 3-rd parameter: humidity oversampling
	
	config = ((!lua_isnumber(L, 5)?BME280_STANDBY_TIME_20_MS:(luaL_checkinteger(L, 5)&bit3))<< 5) // 5-th parameter: inactive duration in normal mode
		| ((!lua_isnumber(L, 6)?BME280_FILTER_COEFF_16:(luaL_checkinteger(L, 6)&bit3)) << 2); // 6-th parameter: IIR filter
	full_init = !lua_isnumber(L, 7)?1:lua_tointeger(L, 7); // 7-th parameter: init the chip too
	NODE_DBG("mode: %x\nhumidity oss: %x\nconfig: %x\n", bme280_mode, bme280_ossh, config);
	
	bme280_i2c_addr = BME280_I2C_ADDRESS1;
	platform_i2c_send_start(bme280_i2c_id);
	ack = platform_i2c_send_address(bme280_i2c_id, bme280_i2c_addr, PLATFORM_I2C_DIRECTION_TRANSMITTER);
	platform_i2c_send_stop(bme280_i2c_id);
	if (!ack) {
		NODE_DBG("No ACK on address: %x\n", bme280_i2c_addr);
		bme280_i2c_addr = BME280_I2C_ADDRESS2;
		platform_i2c_send_start(bme280_i2c_id);
		ack = platform_i2c_send_address(bme280_i2c_id, bme280_i2c_addr, PLATFORM_I2C_DIRECTION_TRANSMITTER);
		platform_i2c_send_stop(bme280_i2c_id);
		if (!ack) {
			NODE_DBG("No ACK on address: %x\n", bme280_i2c_addr);
			return 0;
		}
	}

	uint8_t chipid = r8u(BME280_REGISTER_CHIPID);
	NODE_DBG("chip_id: %x\n", chipid);
	bme280_isbme = (chipid == 0x60);
	
#define r16uLE_buf(reg)	(uint16_t)((reg[1] << 8) | reg[0])
#define r16sLE_buf(reg)	 (int16_t)(r16uLE_buf(reg))
	uint8_t	buf[18], *reg;

	r8u_n(BME280_REGISTER_DIG_T, 6, buf);
	reg = buf;
	bme280_data.dig_T1 = r16uLE_buf(reg); reg+=2;
	bme280_data.dig_T2 = r16sLE_buf(reg); reg+=2;
	bme280_data.dig_T3 = r16sLE_buf(reg);
	//NODE_DBG("dig_T: %d\t%d\t%d\n", bme280_data.dig_T1, bme280_data.dig_T2, bme280_data.dig_T3);

	r8u_n(BME280_REGISTER_DIG_P, 18, buf);
	reg = buf;
	bme280_data.dig_P1 = r16uLE_buf(reg); reg+=2;
	bme280_data.dig_P2 = r16sLE_buf(reg); reg+=2;
	bme280_data.dig_P3 = r16sLE_buf(reg); reg+=2;
	bme280_data.dig_P4 = r16sLE_buf(reg); reg+=2;
	bme280_data.dig_P5 = r16sLE_buf(reg); reg+=2;
	bme280_data.dig_P6 = r16sLE_buf(reg); reg+=2;
	bme280_data.dig_P7 = r16sLE_buf(reg); reg+=2;
	bme280_data.dig_P8 = r16sLE_buf(reg); reg+=2;
	bme280_data.dig_P9 = r16sLE_buf(reg);
	// NODE_DBG("dig_P: %d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\n", bme280_data.dig_P1, bme280_data.dig_P2, bme280_data.dig_P3, bme280_data.dig_P4, bme280_data.dig_P5, bme280_data.dig_P6, bme280_data.dig_P7, bme280_data.dig_P8, bme280_data.dig_P9);
	
	if (full_init) w8u(BME280_REGISTER_CONFIG, config);
	if (bme280_isbme) {
		bme280_data.dig_H1 = r8u(BME280_REGISTER_DIG_H1);
		r8u_n(BME280_REGISTER_DIG_H2, 7, buf);
		reg = buf;
		bme280_data.dig_H2 = r16sLE_buf(reg); reg+=2;
		bme280_data.dig_H3 = reg[0]; reg++;
		bme280_data.dig_H4 = (int16_t)reg[0] << 4 | (reg[1] & 0x0F); reg+=1;	// H4[11:4 3:0] = 0xE4[7:0] 0xE5[3:0] 12-bit signed
		bme280_data.dig_H5 = (int16_t)reg[1] << 4 | (reg[0]   >> 4); reg+=2;	// H5[11:4 3:0] = 0xE6[7:0] 0xE5[7:4] 12-bit signed
		bme280_data.dig_H6 = (int8_t)reg[0];
		// NODE_DBG("dig_H: %d\t%d\t%d\t%d\t%d\t%d\n", bme280_data.dig_H1, bme280_data.dig_H2, bme280_data.dig_H3, bme280_data.dig_H4, bme280_data.dig_H5, bme280_data.dig_H6);

		if (full_init) w8u(BME280_REGISTER_CONTROL_HUM, bme280_ossh);
		lua_pushinteger(L, 2);
	} else {
		lua_pushinteger(L, 1);
	}
#undef r16uLE_buf
#undef r16sLE_buf
	if (full_init) w8u(BME280_REGISTER_CONTROL, bme280_mode);
	
	return 1;
}

static void bme280_readoutdone (void *arg)
{
	NODE_DBG("timer out\n");
	lua_State *L = lua_getstate();
	lua_rawgeti (L, LUA_REGISTRYINDEX, lua_connected_readout_ref);
	lua_call (L, 0, 0);
	luaL_unref (L, LUA_REGISTRYINDEX, lua_connected_readout_ref);
	os_timer_disarm (&bme280_timer);
}

static int bme280_lua_startreadout(lua_State* L) {
	uint32_t delay;
	
	if (lua_isnumber(L, 1)) {
		delay = luaL_checkinteger(L, 1);
		if (!delay) {delay = BME280_SAMPLING_DELAY;} // if delay is 0 then set the default delay
	}
	
	if (!lua_isnoneornil(L, 2)) {
		lua_pushvalue(L, 2);
		lua_connected_readout_ref = luaL_ref(L, LUA_REGISTRYINDEX);
	} else {
		lua_connected_readout_ref = LUA_NOREF;
	}

	w8u(BME280_REGISTER_CONTROL_HUM, bme280_ossh);
	w8u(BME280_REGISTER_CONTROL, (bme280_mode & 0xFC) | BME280_FORCED_MODE);
	NODE_DBG("control old: %x, control: %x, delay: %d\n", bme280_mode, (bme280_mode & 0xFC) | BME280_FORCED_MODE, delay);

	if (lua_connected_readout_ref != LUA_NOREF) {
		NODE_DBG("timer armed\n");
		os_timer_disarm (&bme280_timer);
		os_timer_setfn (&bme280_timer, (os_timer_func_t *)bme280_readoutdone, L);
		os_timer_arm (&bme280_timer, delay, 0); // trigger callback when readout is ready
	}
	return 0;
}

// Return nothing on failure
// Return T, QFE, H if no altitude given
// Return T, QFE, H, QNH if altitude given
static int bme280_lua_read(lua_State* L) {
	uint8_t buf[8];
	uint32_t qfe;
	uint8_t calc_qnh = lua_isnumber(L, 1);

	r8u_n(BME280_REGISTER_PRESS, 8, buf);	// registers are P[3], T[3], H[2]

	// Must do Temp first since bme280_t_fine is used by the other compensation functions
	uint32_t adc_T = (uint32_t)(((buf[3] << 16) | (buf[4] << 8) | buf[5]) >> 4);
	if (adc_T == 0x80000 || adc_T == 0xfffff)
		return 0;
	lua_pushinteger(L, bme280_compensate_T(adc_T));

	uint32_t adc_P = (uint32_t)(((buf[0] << 16) | (buf[1] << 8) | buf[2]) >> 4);
	if (adc_P ==0x80000 || adc_P == 0xfffff) {
		lua_pushnil(L);
		calc_qnh = 0;
	} else {
		qfe = bme280_compensate_P(adc_P);
		lua_pushinteger(L, qfe);
	}

	uint32_t adc_H = (uint32_t)((buf[6] << 8) | buf[7]);
	if (!bme280_isbme || adc_H == 0x8000 || adc_H == 0xffff)
		lua_pushnil(L);
	else
		lua_pushinteger(L, bme280_compensate_H(adc_H));

	if (calc_qnh) { // have altitude
		int32_t h = luaL_checkinteger(L, 1);
		double qnh = bme280_qfe2qnh(qfe, h);
		lua_pushinteger(L, (int32_t)(qnh + 0.5));
		return 4;
	}
	return 3;
}

static int bme280_lua_temp(lua_State* L) {
	uint8_t buf[3];
	r8u_n(BME280_REGISTER_TEMP, 3, buf); // registers are P[3], T[3], H[2]
	uint32_t adc_T = (uint32_t)(((buf[0] << 16) | (buf[1] << 8) | buf[2]) >> 4);
	if (adc_T == 0x80000 || adc_T == 0xfffff)
		return 0;
	lua_pushinteger(L, bme280_compensate_T(adc_T));
	lua_pushinteger(L, bme280_t_fine);
	return 2;
}

static int bme280_lua_baro(lua_State* L) {
	uint8_t buf[6];
	r8u_n(BME280_REGISTER_PRESS, 6, buf); // registers are P[3], T[3], H[2]
	uint32_t adc_T = (uint32_t)(((buf[3] << 16) | (buf[4] << 8) | buf[5]) >> 4);
	uint32_t T = bme280_compensate_T(adc_T);
	uint32_t adc_P = (uint32_t)(((buf[0] << 16) | (buf[1] << 8) | buf[2]) >> 4);
	if (adc_T == 0x80000 || adc_T == 0xfffff || adc_P ==0x80000 || adc_P == 0xfffff)
		return 0;
	lua_pushinteger(L, bme280_compensate_P(adc_P));
	lua_pushinteger(L, T);
	return 2;
}

static int bme280_lua_humi(lua_State* L) {
	if (!bme280_isbme) return 0;
	uint8_t buf[5];
	r8u_n(BME280_REGISTER_TEMP, 5, buf); // registers are P[3], T[3], H[2]

	uint32_t adc_T = (uint32_t)(((buf[0] << 16) | (buf[1] << 8) | buf[2]) >> 4);
	uint32_t T = bme280_compensate_T(adc_T);
	uint32_t adc_H = (uint32_t)((buf[3] << 8) | buf[4]);
	if (adc_T == 0x80000 || adc_T == 0xfffff || adc_H == 0x8000 || adc_H == 0xffff)
		return 0;
	lua_pushinteger(L, bme280_compensate_H(adc_H));
	lua_pushinteger(L, T);
	return 2;
}

static int bme280_lua_qfe2qnh(lua_State* L) {
	if (!lua_isnumber(L, 2)) {
		return luaL_error(L, "wrong arg range");
	}
	int32_t qfe = luaL_checkinteger(L, 1);
	int32_t h = luaL_checkinteger(L, 2);
	double qnh = bme280_qfe2qnh(qfe, h);
	lua_pushinteger(L, (int32_t)(qnh + 0.5));
	return 1;
}

static int bme280_lua_altitude(lua_State* L) {
	if (!lua_isnumber(L, 2)) {
		return luaL_error(L, "wrong arg range");
	}
	int32_t P = luaL_checkinteger(L, 1);
	int32_t qnh = luaL_checkinteger(L, 2);
	double h = (1.0 - pow((double)P/(double)qnh, 1.0/5.25588)) / 2.25577e-5 * 100.0;

	lua_pushinteger(L, (int32_t)(h + (((h<0)?-1:(h>0)) * 0.5)));
	return 1;
}

static int bme280_lua_dewpoint(lua_State* L) {
	if (!lua_isnumber(L, 2)) {
		return luaL_error(L, "wrong arg range");
	}
	double H = luaL_checkinteger(L, 1)/100000.0;
	double T = luaL_checkinteger(L, 2)/100.0;

	const double c243 = 243.5;
	const double c17 = 17.67;
	double c = ln(H) + ((c17 * T) / (c243 + T));
	double d = (c243 * c)/(c17 - c) * 100.0;

	lua_pushinteger(L, (int32_t)(d + (((d<0)?-1:(d>0)) * 0.5)));
	return 1;
}

static const LUA_REG_TYPE bme280_map[] = {
	{ LSTRKEY( "setup" ), LFUNCVAL(bme280_lua_setup)},
	{ LSTRKEY( "temp" ),  LFUNCVAL(bme280_lua_temp)},
	{ LSTRKEY( "baro" ),  LFUNCVAL(bme280_lua_baro)},
	{ LSTRKEY( "humi" ),  LFUNCVAL(bme280_lua_humi)},
	{ LSTRKEY( "startreadout" ),  LFUNCVAL(bme280_lua_startreadout)},
	{ LSTRKEY( "qfe2qnh" ),  LFUNCVAL(bme280_lua_qfe2qnh)},
	{ LSTRKEY( "altitude" ),  LFUNCVAL(bme280_lua_altitude)},
	{ LSTRKEY( "dewpoint" ),  LFUNCVAL(bme280_lua_dewpoint)},
	{ LSTRKEY( "read" ),  LFUNCVAL(bme280_lua_read)},
	{ LNILKEY, LNILVAL}
};

NODEMCU_MODULE(BME280, "bme280", bme280_map, NULL);
