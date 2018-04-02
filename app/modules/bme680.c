// ***************************************************************************
// Port of BMP680 module for ESP8266 with nodeMCU
// 
// Written by Lukas Voborsky, @voborsky
// ***************************************************************************

// #define NODE_DEBUG

#include "module.h"
#include "lauxlib.h"
#include "platform.h"
#include "c_math.h"

#include "bme680_defs.h"

#define DEFAULT_HEATER_DUR 100
#define DEFAULT_HEATER_TEMP 300
#define DEFAULT_AMBIENT_TEMP 23

static const uint32_t bme680_i2c_id = BME680_CHIP_ID_ADDR;

static uint8_t bme680_i2c_addr = BME680_I2C_ADDR_PRIMARY;
os_timer_t bme680_timer; // timer for forced mode readout
int lua_connected_readout_ref; // callback when readout is ready

static struct bme680_calib_data bme680_data;
static uint8_t bme680_mode = 0; // stores oversampling settings
static uint8 os_temp = 0;
static uint8 os_pres = 0;
static uint8 os_hum = 0; // stores humidity oversampling settings
static uint16_t heatr_dur;
static int8_t amb_temp = 23; //DEFAULT_AMBIENT_TEMP;

static uint32_t bme680_h = 0; 
static double bme680_hc = 1.0;

// return 0 if good
static int r8u_n(uint8_t reg, int n, uint8_t *buff) {
	int i;

	platform_i2c_send_start(bme680_i2c_id);
	platform_i2c_send_address(bme680_i2c_id, bme680_i2c_addr, PLATFORM_I2C_DIRECTION_TRANSMITTER);
	platform_i2c_send_byte(bme680_i2c_id, reg);
//	platform_i2c_send_stop(bme680_i2c_id);	// doco says not needed

	platform_i2c_send_start(bme680_i2c_id);
	platform_i2c_send_address(bme680_i2c_id, bme680_i2c_addr, PLATFORM_I2C_DIRECTION_RECEIVER);

	while (n-- > 0)
		*buff++ = platform_i2c_recv_byte(bme680_i2c_id, n > 0);
	platform_i2c_send_stop(bme680_i2c_id);

	return 0;
}

static uint8_t w8u(uint8_t reg, uint8_t val) {
	platform_i2c_send_start(bme680_i2c_id);
	platform_i2c_send_address(bme680_i2c_id, bme680_i2c_addr, PLATFORM_I2C_DIRECTION_TRANSMITTER);
	platform_i2c_send_byte(bme680_i2c_id, reg);
	platform_i2c_send_byte(bme680_i2c_id, val);
	platform_i2c_send_stop(bme680_i2c_id);
}

static uint8_t r8u(uint8_t reg) {
	uint8_t ret[1];
	r8u_n(reg, 1, ret);
	return ret[0];
}

/* This part of code is coming from the original bme680.c driver by Bosch.
 * Copyright (C) 2017 - 2018 Bosch Sensortec GmbH
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * Neither the name of the copyright holder nor the names of the
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 * CONTRIBUTORS "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL COPYRIGHT HOLDER
 * OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES(INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE
 *
 * The information provided is believed to be accurate and reliable.
 * The copyright holder assumes no responsibility
 * for the consequences of use
 * of such information nor for any infringement of patents or
 * other rights of third parties which may result from its use.
 * No license is granted by implication or otherwise under any patent or
 * patent rights of the copyright holder.
 */

/**static variables */
/**Look up table for the possible gas range values */
uint32_t lookupTable1[16] = { UINT32_C(2147483647), UINT32_C(2147483647), UINT32_C(2147483647), UINT32_C(2147483647),
        UINT32_C(2147483647), UINT32_C(2126008810), UINT32_C(2147483647), UINT32_C(2130303777), UINT32_C(2147483647),
        UINT32_C(2147483647), UINT32_C(2143188679), UINT32_C(2136746228), UINT32_C(2147483647), UINT32_C(2126008810),
        UINT32_C(2147483647), UINT32_C(2147483647) };
