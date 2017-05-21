//***************************************************************************
// Si7021 module for ESP8266 with nodeMCU
// fetchbot @github
// MIT license, http://opensource.org/licenses/MIT
//***************************************************************************

#include "module.h"
#include "lauxlib.h"
#include "platform.h"
#include "osapi.h"

//***************************************************************************
// I2C ADDRESS DEFINITON
//***************************************************************************

#define ADS1115_I2C_ADDR_GND		(0x48)
#define ADS1115_I2C_ADDR_VDD		(0x49)
#define ADS1115_I2C_ADDR_SDA		(0x4A)
#define ADS1115_I2C_ADDR_SCL		(0x4B)

//***************************************************************************
// POINTER REGISTER
//***************************************************************************

#define ADS1115_POINTER_MASK		(0x03)
#define ADS1115_POINTER_CONVERSION	(0x00)
#define ADS1115_POINTER_CONFIG		(0x01)
#define ADS1115_POINTER_THRESH_LOW	(0x02)
#define ADS1115_POINTER_THRESH_HI	(0x03)

//***************************************************************************
// CONFIG REGISTER
//***************************************************************************

#define ADS1115_OS_MASK				(0x8000)
#define ADS1115_OS_NON				(0x0000)
#define ADS1115_OS_SINGLE			(0x8000)	// Write: Set to start a single-conversion
#define ADS1115_OS_BUSY				(0x0000)	// Read: Bit = 0 when conversion is in progress
#define ADS1115_OS_NOTBUSY			(0x8000)	// Read: Bit = 1 when device is not performing a conversion

#define ADS1115_MUX_MASK			(0x7000)
#define ADS1115_MUX_DIFF_0_1		(0x0000)	// Differential P = AIN0, N = AIN1 (default)
#define ADS1115_MUX_DIFF_0_3		(0x1000)	// Differential P = AIN0, N = AIN3
#define ADS1115_MUX_DIFF_1_3		(0x2000)	// Differential P = AIN1, N = AIN3
#define ADS1115_MUX_DIFF_2_3		(0x3000)	// Differential P = AIN2, N = AIN3
#define ADS1115_MUX_SINGLE_0		(0x4000)	// Single-ended AIN0
#define ADS1115_MUX_SINGLE_1		(0x5000)	// Single-ended AIN1
#define ADS1115_MUX_SINGLE_2		(0x6000)	// Single-ended AIN2
#define ADS1115_MUX_SINGLE_3		(0x7000)	// Single-ended AIN3

#define ADS1115_PGA_MASK			(0x0E00)
#define ADS1115_PGA_6_144V			(0x0000)	// +/-6.144V range = Gain 2/3
#define ADS1115_PGA_4_096V			(0x0200)	// +/-4.096V range = Gain 1
#define ADS1115_PGA_2_048V			(0x0400)	// +/-2.048V range = Gain 2 (default)
#define ADS1115_PGA_1_024V			(0x0600)	// +/-1.024V range = Gain 4
#define ADS1115_PGA_0_512V			(0x0800)	// +/-0.512V range = Gain 8
#define ADS1115_PGA_0_256V			(0x0A00)	// +/-0.256V range = Gain 16

#define ADS1115_MODE_MASK			(0x0100)
#define ADS1115_MODE_CONTIN			(0x0000)	// Continuous conversion mode
#define ADS1115_MODE_SINGLE			(0x0100)	// Power-down single-shot mode (default)

#define ADS1115_DR_MASK				(0x00E0)
#define ADS1115_DR_8SPS				(0x0000)	// 8 samples per second
#define ADS1115_DR_16SPS			(0x0020)	// 16 samples per second
#define ADS1115_DR_32SPS			(0x0040)	// 32 samples per second
#define ADS1115_DR_64SPS			(0x0060)	// 64 samples per second
#define ADS1115_DR_128SPS			(0x0080)	// 128 samples per second (default)
#define ADS1115_DR_250SPS			(0x00A0)	// 250 samples per second
#define ADS1115_DR_475SPS			(0x00C0)	// 475 samples per second
#define ADS1115_DR_860SPS			(0x00E0)	// 860 samples per second

#define ADS1115_CMODE_MASK			(0x0010)
#define ADS1115_CMODE_TRAD			(0x0000)	// Traditional comparator with hysteresis (default)
#define ADS1115_CMODE_WINDOW		(0x0010)	// Window comparator

