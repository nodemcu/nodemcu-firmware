/*
 * tsl2561.c
 *
 *  Created on: Aug 21, 2015
 *  Author: Michael Lucas (Aeprox @github)
 */
#include "module.h"
#include "lauxlib.h"
#include "platform.h"
#include "../tsl2561/tsl2561.h"

static uint16_t ch0;
static uint16_t ch1;

/* Initialises the device on pins sdapin and sclpin
 * Lua: 	status = tsl2561.init(sdapin, sclpin, address(optional), package(optional))
 */
static int ICACHE_FLASH_ATTR tsl2561_init(lua_State* L) {
	uint32_t sda;
	uint32_t scl;
	// check parameters
	if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2)) {
		return luaL_error(L, "wrong arg range");
	}

	sda = luaL_checkinteger(L, 1);
	scl = luaL_checkinteger(L, 2);

	if (scl == 0 || sda == 0) {
		return luaL_error(L, "no i2c for D0");
	}
	// init I2C
	uint8_t error = tsl2561Init(sda, scl);

	// Parse optional parameters
	if (lua_isnumber(L, 3)) {
		uint8_t address = luaL_checkinteger(L, 3);
		if (!((address == TSL2561_ADDRESS_GND) || (address == TSL2561_ADDRESS_FLOAT) || (address == TSL2561_ADDRESS_VDD))) {
			return luaL_error(L, "Invalid argument: address");
		}
		else{
			tsl2561SetAddress(address);
		}
	}
	if (lua_isnumber(L, 4)) {
		uint8_t package = luaL_checkinteger(L, 4);
		if (!((package == TSL2561_PACKAGE_T_FN_CL) || (package == TSL2561_PACKAGE_CS))) {
			return luaL_error(L, "Invalid argument: package");
		}
		else{
			tsl2561SetPackage(package);
		}
	}
	lua_pushinteger(L, error);
	return 1;
}
/* Sets the integration time and gain settings of the device
 * Lua: 	status = tsl2561.settiming(integration, gain)
 */
static int ICACHE_FLASH_ATTR tsl2561_lua_settiming(lua_State* L) {
	// check variables
	if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2)) {
		return luaL_error(L, "wrong arg range");
	}
	uint8_t integration = luaL_checkinteger(L, 1);
	if (!((integration == TSL2561_INTEGRATIONTIME_13MS) || (integration == TSL2561_INTEGRATIONTIME_101MS) || (integration == TSL2561_INTEGRATIONTIME_402MS))) {
		return luaL_error(L, "Invalid argument: integration");
	}
	uint8_t gain = luaL_checkinteger(L, 2);
	if (!((gain == TSL2561_GAIN_16X) || (gain == TSL2561_GAIN_1X))) {
		return luaL_error(L, "Invalid argument: gain");
	}

	lua_pushinteger(L, tsl2561SetTiming(integration, gain));
	return 1;
}
/* Reads sensor values from device and return calculated lux
 * Lua: 	lux, status = tsl2561.getlux()
 */
static int ICACHE_FLASH_ATTR tsl2561_lua_calclux(lua_State* L) {
	uint8_t error = tsl2561GetLuminosity(&ch0, &ch1);
	if (error) {
		lua_pushinteger(L, 0);
		lua_pushinteger(L, error);
	} else {
		lua_pushinteger(L, tsl2561CalculateLux(ch0, ch1));
		lua_pushinteger(L, error);
	}
	return 2;
}
/* Reads sensor values from device and returns them
 * Lua: 	ch0, ch1, status = tsl2561.getrawchannels()
 */
static int ICACHE_FLASH_ATTR tsl2561_lua_getchannels(lua_State* L) {
	uint8_t error = tsl2561GetLuminosity(&ch0, &ch1);
	lua_pushinteger(L, ch0);
	lua_pushinteger(L, ch1);
	lua_pushinteger(L, error);

	return 3;
}

// Module function map
LROT_BEGIN(tsl2561, NULL, 0)
  LROT_FUNCENTRY( settiming, tsl2561_lua_settiming )
  LROT_FUNCENTRY( getlux, tsl2561_lua_calclux )
  LROT_FUNCENTRY( getrawchannels, tsl2561_lua_getchannels )
  LROT_FUNCENTRY( init, tsl2561_init )
  LROT_NUMENTRY( TSL2561_OK, TSL2561_ERROR_OK )
  LROT_NUMENTRY( TSL2561_ERROR_I2CINIT, TSL2561_ERROR_I2CINIT )
  LROT_NUMENTRY( TSL2561_ERROR_I2CBUSY, TSL2561_ERROR_I2CBUSY )
  LROT_NUMENTRY( TSL2561_ERROR_NOINIT, TSL2561_ERROR_NOINIT )
  LROT_NUMENTRY( TSL2561_ERROR_LAST, TSL2561_ERROR_LAST )
  LROT_NUMENTRY( INTEGRATIONTIME_13MS, TSL2561_INTEGRATIONTIME_13MS )
  LROT_NUMENTRY( INTEGRATIONTIME_101MS, TSL2561_INTEGRATIONTIME_101MS )
  LROT_NUMENTRY( INTEGRATIONTIME_402MS, TSL2561_INTEGRATIONTIME_402MS )
  LROT_NUMENTRY( GAIN_1X, TSL2561_GAIN_1X )
  LROT_NUMENTRY( GAIN_16X, TSL2561_GAIN_16X )
  LROT_NUMENTRY( PACKAGE_CS, TSL2561_PACKAGE_CS )
  LROT_NUMENTRY( PACKAGE_T_FN_CL, TSL2561_PACKAGE_T_FN_CL )
  LROT_NUMENTRY( ADDRESS_GND, TSL2561_ADDRESS_GND )
  LROT_NUMENTRY( ADDRESS_FLOAT, TSL2561_ADDRESS_FLOAT )
  LROT_NUMENTRY( ADDRESS_VDD, TSL2561_ADDRESS_VDD )
LROT_END(tsl2561, NULL, 0)


NODEMCU_MODULE(TSL2561, "tsl2561", tsl2561, NULL);
