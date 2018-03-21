/**************************************************************************/
/*! 
 @file     tsl2561.c
 @author   K. Townsend (microBuilder.eu)/ Adapted for nodeMCU by Michael Lucas (Aeprox @github)
 
 @brief    Drivers for the TAOS TSL2561 I2C digital luminosity sensor

 @section DESCRIPTION

 The TSL2561 is a 16-bit digital luminosity sensor the approximates
 the human eye's response to light.  It contains one broadband
 photodiode that measures visible plus infrared light (channel 0)
 and one infrared photodiode (channel 1).

 @section EXAMPLE

 @code
 #include "drivers/sensors/tsl2561/tsl2561.h"
 ...
 uint16_t broadband, ir;
 uint32_t lux;

 // Initialise luminosity sensor
 tsl2561Init(sda_pin, scl_pin);

 // Optional ... default setting is 400ms with no gain
 // Set timing to 101ms with no gain
 tsl2561SetTiming(TSL2561_INTEGRATIONTIME_101MS, TSL2561_GAIN_0X);

 // Check luminosity level and calculate lux
 tsl2561GetLuminosity(&broadband, &ir);
 lux = tsl2561CalculateLux(broadband, ir);
 printf("Broadband: %u, IR: %u, Lux: %d %s", broadband, ir, lux, CFG_PRINTF_NEWLINE);

 @endcode

 @section LICENSE

 Software License Agreement (BSD License)

 Copyright (c) 2010, microBuilder SARL
 All rights reserved.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:
 1. Redistributions of source code must retain the above copyright
 notice, this list of conditions and the following disclaimer.
 2. Redistributions in binary form must reproduce the above copyright
 notice, this list of conditions and the following disclaimer in the
 documentation and/or other materials provided with the distribution.
 3. Neither the name of the copyright holders nor the
 names of its contributors may be used to endorse or promote products
 derived from this software without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ''AS IS'' AND ANY
 EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE FOR ANY
 DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/**************************************************************************/
#include "tsl2561.h"
#include "platform.h"
#include "user_interface.h"
#include "osapi.h"

static const uint32_t tsl2561_i2c_id = 0;
static bool _tsl2561Initialised = 0;
static tsl2561IntegrationTime_t _tsl2561IntegrationTime = TSL2561_INTEGRATIONTIME_402MS;
static tsl2561Gain_t _tsl2561Gain = TSL2561_GAIN_1X;
static tsl2561Address_t tsl2561Address = TSL2561_ADDRESS_FLOAT;
static tsl2561Package_t tsl2561Package = TSL2561_PACKAGE_T_FN_CL;

/**************************************************************************/
/*! 
 @brief  Writes an 8 bit values over I2C
 */
/**************************************************************************/
tsl2561Error_t tsl2561Write8(uint8_t reg, uint8_t value) {
	platform_i2c_send_start(tsl2561_i2c_id);
	platform_i2c_send_address(tsl2561_i2c_id, tsl2561Address, PLATFORM_I2C_DIRECTION_TRANSMITTER);
	platform_i2c_send_byte(tsl2561_i2c_id, reg);
	platform_i2c_send_byte(tsl2561_i2c_id, value);
	platform_i2c_send_stop(tsl2561_i2c_id);
	return TSL2561_ERROR_OK;
}

/**************************************************************************/
/*! 
 @brief  Reads a 16 bit values over I2C
 */
/**************************************************************************/
tsl2561Error_t tsl2561Read16(uint8_t reg, uint16_t *value) {
	platform_i2c_send_start(tsl2561_i2c_id);
	platform_i2c_send_address(tsl2561_i2c_id, tsl2561Address, PLATFORM_I2C_DIRECTION_TRANSMITTER);
	platform_i2c_send_byte(tsl2561_i2c_id, reg);
	platform_i2c_send_stop(tsl2561_i2c_id);

	platform_i2c_send_start(tsl2561_i2c_id);
	platform_i2c_send_address(tsl2561_i2c_id, tsl2561Address, PLATFORM_I2C_DIRECTION_RECEIVER);
	uint8_t ch_low = platform_i2c_recv_byte(tsl2561_i2c_id, 0);
	platform_i2c_send_stop(tsl2561_i2c_id);

	platform_i2c_send_start(tsl2561_i2c_id);
	platform_i2c_send_address(tsl2561_i2c_id, tsl2561Address, PLATFORM_I2C_DIRECTION_TRANSMITTER);
	platform_i2c_send_byte(tsl2561_i2c_id, reg + 1);
	platform_i2c_send_stop(tsl2561_i2c_id);

	platform_i2c_send_start(tsl2561_i2c_id);
	platform_i2c_send_address(tsl2561_i2c_id, tsl2561Address, PLATFORM_I2C_DIRECTION_RECEIVER);
	uint8_t ch_high = platform_i2c_recv_byte(tsl2561_i2c_id, 0);
	platform_i2c_send_stop(tsl2561_i2c_id);

	// Shift values to create properly formed integer (low byte first)
	*value = (ch_low | (ch_high << 8));

	return TSL2561_ERROR_OK;
}