#define ADS1115_CPOL_MASK			(0x0008)
#define ADS1115_CPOL_ACTVLOW		(0x0000)	// ALERT/RDY pin is low when active (default)
#define ADS1115_CPOL_ACTVHI			(0x0008)	// ALERT/RDY pin is high when active

#define ADS1115_CLAT_MASK			(0x0004)	// Determines if ALERT/RDY pin latches once asserted
#define ADS1115_CLAT_NONLAT			(0x0000)	// Non-latching comparator (default)
#define ADS1115_CLAT_LATCH			(0x0004)	// Latching comparator

#define ADS1115_CQUE_MASK			(0x0003)
#define ADS1115_CQUE_1CONV			(0x0000)	// Assert ALERT/RDY after one conversions
#define ADS1115_CQUE_2CONV			(0x0001)	// Assert ALERT/RDY after two conversions
#define ADS1115_CQUE_4CONV			(0x0002)	// Assert ALERT/RDY after four conversions
#define ADS1115_CQUE_NONE			(0x0003)	// Disable the comparator and put ALERT/RDY in high state (default)

//***************************************************************************

static const uint8_t ads1115_i2c_id = 0;
static const uint8_t general_i2c_addr = 0x00;
static const uint8_t ads1115_i2c_reset = 0x06;
static uint8_t ads1115_i2c_addr = ADS1115_I2C_ADDR_GND;
static uint16_t ads1115_os = ADS1115_OS_SINGLE;
static uint16_t ads1115_gain = ADS1115_PGA_6_144V;
static uint16_t ads1115_samples = ADS1115_DR_128SPS;
static uint16_t ads1115_channel = ADS1115_MUX_SINGLE_0;
static uint16_t ads1115_comp = ADS1115_CQUE_NONE;
static uint16_t ads1115_mode = ADS1115_MODE_SINGLE;
static uint16_t ads1115_threshold_low = 0x8000;
static uint16_t ads1115_threshold_hi = 0x7FFF;
static uint16_t ads1115_config = 0x8583;
static uint16_t ads1115_conversion = 0;
static double ads1115_volt = 0;
os_timer_t ads1115_timer;	// timer for conversion delay
int ads1115_timer_ref; // callback when readout is ready

static int ads1115_lua_readoutdone(void);

static uint8_t write_reg(uint8_t reg, uint16_t config) {
	platform_i2c_send_start(ads1115_i2c_id);
	platform_i2c_send_address(ads1115_i2c_id, ads1115_i2c_addr, PLATFORM_I2C_DIRECTION_TRANSMITTER);
	platform_i2c_send_byte(ads1115_i2c_id, reg);
	platform_i2c_send_byte(ads1115_i2c_id, (uint8_t)(config >> 8));
	platform_i2c_send_byte(ads1115_i2c_id, (uint8_t)(config & 0xFF));
	platform_i2c_send_stop(ads1115_i2c_id);
}

static uint16_t read_reg(uint8_t reg) {
	platform_i2c_send_start(ads1115_i2c_id);
	platform_i2c_send_address(ads1115_i2c_id, ads1115_i2c_addr, PLATFORM_I2C_DIRECTION_TRANSMITTER);
	platform_i2c_send_byte(ads1115_i2c_id, reg);
	platform_i2c_send_stop(ads1115_i2c_id);
	platform_i2c_send_start(ads1115_i2c_id);
	platform_i2c_send_address(ads1115_i2c_id, ads1115_i2c_addr, PLATFORM_I2C_DIRECTION_RECEIVER);
	uint16_t buf = (platform_i2c_recv_byte(ads1115_i2c_id, 1) << 8);
	buf += platform_i2c_recv_byte(ads1115_i2c_id, 0);
	platform_i2c_send_stop(ads1115_i2c_id);
	return buf;
}

// convert ADC value to voltage corresponding to PGA settings
static double get_volt(uint16_t value) {
	
	double volt = 0;
	
	switch (ads1115_gain) {
		case (ADS1115_PGA_6_144V):
			volt = (int16_t)value * 0.1875;
			break;
		case (ADS1115_PGA_4_096V):
			volt = (int16_t)value * 0.125;
			break;
		case (ADS1115_PGA_2_048V):
			volt = (int16_t)value * 0.0625;
			break;
		case (ADS1115_PGA_1_024V):
			volt = (int16_t)value * 0.03125;
			break;
		case (ADS1115_PGA_0_512V):
			volt = (int16_t)value * 0.015625;
			break;
		case (ADS1115_PGA_0_256V):
			volt = (int16_t)value * 0.0078125;
			break;
	}
	
	return volt;
}

