/*
 * tsl2561.c
 *
 *  Created on: Aug 21, 2015
 *  Author: Michael Lucas (Aeprox @github)
 */
#include "lualib.h"
#include "lauxlib.h"
#include "platform.h"
#include "auxmods.h"
#include "lrotable.h"
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
	lua_pushnumber(L, error);
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

	lua_pushnumber(L, tsl2561SetTiming(integration, gain));
	return 1;
}
/* Reads sensor values from device and return calculated lux
 * Lua: 	lux, status = tsl2561.getlux()
 */
static int ICACHE_FLASH_ATTR tsl2561_lua_calclux(lua_State* L) {
	uint8_t error = tsl2561GetLuminosity(&ch0, &ch1);
	if (error) {
		lua_pushnumber(L, 0);
		lua_pushnumber(L, error);
	} else {
		lua_pushnumber(L, tsl2561CalculateLux(ch0, ch1));
		lua_pushnumber(L, error);
	}
	return 2;
}
/* Reads sensor values from device and returns them
 * Lua: 	ch0, ch1, status = tsl2561.getrawchannels()
 */
static int ICACHE_FLASH_ATTR tsl2561_lua_getchannels(lua_State* L) {
	uint8_t error = tsl2561GetLuminosity(&ch0, &ch1);
	lua_pushnumber(L, ch0);
	lua_pushnumber(L, ch1);
	lua_pushnumber(L, error);

	return 3;
}

#define MIN_OPT_LEVEL 2
#include "lrodefs.h"
const LUA_REG_TYPE tsl2561_map[] =
{
	{	LSTRKEY( "settiming" ), LFUNCVAL( tsl2561_lua_settiming)},
	{	LSTRKEY( "getlux" ), LFUNCVAL( tsl2561_lua_calclux )},
	{	LSTRKEY( "getrawchannels" ), LFUNCVAL( tsl2561_lua_getchannels )},
	{	LSTRKEY( "init" ), LFUNCVAL( tsl2561_init )},

#if LUA_OPTIMIZE_MEMORY > 0
	{	LSTRKEY( "TSL2561_OK" ), LNUMVAL( TSL2561_ERROR_OK )},

	{	LSTRKEY( "TSL2561_ERROR_I2CINIT" ), LNUMVAL( TSL2561_ERROR_I2CINIT )},
	{	LSTRKEY( "TSL2561_ERROR_I2CBUSY" ), LNUMVAL( TSL2561_ERROR_I2CBUSY )},
	{	LSTRKEY( "TSL2561_ERROR_NOINIT" ), LNUMVAL( TSL2561_ERROR_NOINIT )},
	{	LSTRKEY( "TSL2561_ERROR_LAST" ), LNUMVAL( TSL2561_ERROR_LAST )},

	{	LSTRKEY( "INTEGRATIONTIME_13MS" ), LNUMVAL( TSL2561_INTEGRATIONTIME_13MS )},
	{	LSTRKEY( "INTEGRATIONTIME_101MS" ), LNUMVAL( TSL2561_INTEGRATIONTIME_101MS )},
	{	LSTRKEY( "INTEGRATIONTIME_402MS" ), LNUMVAL( TSL2561_INTEGRATIONTIME_402MS )},
	{	LSTRKEY( "GAIN_1X" ), LNUMVAL( TSL2561_GAIN_1X )},
	{	LSTRKEY( "GAIN_16X" ), LNUMVAL( TSL2561_GAIN_16X )},

	{	LSTRKEY( "PACKAGE_CS" ), LNUMVAL( TSL2561_PACKAGE_CS )},
	{	LSTRKEY( "PACKAGE_T_FN_CL" ), LNUMVAL( TSL2561_PACKAGE_T_FN_CL )},

	{	LSTRKEY( "ADDRESS_GND" ), LNUMVAL( TSL2561_ADDRESS_GND )},
	{	LSTRKEY( "ADDRESS_FLOAT" ), LNUMVAL( TSL2561_ADDRESS_FLOAT )},
	{	LSTRKEY( "ADDRESS_VDD" ), LNUMVAL( TSL2561_ADDRESS_VDD )},
#endif

	{	LNILKEY, LNILVAL}
};

LUALIB_API int luaopen_tsl2561(lua_State *L) {
#if LUA_OPTIMIZE_MEMORY > 0

#else // #if LUA_OPTIMIZE_MEMORY > 0
	luaL_register(L, "tsl2561", tsl2561_map);

	MOD_REG_NUMBER(L, "TSL2561_OK", TSL2561_ERROR_OK);

	MOD_REG_NUMBER(L, "TSL2561_ERROR_I2CINIT", TSL2561_ERROR_I2CINIT);
	MOD_REG_NUMBER(L, "TSL2561_ERROR_I2CBUSY", TSL2561_ERROR_I2CBUSY);
	MOD_REG_NUMBER(L, "TSL2561_ERROR_NOINIT", TSL2561_ERROR_NOINIT);
	MOD_REG_NUMBER(L, "TSL2561_ERROR_LAST", TSL2561_ERROR_LAST);

	MOD_REG_NUMBER(L, "INTEGRATIONTIME_13MS", TSL2561_INTEGRATIONTIME_13MS);
	MOD_REG_NUMBER(L, "INTEGRATIONTIME_101MS", TSL2561_INTEGRATIONTIME_101MS);
	MOD_REG_NUMBER(L, "INTEGRATIONTIME_402MS", TSL2561_INTEGRATIONTIME_402MS);
	MOD_REG_NUMBER(L, "GAIN_1X", TSL2561_GAIN_1X);
	MOD_REG_NUMBER(L, "GAIN_16X", TSL2561_GAIN_16X);

	MOD_REG_NUMBER(L, "PACKAGE_CS", TSL2561_PACKAGE_CS);
	MOD_REG_NUMBER(L, "PACKAGE_T_FN_CL", TSL2561_PACKAGE_T_FN_CL);

	MOD_REG_NUMBER(L, "ADDRESS_GND", TSL2561_ADDRESS_GND);
	MOD_REG_NUMBER(L, "ADDRESS_FLOAT", TSL2561_ADDRESS_FLOAT);
	MOD_REG_NUMBER(L, "ADDRESS_VDD", TSL2561_ADDRESS_VDD);

	return 1;
#endif // #if LUA_OPTIMIZE_MEMORY > 0
}