/**Look up table for the possible gas range values */
uint32_t lookupTable2[16] = { UINT32_C(4096000000), UINT32_C(2048000000), UINT32_C(1024000000), UINT32_C(512000000),
        UINT32_C(255744255), UINT32_C(127110228), UINT32_C(64000000), UINT32_C(32258064), UINT32_C(16016016), UINT32_C(
                8000000), UINT32_C(4000000), UINT32_C(2000000), UINT32_C(1000000), UINT32_C(500000), UINT32_C(250000),
        UINT32_C(125000) };

static uint8_t calc_heater_res(uint16_t temp)
{
	uint8_t heatr_res;
	int32_t var1;
	int32_t var2;
	int32_t var3;
	int32_t var4;
	int32_t var5;
	int32_t heatr_res_x100;

	if (temp < 200) /* Cap temperature */
		temp = 200;
	else if (temp > 400)
		temp = 400;

	var1 = (((int32_t) amb_temp * bme680_data.par_gh3) / 1000) * 256;
	var2 = (bme680_data.par_gh1 + 784) * (((((bme680_data.par_gh2 + 154009) * temp * 5) / 100) + 3276800) / 10);
	var3 = var1 + (var2 / 2);
	var4 = (var3 / (bme680_data.res_heat_range + 4));
	var5 = (131 * bme680_data.res_heat_val) + 65536;
	heatr_res_x100 = (int32_t) (((var4 / var5) - 250) * 34);
	heatr_res = (uint8_t) ((heatr_res_x100 + 50) / 100);

	return heatr_res;
}

static uint8_t calc_heater_dur(uint16_t dur)
{
	uint8_t factor = 0;
	uint8_t durval;

	if (dur >= 0xfc0) {
		durval = 0xff; /* Max duration*/
	} else {
		while (dur > 0x3F) {
			dur = dur / 4;
			factor += 1;
		}
		durval = (uint8_t) (dur + (factor * 64));
	}

	return durval;
}

static int16_t calc_temperature(uint32_t temp_adc)
{
	int64_t var1;
	int64_t var2;
	int64_t var3;
	int16_t calc_temp;

	var1 = ((int32_t) temp_adc / 8) - ((int32_t) bme680_data.par_t1 * 2);
	var2 = (var1 * (int32_t) bme680_data.par_t2) / 2048;
	var3 = ((var1 / 2) * (var1 / 2)) / 4096;
	var3 = ((var3) * ((int32_t) bme680_data.par_t3 * 16)) / 16384;
	bme680_data.t_fine = (int32_t) (var2 + var3);
	calc_temp = (int16_t) (((bme680_data.t_fine * 5) + 128) / 256);

	return calc_temp;
}

static uint32_t calc_pressure(uint32_t pres_adc)
{
	int32_t var1;
	int32_t var2;
	int32_t var3;
	int32_t calc_pres;

	var1 = (((int32_t) bme680_data.t_fine) / 2) - 64000;
	var2 = ((var1 / 4) * (var1 / 4)) / 2048;
	var2 = ((var2) * (int32_t) bme680_data.par_p6) / 4;
	var2 = var2 + ((var1 * (int32_t) bme680_data.par_p5) * 2);
	var2 = (var2 / 4) + ((int32_t) bme680_data.par_p4 * 65536);
	var1 = ((var1 / 4) * (var1 / 4)) / 8192;
	var1 = (((var1) * ((int32_t) bme680_data.par_p3 * 32)) / 8) + (((int32_t) bme680_data.par_p2 * var1) / 2);
	var1 = var1 / 262144;
	var1 = ((32768 + var1) * (int32_t) bme680_data.par_p1) / 32768;
	calc_pres = (int32_t) (1048576 - pres_adc);
	calc_pres = (int32_t) ((calc_pres - (var2 / 4096)) * (3125));
	calc_pres = ((calc_pres / var1) * 2);
	var1 = ((int32_t) bme680_data.par_p9 * (int32_t) (((calc_pres / 8) * (calc_pres / 8)) / 8192)) / 4096;
	var2 = ((int32_t) (calc_pres / 4) * (int32_t) bme680_data.par_p8) / 8192;
	var3 = ((int32_t) (calc_pres / 256) * (int32_t) (calc_pres / 256) * (int32_t) (calc_pres / 256)
	        * (int32_t) bme680_data.par_p10) / 131072;
	calc_pres = (int32_t) (calc_pres) + ((var1 + var2 + var3 + ((int32_t) bme680_data.par_p7 * 128)) / 16);

	return (uint32_t) calc_pres;
}