// convert threshold in volt to ADC value corresponding to PGA settings
static uint8_t get_value(int16_t *volt) {
	
	switch (ads1115_gain) {
		case (ADS1115_PGA_6_144V):
			if ((*volt >= 6144) || (*volt < -6144) || ((*volt < 0) && (ads1115_channel >> 14))) return 1;
			*volt = *volt / 0.1875;
			break;
		case (ADS1115_PGA_4_096V):
			if ((*volt >= 4096) || (*volt < -4096) || ((*volt < 0) && (ads1115_channel >> 14))) return 1;
			*volt = *volt / 0.125;
			break;
		case (ADS1115_PGA_2_048V):
			if ((*volt >= 2048) || (*volt < -2048) || ((*volt < 0) && (ads1115_channel >> 14))) return 1;
			*volt = *volt / 0.0625;
			break;
		case (ADS1115_PGA_1_024V):
			if ((*volt >= 1024) || (*volt < -1024) || ((*volt < 0) && (ads1115_channel >> 14))) return 1;
			*volt = *volt / 0.03125;
			break;
		case (ADS1115_PGA_0_512V):
			if ((*volt >= 512) || (*volt < -512) || ((*volt < 0) && (ads1115_channel >> 14))) return 1;
			*volt = *volt / 0.015625;
			break;
		case (ADS1115_PGA_0_256V):
			if ((*volt >= 256) || (*volt < -256) || ((*volt < 0) && (ads1115_channel >> 14))) return 1;
			*volt = *volt / 0.0078125;
			break;
	}
	
	return 0;
}

// Initializes ADC
// Lua:		ads11115.setup(ADDRESS)
static int ads1115_lua_setup(lua_State *L) {
	
	// check variables
	if (!lua_isnumber(L, 1)) {
		return luaL_error(L, "wrong arg range");
	}
	
	ads1115_i2c_addr = luaL_checkinteger(L, 1);
	if (!((ads1115_i2c_addr == ADS1115_I2C_ADDR_GND) || (ads1115_i2c_addr == ADS1115_I2C_ADDR_VDD) || (ads1115_i2c_addr == ADS1115_I2C_ADDR_SDA) || (ads1115_i2c_addr == ADS1115_I2C_ADDR_SCL))) {
		return luaL_error(L, "Invalid argument: adddress");
	}
	
	platform_i2c_send_start(ads1115_i2c_id);
	platform_i2c_send_address(ads1115_i2c_id, general_i2c_addr, PLATFORM_I2C_DIRECTION_TRANSMITTER);
	platform_i2c_send_byte(ads1115_i2c_id, ads1115_i2c_reset);
	platform_i2c_send_stop(ads1115_i2c_id);
	
	// check for device on i2c bus
	if (read_reg(ADS1115_POINTER_CONFIG) != 0x8583) {
		return luaL_error(L, "found no device");
	}
	
	return 0;
}