/**************************************************************************/
/*! 
 @brief  Enables the device
 */
/**************************************************************************/
tsl2561Error_t tsl2561Enable(void) {
	if (!_tsl2561Initialised)
		return TSL2561_ERROR_NOINIT;

	// Enable the device by setting the control bit to 0x03
	return tsl2561Write8(TSL2561_COMMAND_BIT | TSL2561_REGISTER_CONTROL,
	TSL2561_CONTROL_POWERON);
}

/**************************************************************************/
/*! 
 @brief  Disables the device (putting it in lower power sleep mode)
 */
/**************************************************************************/
tsl2561Error_t tsl2561Disable(void) {
	if (!_tsl2561Initialised)
		return TSL2561_ERROR_NOINIT;

	// Turn the device off to save power
	return tsl2561Write8(TSL2561_COMMAND_BIT | TSL2561_REGISTER_CONTROL,
	TSL2561_CONTROL_POWEROFF);
}

void tsl2561SetAddress(uint8_t address){
	tsl2561Address = address;
	return;
}
void tsl2561SetPackage(uint8_t package){
	tsl2561Package = package;
	return;
}

/**************************************************************************/
/*! 
 @brief  Initialises the I2C block
 */
/**************************************************************************/
tsl2561Error_t tsl2561Init(uint8_t sda, uint8_t scl) {
	// Initialise I2C
	platform_i2c_setup(tsl2561_i2c_id, sda, scl, PLATFORM_I2C_SPEED_SLOW);

	_tsl2561Initialised = 1;

	// Set default integration time and gain
	tsl2561SetTiming(_tsl2561IntegrationTime, _tsl2561Gain);

	// Note: by default, the device is in power down mode on bootup

	return TSL2561_ERROR_OK;
}

/**************************************************************************/
/*! 
 @brief  Sets the integration time and gain (controls sensitivity)
 */
/**************************************************************************/
tsl2561Error_t tsl2561SetTiming(tsl2561IntegrationTime_t integration, tsl2561Gain_t gain) {
	if (!_tsl2561Initialised)
		return TSL2561_ERROR_NOINIT;

	tsl2561Error_t error = TSL2561_ERROR_OK;

	// Enable the device by setting the control bit to 0x03
	error = tsl2561Enable();
	if (error)
		return error;

	// set timing and gain on device
	error = tsl2561Write8(TSL2561_COMMAND_BIT | TSL2561_REGISTER_TIMING, integration | gain);
	if (error)
		return error;

	// Update value placeholders
	_tsl2561IntegrationTime = integration;
	_tsl2561Gain = gain;

	// Turn the device off to save power
	error = tsl2561Disable();
	if (error)
		return error;

	return error;
}

/**************************************************************************/
/*! 
 @brief  Reads the luminosity on both channels from the TSL2561
 */
/**************************************************************************/
tsl2561Error_t tsl2561GetLuminosity(uint16_t *broadband, uint16_t *ir) {
	if (!_tsl2561Initialised)
		return TSL2561_ERROR_NOINIT;

	tsl2561Error_t error = TSL2561_ERROR_OK;

	// Enable the device by setting the control bit to 0x03
	error = tsl2561Enable();
	if (error)
		return error;

	// Wait x ms for ADC to complete
	switch (_tsl2561IntegrationTime) {
	case TSL2561_INTEGRATIONTIME_13MS:
		os_delay_us(14000); //systickDelay(14);
		break;
	case TSL2561_INTEGRATIONTIME_101MS:
		os_delay_us(102000); //systickDelay(102);
		break;
	default:
		os_delay_us(404000); //systickDelay(404);
		break;
	}

	// Reads two byte value from channel 0 (visible + infrared)
	error = tsl2561Read16(
	TSL2561_COMMAND_BIT | TSL2561_WORD_BIT | TSL2561_REGISTER_CHAN0_LOW, broadband);
	if (error)
		return error;

	// Reads two byte value from channel 1 (infrared)
	error = tsl2561Read16(
	TSL2561_COMMAND_BIT | TSL2561_WORD_BIT | TSL2561_REGISTER_CHAN1_LOW, ir);
	if (error)
		return error;

	// Turn the device off to save power
	error = tsl2561Disable();
	if (error)
		return error;

	return error;
}