static uint32_t calc_humidity(uint16_t hum_adc)
{
	int32_t var1;
	int32_t var2;
	int32_t var3;
	int32_t var4;
	int32_t var5;
	int32_t var6;
	int32_t temp_scaled;
	int32_t calc_hum;

	temp_scaled = (((int32_t) bme680_data.t_fine * 5) + 128) / 256;
	var1 = (int32_t) (hum_adc - ((int32_t) ((int32_t) bme680_data.par_h1 * 16)))
	        - (((temp_scaled * (int32_t) bme680_data.par_h3) / ((int32_t) 100)) / 2);
	var2 = ((int32_t) bme680_data.par_h2
	        * (((temp_scaled * (int32_t) bme680_data.par_h4) / ((int32_t) 100))
	                + (((temp_scaled * ((temp_scaled * (int32_t) bme680_data.par_h5) / ((int32_t) 100))) / 64)
	                        / ((int32_t) 100)) + (int32_t) (1 * 16384))) / 1024;
	var3 = var1 * var2;
	var4 = (int32_t) bme680_data.par_h6 * 128;
	var4 = ((var4) + ((temp_scaled * (int32_t) bme680_data.par_h7) / ((int32_t) 100))) / 16;
	var5 = ((var3 / 16384) * (var3 / 16384)) / 1024;
	var6 = (var4 * var5) / 2;
	calc_hum = (((var3 + var6) / 1024) * ((int32_t) 1000)) / 4096;

	if (calc_hum > 100000) /* Cap at 100%rH */
		calc_hum = 100000;
	else if (calc_hum < 0)
		calc_hum = 0;

	return (uint32_t) calc_hum;
}

static uint32_t calc_gas_resistance(uint16_t gas_res_adc, uint8_t gas_range)
{
	int64_t var1;
	uint64_t var2;
	int64_t var3;
	uint32_t calc_gas_res;

	var1 = (int64_t) ((1340 + (5 * (int64_t) bme680_data.range_sw_err)) * ((int64_t) lookupTable1[gas_range])) / 65536;
	var2 = (((int64_t) ((int64_t) gas_res_adc * 32768) - (int64_t) (16777216)) + var1);
	var3 = (((int64_t) lookupTable2[gas_range] * (int64_t) var1) / 512);
	calc_gas_res = (uint32_t) ((var3 + ((int64_t) var2 / 2)) / (int64_t) var2);

	return calc_gas_res;
}

uint16_t calc_dur()
{
	uint32_t tph_dur; /* Calculate in us */

	/* TPH measurement duration */
  
	tph_dur = ((uint32_t) (os_temp + os_pres + os_hum) * UINT32_C(1963));
	tph_dur += UINT32_C(477 * 4); /* TPH switching duration */
	tph_dur += UINT32_C(477 * 5); /* Gas measurement duration */
	tph_dur += UINT32_C(500); /* Get it to the closest whole number.*/
	tph_dur /= UINT32_C(1000); /* Convert to ms */

	tph_dur += UINT32_C(1); /* Wake up duration of 1ms */
  NODE_DBG("tpc_dur: %d\n", tph_dur);
	/* The remaining time should be used for heating */
	return heatr_dur + (uint16_t) tph_dur;
}
/* This part of code is coming from the original bme680.c driver by Bosch.
 * END */


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
	if (bme680_h == h) {
		hc = bme680_hc;
	} else {
		hc = pow((double)(1.0 - 2.25577e-5 * h), (double)(-5.25588));
		bme680_hc = hc; bme680_h = h;
	}
	double qnh = (double)qfe * hc;
	return qnh;
}