// Change ADC settings
// Lua: 	ads1115.setting(GAIN,SAMPLES,CHANNEL,MODE[,CONVERSION_RDY][,COMPARATOR,THRESHOLD_LOW,THRESHOLD_HI])
static int ads1115_lua_setting(lua_State *L) {
	
	// check variables
	if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2) || !lua_isnumber(L, 3) || !lua_isnumber(L, 4)) {
		return luaL_error(L, "wrong arg range");
	}
	
	ads1115_gain = luaL_checkinteger(L, 1);
	if (!((ads1115_gain == ADS1115_PGA_6_144V) || (ads1115_gain == ADS1115_PGA_4_096V) || (ads1115_gain == ADS1115_PGA_2_048V) || (ads1115_gain == ADS1115_PGA_1_024V) || (ads1115_gain == ADS1115_PGA_0_512V) || (ads1115_gain == ADS1115_PGA_0_256V))) {
		return luaL_error(L, "Invalid argument: gain");
	}
	
	ads1115_samples = luaL_checkinteger(L, 2);
	if (!((ads1115_samples == ADS1115_DR_8SPS) || (ads1115_samples == ADS1115_DR_16SPS) || (ads1115_samples == ADS1115_DR_32SPS) || (ads1115_samples == ADS1115_DR_64SPS) || (ads1115_samples == ADS1115_DR_128SPS) || (ads1115_samples == ADS1115_DR_250SPS) || (ads1115_samples == ADS1115_DR_475SPS) || (ads1115_samples == ADS1115_DR_860SPS))) {
		return luaL_error(L, "Invalid argument: samples");
	}
	
	ads1115_channel = luaL_checkinteger(L, 3);
	if (!((ads1115_channel == ADS1115_MUX_SINGLE_0) || (ads1115_channel == ADS1115_MUX_SINGLE_1) || (ads1115_channel == ADS1115_MUX_SINGLE_2) || (ads1115_channel == ADS1115_MUX_SINGLE_3) || (ads1115_channel == ADS1115_MUX_DIFF_0_1) || (ads1115_channel == ADS1115_MUX_DIFF_0_3) || (ads1115_channel == ADS1115_MUX_DIFF_1_3) || (ads1115_channel == ADS1115_MUX_DIFF_2_3))) {
		return luaL_error(L, "Invalid argument: channel");
	}
	
	ads1115_mode = luaL_checkinteger(L, 4);
	if (!((ads1115_mode == ADS1115_MODE_SINGLE) || (ads1115_mode == ADS1115_MODE_CONTIN))) {
		return luaL_error(L, "Invalid argument: mode");
	}
	
	if (ads1115_mode == ADS1115_MODE_SINGLE) {
		ads1115_os = ADS1115_OS_SINGLE;
	} else {
		ads1115_os = ADS1115_OS_NON;
	}
	
	ads1115_comp = ADS1115_CQUE_NONE;
	
	// Parse optional parameters
	if (lua_isnumber(L, 5) && !(lua_isnumber(L, 6) || lua_isnumber(L, 7))) {
		
		//	conversion ready mode
		ads1115_comp = luaL_checkinteger(L, 5);
		if (!((ads1115_comp == ADS1115_CQUE_1CONV) || (ads1115_comp == ADS1115_CQUE_2CONV) || (ads1115_comp == ADS1115_CQUE_4CONV))) {
			return luaL_error(L, "Invalid argument: conversion ready mode");
		}
		
		ads1115_threshold_low = 0x7FFF;
		ads1115_threshold_hi = 0x8000;
		
		write_reg(ADS1115_POINTER_THRESH_LOW, ads1115_threshold_low);
		write_reg(ADS1115_POINTER_THRESH_HI, ads1115_threshold_hi);
		
	} else if (lua_isnumber(L, 5) && lua_isnumber(L, 6) && lua_isnumber(L, 7)) {
		
		//	comparator mode
		ads1115_comp = luaL_checkinteger(L, 5);
		if (!((ads1115_comp == ADS1115_CQUE_1CONV) || (ads1115_comp == ADS1115_CQUE_2CONV) || (ads1115_comp == ADS1115_CQUE_4CONV))) {
			return luaL_error(L, "Invalid argument: comparator mode");
		}
		
		ads1115_threshold_low = luaL_checkinteger(L, 5);
		ads1115_threshold_hi = luaL_checkinteger(L, 6);
		
		if ((int16_t)ads1115_threshold_low > (int16_t)ads1115_threshold_hi) {
			return luaL_error(L, "Invalid argument: threshold_low > threshold_hi");
		}
		
		if (get_value(&ads1115_threshold_low)) {
			return luaL_error(L, "Invalid argument: threshold_low");
		}
		
		if (get_value(&ads1115_threshold_hi)) {
			return luaL_error(L, "Invalid argument: threshold_hi");
		}
		
		write_reg(ADS1115_POINTER_THRESH_LOW, ads1115_threshold_low);
		write_reg(ADS1115_POINTER_THRESH_HI, ads1115_threshold_hi);
		
	}
	
	ads1115_config = (ads1115_os | ads1115_channel | ads1115_gain | ads1115_mode | ads1115_samples | ADS1115_CMODE_TRAD | ADS1115_CPOL_ACTVLOW | ADS1115_CLAT_NONLAT | ads1115_comp);
	
	write_reg(ADS1115_POINTER_CONFIG, ads1115_config);
	
	return 0;
}