/**************************************************************************/
/*! 
 @brief  Calculates LUX from the supplied ch0 (broadband) and ch1 
 (IR) readings
 */
/**************************************************************************/
uint32_t tsl2561CalculateLux(uint16_t ch0, uint16_t ch1) {
	unsigned long chScale;
	unsigned long channel1;
	unsigned long channel0;

	switch (_tsl2561IntegrationTime) {
	case TSL2561_INTEGRATIONTIME_13MS:
		chScale = TSL2561_LUX_CHSCALE_TINT0;
		break;
	case TSL2561_INTEGRATIONTIME_101MS:
		chScale = TSL2561_LUX_CHSCALE_TINT1;
		break;
	default: // No scaling ... integration time = 402ms
		chScale = (1 << TSL2561_LUX_CHSCALE);
		break;
	}

	// Scale for gain (1x or 16x)
	if (!_tsl2561Gain)
		chScale = chScale << 4;

	// scale the channel values
	channel0 = (ch0 * chScale) >> TSL2561_LUX_CHSCALE;
	channel1 = (ch1 * chScale) >> TSL2561_LUX_CHSCALE;

	// find the ratio of the channel values (Channel1/Channel0)
	unsigned long ratio1 = 0;
	if (channel0 != 0)
		ratio1 = (channel1 << (TSL2561_LUX_RATIOSCALE + 1)) / channel0;

	// round the ratio value
	unsigned long ratio = (ratio1 + 1) >> 1;

	unsigned int b, m;

	if (tsl2561Package == TSL2561_PACKAGE_CS){
		if ((ratio >= 0) && (ratio <= TSL2561_LUX_K1C)) {
			b = TSL2561_LUX_B1C;
			m = TSL2561_LUX_M1C;
		} else if (ratio <= TSL2561_LUX_K2C) {
			b = TSL2561_LUX_B2C;
			m = TSL2561_LUX_M2C;
		} else if (ratio <= TSL2561_LUX_K3C) {
			b = TSL2561_LUX_B3C;
			m = TSL2561_LUX_M3C;
		} else if (ratio <= TSL2561_LUX_K4C) {
			b = TSL2561_LUX_B4C;
			m = TSL2561_LUX_M4C;
		} else if (ratio <= TSL2561_LUX_K5C) {
			b = TSL2561_LUX_B5C;
			m = TSL2561_LUX_M5C;
		} else if (ratio <= TSL2561_LUX_K6C) {
			b = TSL2561_LUX_B6C;
			m = TSL2561_LUX_M6C;
		} else if (ratio <= TSL2561_LUX_K7C) {
			b = TSL2561_LUX_B7C;
			m = TSL2561_LUX_M7C;
		} else if (ratio > TSL2561_LUX_K8C) {
			b = TSL2561_LUX_B8C;
			m = TSL2561_LUX_M8C;
		}
	}
	else{
		if ((ratio >= 0) && (ratio <= TSL2561_LUX_K1T))
		{	b=TSL2561_LUX_B1T; m=TSL2561_LUX_M1T;}
		else if (ratio <= TSL2561_LUX_K2T)
		{	b=TSL2561_LUX_B2T; m=TSL2561_LUX_M2T;}
		else if (ratio <= TSL2561_LUX_K3T)
		{	b=TSL2561_LUX_B3T; m=TSL2561_LUX_M3T;}
		else if (ratio <= TSL2561_LUX_K4T)
		{	b=TSL2561_LUX_B4T; m=TSL2561_LUX_M4T;}
		else if (ratio <= TSL2561_LUX_K5T)
		{	b=TSL2561_LUX_B5T; m=TSL2561_LUX_M5T;}
		else if (ratio <= TSL2561_LUX_K6T)
		{	b=TSL2561_LUX_B6T; m=TSL2561_LUX_M6T;}
		else if (ratio <= TSL2561_LUX_K7T)
		{	b=TSL2561_LUX_B7T; m=TSL2561_LUX_M7T;}
		else if (ratio > TSL2561_LUX_K8T)
		{	b=TSL2561_LUX_B8T; m=TSL2561_LUX_M8T;}
	}

	unsigned long temp;
	temp = ((channel0 * b) - (channel1 * m));

	// do not allow negative lux value
	if (temp < 0)
		temp = 0;

	// round lsb (2^(LUX_SCALE-1))
	temp += (1 << (TSL2561_LUX_LUXSCALE - 1));

	// strip off fractional portion
	uint32_t lux = temp >> TSL2561_LUX_LUXSCALE;

	// Signal I2C had no errors
	return lux;
}