static int bme680_lua_setup(lua_State* L) {
	uint8_t ack;

	bme680_i2c_addr = BME680_I2C_ADDR_PRIMARY;
	platform_i2c_send_start(bme680_i2c_id);
	ack = platform_i2c_send_address(bme680_i2c_id, bme680_i2c_addr, PLATFORM_I2C_DIRECTION_TRANSMITTER);
	platform_i2c_send_stop(bme680_i2c_id);
	if (!ack) {
		NODE_DBG("No ACK on address: %x\n", bme680_i2c_addr);
		bme680_i2c_addr = BME680_I2C_ADDR_SECONDARY;
		platform_i2c_send_start(bme680_i2c_id);
		ack = platform_i2c_send_address(bme680_i2c_id, bme680_i2c_addr, PLATFORM_I2C_DIRECTION_TRANSMITTER);
		platform_i2c_send_stop(bme680_i2c_id);
		if (!ack) {
			NODE_DBG("No ACK on address: %x\n", bme680_i2c_addr);
			return 0;
		}
	}

	uint8_t chipid = r8u(BME680_CHIP_ID_ADDR);
	NODE_DBG("chip_id: %x\n", chipid);

#define r16uLE_buf(reg)	(uint16_t)(((uint16_t)reg[1] << 8) | (uint16_t)reg[0])
#define r16sLE_buf(reg)	 (int16_t)(r16uLE_buf(reg))
	uint8_t	buff[BME680_COEFF_SIZE], *reg;
	r8u_n(BME680_COEFF_ADDR1, BME680_COEFF_ADDR1_LEN, buff);
  r8u_n(BME680_COEFF_ADDR2, BME680_COEFF_ADDR2_LEN, &buff[BME680_COEFF_ADDR1_LEN]);

	reg = buff + 1; 
	bme680_data.par_t2 = r16sLE_buf(reg); reg+=2; // #define BME680_T3_REG		(3)
	bme680_data.par_t3 = (int8_t) reg[0]; reg+=2; // #define BME680_P1_LSB_REG	(5)
	bme680_data.par_p1 = r16uLE_buf(reg); reg+=2; // #define BME680_P2_LSB_REG	(7)
	bme680_data.par_p2 = r16sLE_buf(reg); reg+=2; // #define BME680_P3_REG		(9)
	bme680_data.par_p3 = (int8_t) reg[0]; reg+=2; // #define BME680_P4_LSB_REG	(11)
	bme680_data.par_p4 = r16sLE_buf(reg); reg+=2; // #define BME680_P5_LSB_REG	(13)
	bme680_data.par_p5 = r16sLE_buf(reg); reg+=2; // #define BME680_P7_REG		(15)
	bme680_data.par_p7 = (int8_t) reg[0]; reg++; // #define BME680_P6_REG		(16)
	bme680_data.par_p6 = (int8_t) reg[0]; reg+=3;  // #define BME680_P8_LSB_REG	(19)
	bme680_data.par_p8 = r16sLE_buf(reg); reg+=2; // #define BME680_P9_LSB_REG	(21)
	bme680_data.par_p9 = r16sLE_buf(reg); reg+=2; // #define BME680_P10_REG		(23)
	bme680_data.par_p10 = (int8_t) reg[0]; reg+=2; // #define BME680_H2_MSB_REG	(25)
  bme680_data.par_h2 = (uint16_t) (((uint16_t) reg[0] << BME680_HUM_REG_SHIFT_VAL)
    | ((reg[1]) >> BME680_HUM_REG_SHIFT_VAL)); reg++; // #define BME680_H1_LSB_REG	(26)
  bme680_data.par_h1 = (uint16_t) (((uint16_t) reg[1] << BME680_HUM_REG_SHIFT_VAL)
    | (reg[0] & BME680_BIT_H1_DATA_MSK)); reg+=2; // #define BME680_H3_REG		(28)
	bme680_data.par_h3 = (int8_t) reg[0]; reg++; // #define BME680_H4_REG		(29)
	bme680_data.par_h4 = (int8_t) reg[0]; reg++; // #define BME680_H5_REG		(30)
	bme680_data.par_h5 = (int8_t) reg[0]; reg++; // #define BME680_H6_REG		(31)
	bme680_data.par_h6 = (uint8_t) reg[0]; reg++; // #define BME680_H7_REG		(32)
	bme680_data.par_h7 = (int8_t) reg[0]; reg++; // #define BME680_T1_LSB_REG	(33)
  bme680_data.par_t1 = r16uLE_buf(reg); reg+=2; // #define BME680_GH2_LSB_REG	(35)
  bme680_data.par_gh2 = r16sLE_buf(reg); reg+=2; // #define BME680_GH1_REG		(37)
	bme680_data.par_gh1 = reg[0]; reg++; // #define BME680_GH3_REG		(38)
	bme680_data.par_gh3 = reg[0]; 
#undef r16uLE_buf
#undef r16sLE_buf
  
  /* Other coefficients */
  bme680_data.res_heat_range = ((r8u(BME680_ADDR_RES_HEAT_RANGE_ADDR) & BME680_RHRANGE_MSK) / 16);
  bme680_data.res_heat_val = (int8_t) r8u(BME680_ADDR_RES_HEAT_VAL_ADDR);
  bme680_data.range_sw_err = ((int8_t) r8u(BME680_ADDR_RANGE_SW_ERR_ADDR) & (int8_t) BME680_RSERROR_MSK) / 16;
    
  NODE_DBG("par_T: %d\t%d\t%d\n", bme680_data.par_t1, bme680_data.par_t2, bme680_data.par_t3);
  NODE_DBG("par_P: %d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\n", bme680_data.par_p1, bme680_data.par_p2, bme680_data.par_p3, bme680_data.par_p4, bme680_data.par_p5, bme680_data.par_p6, bme680_data.par_p7, bme680_data.par_p8, bme680_data.par_p9, bme680_data.par_p10);
  NODE_DBG("par_H: %d\t%d\t%d\t%d\t%d\t%d\t%d\n", bme680_data.par_h1, bme680_data.par_h2, bme680_data.par_h3, bme680_data.par_h4, bme680_data.par_h5, bme680_data.par_h6, bme680_data.par_h7);
  NODE_DBG("par_GH: %d\t%d\t%d\n", bme680_data.par_gh1, bme680_data.par_gh2, bme680_data.par_gh3);
  NODE_DBG("res_heat_range, res_heat_val, range_sw_err: %d\t%d\t%d\n", bme680_data.res_heat_range, bme680_data.res_heat_val, bme680_data.range_sw_err);
	
  uint8_t full_init = !lua_isnumber(L, 7)?1:lua_tointeger(L, 7); // 7-th parameter: init the chip too
  if (full_init) {
    uint8_t filter;
    uint8_t const bit3 = 0b111;
    uint8_t const bit2 = 0b11;

    //bme680.setup([temp_oss, press_oss, humi_oss, heater_temp, heater_duration, IIR_filter])

    os_temp = (!lua_isnumber(L, 1)?BME680_OS_2X:(luaL_checkinteger(L, 1)&bit3)); // 1-st parameter: temperature oversampling
    os_pres = (!lua_isnumber(L, 2)?BME680_OS_16X:(luaL_checkinteger(L, 2)&bit3)); // 2-nd parameter: pressure oversampling
    os_hum = (!lua_isnumber(L, 3))?BME680_OS_1X:(luaL_checkinteger(L, 3)&bit3);
    bme680_mode = BME680_SLEEP_MODE | (os_pres << 2) | (os_temp << 5); 
    os_hum = os_hum; // 3-rd parameter: humidity oversampling
       
    filter = ((!lua_isnumber(L, 6)?BME680_FILTER_SIZE_31:(luaL_checkinteger(L, 6)&bit3)) << 2); // 6-th parameter: IIR filter
  
    NODE_DBG("mode: %x\nhumidity oss: %x\nconfig: %x\n", bme680_mode, os_hum, filter);
    
    heatr_dur = (!lua_isnumber(L, 5)?DEFAULT_HEATER_DUR:(luaL_checkinteger(L, 5))); // 5-th parameter: heater duration
    w8u(BME680_GAS_WAIT0_ADDR, calc_heater_dur(heatr_dur));
    w8u(BME680_RES_HEAT0_ADDR, calc_heater_res((!lua_isnumber(L, 4)?DEFAULT_HEATER_TEMP:(luaL_checkinteger(L, 4))))); // 4-th parameter: heater temperature
  
    w8u(BME680_CONF_ODR_FILT_ADDR, BME680_SET_BITS_POS_0(r8u(BME680_CONF_ODR_FILT_ADDR), BME680_FILTER, filter)); // #define BME680_CONF_ODR_FILT_ADDR		UINT8_C(0x75)
    
    // set heater on 
    w8u(BME680_CONF_HEAT_CTRL_ADDR, BME680_SET_BITS_POS_0(r8u(BME680_CONF_HEAT_CTRL_ADDR), BME680_HCTRL, 1));
    
    w8u(BME680_CONF_T_P_MODE_ADDR, bme680_mode);
    w8u(BME680_CONF_OS_H_ADDR, BME680_SET_BITS_POS_0(r8u(BME680_CONF_OS_H_ADDR), BME680_OSH, os_hum));
    w8u(BME680_CONF_ODR_RUN_GAS_NBC_ADDR, 1 << 4 | 0 & bit3);
  }
  lua_pushinteger(L, 1);

	return 1;
}