// Read the conversion register from the ADC
// Lua: 	ads1115.startread(function(volt, voltdec, adc) print(volt,voltdec,adc) end)
static int ads1115_lua_startread(lua_State *L) {
	
	if (((ads1115_comp == ADS1115_CQUE_1CONV) || (ads1115_comp == ADS1115_CQUE_2CONV) || (ads1115_comp == ADS1115_CQUE_4CONV)) && (ads1115_threshold_low == 0x7FFF) && (ads1115_threshold_hi == 0x8000)) {
		
		if (ads1115_mode == ADS1115_MODE_SINGLE) {
			write_reg(ADS1115_POINTER_CONFIG, ads1115_config);
		}
		
		return 0;
		
	} else {
		
		luaL_argcheck(L, (lua_type(L, 1) == LUA_TFUNCTION || lua_type(L, 1) == LUA_TLIGHTFUNCTION), 1, "Must be function");
		lua_pushvalue(L, 1);
		ads1115_timer_ref = luaL_ref(L, LUA_REGISTRYINDEX);
	
		if (ads1115_mode == ADS1115_MODE_SINGLE) {
			write_reg(ADS1115_POINTER_CONFIG, ads1115_config);
		}
	
		// Start a timer to wait until ADC conversion is done
		os_timer_disarm (&ads1115_timer);
		os_timer_setfn (&ads1115_timer, (os_timer_func_t *)ads1115_lua_readoutdone, NULL);
	
		switch (ads1115_samples) {
			case (ADS1115_DR_8SPS):
				os_timer_arm (&ads1115_timer, 150, 0);
				break;
			case (ADS1115_DR_16SPS):
				os_timer_arm (&ads1115_timer, 75, 0);
				break;
			case (ADS1115_DR_32SPS):
				os_timer_arm (&ads1115_timer, 35, 0);
				break;
			case (ADS1115_DR_64SPS):
				os_timer_arm (&ads1115_timer, 20, 0);
				break;
			case (ADS1115_DR_128SPS):
				os_timer_arm (&ads1115_timer, 10, 0);
				break;
			case (ADS1115_DR_250SPS):
				os_timer_arm (&ads1115_timer, 5, 0);
				break;
			case (ADS1115_DR_475SPS):
				os_timer_arm (&ads1115_timer, 3, 0);
				break;
			case (ADS1115_DR_860SPS):
				os_timer_arm (&ads1115_timer, 2, 0);
				break;
		}
	
		return 0;
	}
}

// adc conversion timer callback
static int ads1115_lua_readoutdone(void) {
	
	ads1115_conversion = read_reg(ADS1115_POINTER_CONVERSION);
	ads1115_volt = get_volt(ads1115_conversion);
	int ads1115_voltdec = (int)((ads1115_volt - (int)ads1115_volt) * 1000);
	ads1115_voltdec = ads1115_voltdec>0?ads1115_voltdec:0-ads1115_voltdec;
	
	lua_State *L = lua_getstate();
	os_timer_disarm (&ads1115_timer);
	
	lua_rawgeti (L, LUA_REGISTRYINDEX, ads1115_timer_ref);
	luaL_unref (L, LUA_REGISTRYINDEX, ads1115_timer_ref);
	ads1115_timer_ref = LUA_NOREF;
	
	lua_pushnumber(L, ads1115_volt);
	lua_pushinteger(L, ads1115_voltdec);
	lua_pushinteger(L, ads1115_conversion);
	
	lua_call (L, 3, 0);
}

// Read the conversion register from the ADC
// Lua: 	volt,voltdec,adc = ads1115.read()
static int ads1115_lua_read(lua_State *L) {
	
	ads1115_conversion = read_reg(ADS1115_POINTER_CONVERSION);
	ads1115_volt = get_volt(ads1115_conversion);
	int ads1115_voltdec = (int)((ads1115_volt - (int)ads1115_volt) * 1000);
	ads1115_voltdec = ads1115_voltdec>0?ads1115_voltdec:0-ads1115_voltdec;
	
	lua_pushnumber(L, ads1115_volt);
	lua_pushinteger(L, ads1115_voltdec);
	lua_pushinteger(L, ads1115_conversion);
	
	return 3;
}