static void bme280_readoutdone (void *arg)
{
	NODE_DBG("timer out\n");
	lua_State *L = lua_getstate();
	lua_rawgeti (L, LUA_REGISTRYINDEX, lua_connected_readout_ref);
	lua_call (L, 0, 0);
	luaL_unref (L, LUA_REGISTRYINDEX, lua_connected_readout_ref);
	os_timer_disarm (&bme680_timer);
}

static int bme680_lua_startreadout(lua_State* L) {
	uint32_t delay;
	
	if (lua_isnumber(L, 1)) {
		delay = luaL_checkinteger(L, 1);
		if (!delay) {delay = calc_dur();} // if delay is 0 then set the default delay
	}
	
	if (!lua_isnoneornil(L, 2)) {
		lua_pushvalue(L, 2);
		lua_connected_readout_ref = luaL_ref(L, LUA_REGISTRYINDEX);
	} else {
		lua_connected_readout_ref = LUA_NOREF;
	}

  w8u(BME680_CONF_OS_H_ADDR, os_hum);
  w8u(BME680_CONF_T_P_MODE_ADDR, (bme680_mode & 0xFC) | BME680_FORCED_MODE);
  
	NODE_DBG("control old: %x, control: %x, delay: %d\n", bme680_mode, (bme680_mode & 0xFC) | BME680_FORCED_MODE, delay);

	if (lua_connected_readout_ref != LUA_NOREF) {
		NODE_DBG("timer armed\n");
		os_timer_disarm (&bme680_timer);
		os_timer_setfn (&bme680_timer, (os_timer_func_t *)bme280_readoutdone, L);
		os_timer_arm (&bme680_timer, delay, 0); // trigger callback when readout is ready
	}
	return 0;
}

// Return nothing on failure
// Return T, QFE, H if no altitude given
// Return T, QFE, H, QNH if altitude given
static int bme680_lua_read(lua_State* L) {
	uint8_t buff[BME680_FIELD_LENGTH] = { 0 };
	uint8_t gas_range;
	uint32_t adc_temp;
	uint32_t adc_pres;
	uint16_t adc_hum;
	uint16_t adc_gas_res;
  uint8_t status;
  
	uint32_t qfe;
	uint8_t calc_qnh = lua_isnumber(L, 1);

	r8u_n(BME680_FIELD0_ADDR, BME680_FIELD_LENGTH, buff);

  status = buff[0] & BME680_NEW_DATA_MSK;

  /* read the raw data from the sensor */
  adc_pres = (uint32_t) (((uint32_t) buff[2] * 4096) | ((uint32_t) buff[3] * 16) | ((uint32_t) buff[4] / 16));
  adc_temp = (uint32_t) (((uint32_t) buff[5] * 4096) | ((uint32_t) buff[6] * 16) | ((uint32_t) buff[7] / 16));
  adc_hum = (uint16_t) (((uint32_t) buff[8] * 256) | (uint32_t) buff[9]);
  adc_gas_res = (uint16_t) ((uint32_t) buff[13] * 4 | (((uint32_t) buff[14]) / 64));
  
  gas_range = buff[14] & BME680_GAS_RANGE_MSK;
 
  status |= buff[14] & BME680_GASM_VALID_MSK;
  status |= buff[14] & BME680_HEAT_STAB_MSK;
  NODE_DBG("status, new_data, gas_range, gasm_valid: 0x%x, 0x%x, 0x%x, 0x%x\n", status, status & BME680_NEW_DATA_MSK, buff[14] & BME680_GAS_RANGE_MSK, buff[14] & BME680_GASM_VALID_MSK);
  if (!(status & BME680_NEW_DATA_MSK)) {
    return 0;
  }

  int16_t temp = calc_temperature(adc_temp);
  amb_temp = temp / 100;
 	lua_pushinteger(L, temp);
  qfe = calc_pressure(adc_pres);
  lua_pushinteger(L, qfe);
  lua_pushinteger(L, calc_humidity(adc_hum));
  lua_pushinteger(L, calc_gas_resistance(adc_gas_res, gas_range));

	if (calc_qnh) { // have altitude
		int32_t h = luaL_checkinteger(L, 1);
		double qnh = bme280_qfe2qnh(qfe, h);
		lua_pushinteger(L, (int32_t)(qnh + 0.5));
		return 5;
	}
	return 4;
}