static const LUA_REG_TYPE ads1115_map[] = {
	{	LSTRKEY( "setup" ),			LFUNCVAL(ads1115_lua_setup)		},
	{	LSTRKEY( "setting" ),		LFUNCVAL(ads1115_lua_setting)	},
	{	LSTRKEY( "startread" ),		LFUNCVAL(ads1115_lua_startread)	},
	{	LSTRKEY( "read" ),			LFUNCVAL(ads1115_lua_read)		},
	{	LSTRKEY( "ADDR_GND" ),		LNUMVAL(ADS1115_I2C_ADDR_GND)	},
	{	LSTRKEY( "ADDR_VDD" ),		LNUMVAL(ADS1115_I2C_ADDR_VDD)	},
	{	LSTRKEY( "ADDR_SDA" ),		LNUMVAL(ADS1115_I2C_ADDR_SDA)	},
	{	LSTRKEY( "ADDR_SCL" ),		LNUMVAL(ADS1115_I2C_ADDR_SCL)	},
	{	LSTRKEY( "SINGLE_SHOT" ),	LNUMVAL(ADS1115_MODE_SINGLE)	},
	{	LSTRKEY( "CONTINUOUS" ),	LNUMVAL(ADS1115_MODE_CONTIN)	},
	{	LSTRKEY( "DIFF_0_1" ),		LNUMVAL(ADS1115_MUX_DIFF_0_1)	},
	{	LSTRKEY( "DIFF_0_3" ),		LNUMVAL(ADS1115_MUX_DIFF_0_3)	},
	{	LSTRKEY( "DIFF_1_3" ),		LNUMVAL(ADS1115_MUX_DIFF_1_3)	},
	{	LSTRKEY( "DIFF_2_3" ),		LNUMVAL(ADS1115_MUX_DIFF_2_3)	},
	{	LSTRKEY( "SINGLE_0" ),		LNUMVAL(ADS1115_MUX_SINGLE_0)	},
	{	LSTRKEY( "SINGLE_1" ),		LNUMVAL(ADS1115_MUX_SINGLE_1)	},
	{	LSTRKEY( "SINGLE_2" ),		LNUMVAL(ADS1115_MUX_SINGLE_2)	},
	{	LSTRKEY( "SINGLE_3" ),		LNUMVAL(ADS1115_MUX_SINGLE_3)	},
	{	LSTRKEY( "GAIN_6_144V" ),	LNUMVAL(ADS1115_PGA_6_144V)		},
	{	LSTRKEY( "GAIN_4_096V" ),	LNUMVAL(ADS1115_PGA_4_096V)		},
	{	LSTRKEY( "GAIN_2_048V" ),	LNUMVAL(ADS1115_PGA_2_048V)		},
	{	LSTRKEY( "GAIN_1_024V" ),	LNUMVAL(ADS1115_PGA_1_024V)		},
	{	LSTRKEY( "GAIN_0_512V" ),	LNUMVAL(ADS1115_PGA_0_512V)		},
	{	LSTRKEY( "GAIN_0_256V" ),	LNUMVAL(ADS1115_PGA_0_256V)		},
	{	LSTRKEY( "DR_8SPS" ),		LNUMVAL(ADS1115_DR_8SPS)		},
	{	LSTRKEY( "DR_16SPS" ),		LNUMVAL(ADS1115_DR_16SPS)		},
	{	LSTRKEY( "DR_32SPS" ),		LNUMVAL(ADS1115_DR_32SPS)		},
	{	LSTRKEY( "DR_64SPS" ),		LNUMVAL(ADS1115_DR_64SPS)		},
	{	LSTRKEY( "DR_128SPS" ),		LNUMVAL(ADS1115_DR_128SPS)		},
	{	LSTRKEY( "DR_250SPS" ),		LNUMVAL(ADS1115_DR_250SPS)		},
	{	LSTRKEY( "DR_475SPS" ),		LNUMVAL(ADS1115_DR_475SPS)		},
	{	LSTRKEY( "DR_860SPS" ),		LNUMVAL(ADS1115_DR_860SPS)		},
	{	LSTRKEY( "CONV_RDY_1" ),	LNUMVAL(ADS1115_CQUE_1CONV)		},
	{	LSTRKEY( "CONV_RDY_2" ),	LNUMVAL(ADS1115_CQUE_2CONV)		},
	{	LSTRKEY( "CONV_RDY_4" ),	LNUMVAL(ADS1115_CQUE_4CONV)		},
	{	LSTRKEY( "COMP_1CONV" ),	LNUMVAL(ADS1115_CQUE_1CONV)		},
	{	LSTRKEY( "COMP_2CONV" ),	LNUMVAL(ADS1115_CQUE_2CONV)		},
	{	LSTRKEY( "COMP_4CONV" ),	LNUMVAL(ADS1115_CQUE_4CONV)		},
	{	LNILKEY, LNILVAL											}
};

NODEMCU_MODULE(ADS1115, "ads1115", ads1115_map, NULL);