static int bme680_lua_qfe2qnh(lua_State* L) {
	if (!lua_isnumber(L, 2)) {
		return luaL_error(L, "wrong arg range");
	}
	int32_t qfe = luaL_checkinteger(L, 1);
	int32_t h = luaL_checkinteger(L, 2);
	double qnh = bme280_qfe2qnh(qfe, h);
	lua_pushinteger(L, (int32_t)(qnh + 0.5));
	return 1;
}

static int bme680_lua_altitude(lua_State* L) {
	if (!lua_isnumber(L, 2)) {
		return luaL_error(L, "wrong arg range");
	}
	int32_t P = luaL_checkinteger(L, 1);
	int32_t qnh = luaL_checkinteger(L, 2);
	double h = (1.0 - pow((double)P/(double)qnh, 1.0/5.25588)) / 2.25577e-5 * 100.0;

	lua_pushinteger(L, (int32_t)(h + (((h<0)?-1:(h>0)) * 0.5)));
	return 1;
}

static int bme680_lua_dewpoint(lua_State* L) {
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

static const LUA_REG_TYPE bme680_map[] = {
	{ LSTRKEY( "setup" ), LFUNCVAL(bme680_lua_setup)},
	{ LSTRKEY( "startreadout" ),  LFUNCVAL(bme680_lua_startreadout)},
	{ LSTRKEY( "qfe2qnh" ),  LFUNCVAL(bme680_lua_qfe2qnh)},
	{ LSTRKEY( "altitude" ),  LFUNCVAL(bme680_lua_altitude)},
	{ LSTRKEY( "dewpoint" ),  LFUNCVAL(bme680_lua_dewpoint)},
	{ LSTRKEY( "read" ),  LFUNCVAL(bme680_lua_read)},
	{ LNILKEY, LNILVAL}
};

NODEMCU_MODULE(BME680, "bme680", bme680_map, NULL);
