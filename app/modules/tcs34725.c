// ***************************************************************************
// TCS34725 module for ESP8266 with nodeMCU
// 
// Written by K. Townsend (microBuilder.eu), Adapted for nodeMCU by Travis Howse (tjhowse gmail.com)
// 
// BSD (see license.txt)
// ***************************************************************************

// Original header:
/**************************************************************************/
/*!
		@file		 tcs34725.c
		@author	 K. Townsend (microBuilder.eu)
		@ingroup	Sensors
		@brief		Driver for the TAOS TCS34725 I2C digital RGB/color sensor
		@license	BSD (see license.txt)
*/
/**************************************************************************/

//#define NODE_DEBUG

#include "module.h"
#include "lauxlib.h"
#include "platform.h"
#include "c_math.h"

// #define TCS34725_ADDRESS					(0x29<<1)
#define TCS34725_ADDRESS					(0x29)
#define TCS34725_BUS_ID							(0x00) 		/* ?? Not sure what this is for . Nodemcu I2C bus ID? */
#define TCS34725_READBIT					(0x01)

#define TCS34725_COMMAND_BIT			(0x80)

#define TCS34725_ENABLE					 (0x00)
#define TCS34725_ENABLE_AIEN			(0x10)			/* RGBC Interrupt Enable */
#define TCS34725_ENABLE_WEN			 (0x08)				/* Wait enable - Writing 1 activates the wait timer */
#define TCS34725_ENABLE_AEN			 (0x02)				/* RGBC Enable - Writing 1 actives the ADC, 0 disables it */
#define TCS34725_ENABLE_PON			 (0x01)				/* Power on - Writing 1 activates the internal oscillator, 0 disables it */
#define TCS34725_ATIME						(0x01)		/* Integration time */
#define TCS34725_WTIME						(0x03)		/* Wait time (if TCS34725_ENABLE_WEN is asserted) */
#define TCS34725_WTIME_2_4MS			(0xFF)			/* WLONG0 = 2.4ms	 WLONG1 = 0.029s */
#define TCS34725_WTIME_204MS			(0xAB)			/* WLONG0 = 204ms	 WLONG1 = 2.45s	*/
#define TCS34725_WTIME_614MS			(0x00)			/* WLONG0 = 614ms	 WLONG1 = 7.4s	 */
#define TCS34725_AILTL						(0x04)		/* Clear channel lower interrupt threshold */
#define TCS34725_AILTH						(0x05)
#define TCS34725_AIHTL						(0x06)		/* Clear channel upper interrupt threshold */
#define TCS34725_AIHTH						(0x07)
#define TCS34725_PERS						 (0x0C)		/* Persistence register - basic SW filtering mechanism for interrupts */
#define TCS34725_PERS_NONE				(0b0000)	/* Every RGBC cycle generates an interrupt																*/
#define TCS34725_PERS_1_CYCLE		 (0b0001)	/* 1 clean channel value outside threshold range generates an interrupt	 */
#define TCS34725_PERS_2_CYCLE		 (0b0010)	/* 2 clean channel values outside threshold range generates an interrupt	*/
#define TCS34725_PERS_3_CYCLE		 (0b0011)	/* 3 clean channel values outside threshold range generates an interrupt	*/
#define TCS34725_PERS_5_CYCLE		 (0b0100)	/* 5 clean channel values outside threshold range generates an interrupt	*/
#define TCS34725_PERS_10_CYCLE		(0b0101)	/* 10 clean channel values outside threshold range generates an interrupt */
#define TCS34725_PERS_15_CYCLE		(0b0110)	/* 15 clean channel values outside threshold range generates an interrupt */
#define TCS34725_PERS_20_CYCLE		(0b0111)	/* 20 clean channel values outside threshold range generates an interrupt */
#define TCS34725_PERS_25_CYCLE		(0b1000)	/* 25 clean channel values outside threshold range generates an interrupt */
#define TCS34725_PERS_30_CYCLE		(0b1001)	/* 30 clean channel values outside threshold range generates an interrupt */
#define TCS34725_PERS_35_CYCLE		(0b1010)	/* 35 clean channel values outside threshold range generates an interrupt */
#define TCS34725_PERS_40_CYCLE		(0b1011)	/* 40 clean channel values outside threshold range generates an interrupt */
#define TCS34725_PERS_45_CYCLE		(0b1100)	/* 45 clean channel values outside threshold range generates an interrupt */
#define TCS34725_PERS_50_CYCLE		(0b1101)	/* 50 clean channel values outside threshold range generates an interrupt */
#define TCS34725_PERS_55_CYCLE		(0b1110)	/* 55 clean channel values outside threshold range generates an interrupt */
#define TCS34725_PERS_60_CYCLE		(0b1111)	/* 60 clean channel values outside threshold range generates an interrupt */
#define TCS34725_CONFIG					 (0x0D)
#define TCS34725_CONFIG_WLONG		 (0x02)		/* Choose between short and long (12x) wait times via TCS34725_WTIME */
#define TCS34725_CONTROL					(0x0F)		/* Set the gain level for the sensor */
#define TCS34725_ID							 (0x12)		/* 0x44 = TCS34721/TCS34725, 0x4D = TCS34723/TCS34727 */
#define TCS34725_STATUS					 (0x13)
#define TCS34725_STATUS_AINT			(0x10)		/* RGBC Clean channel interrupt */
#define TCS34725_STATUS_AVALID		(0x01)		/* Indicates that the RGBC channels have completed an integration cycle */
#define TCS34725_CDATAL					 (0x14)		/* Clear channel data */
#define TCS34725_CDATAH					 (0x15)
#define TCS34725_RDATAL					 (0x16)		/* Red channel data */
#define TCS34725_RDATAH					 (0x17)
#define TCS34725_GDATAL					 (0x18)		/* Green channel data */
#define TCS34725_GDATAH					 (0x19)
#define TCS34725_BDATAL					 (0x1A)		/* Blue channel data */
#define TCS34725_BDATAH					 (0x1B)

#define TCS34725_EN_DELAY				30

typedef enum
{
	TCS34725_INTEGRATIONTIME_2_4MS	= 0xFF,	 /**<	2.4ms - 1 cycle		- Max Count: 1024	*/
	TCS34725_INTEGRATIONTIME_24MS	 = 0xF6,	 /**<	24ms	- 10 cycles	- Max Count: 10240 */
	TCS34725_INTEGRATIONTIME_101MS	= 0xD5,	 /**<	101ms - 42 cycles	- Max Count: 43008 */
	TCS34725_INTEGRATIONTIME_154MS	= 0xC0,	 /**<	154ms - 64 cycles	- Max Count: 65535 */
	TCS34725_INTEGRATIONTIME_700MS	= 0x00		/**<	700ms - 256 cycles - Max Count: 65535 */
}
tcs34725IntegrationTime_t;

typedef enum
{
	TCS34725_GAIN_1X								= 0x00,	 /**<	No gain	*/
	TCS34725_GAIN_4X								= 0x01,	 /**<	2x gain	*/
	TCS34725_GAIN_16X							 = 0x02,	 /**<	16x gain */
	TCS34725_GAIN_60X							 = 0x03		/**<	60x gain */
}
tcs34725Gain_t;
static void temp_setup_debug(int line, const char *str);
uint8_t tcs34725Setup(lua_State* L);
uint8_t tcs34725Enable(lua_State* L);
uint8_t tcs34725Disable(lua_State* L);
uint8_t tcs34725GetRawData(lua_State* L);
uint8_t tcs34725LuaSetIntegrationTime(lua_State* L);
uint8_t tcs34725SetIntegrationTime(tcs34725IntegrationTime_t it, lua_State* L);
uint8_t tcs34725LuaSetGain(lua_State* L);
uint8_t tcs34725SetGain(tcs34725Gain_t gain, lua_State* L);

static bool							_tcs34725Initialised = false;
static int32_t						_tcs34725SensorID = 0;
static tcs34725Gain_t				_tcs34725Gain = TCS34725_GAIN_1X;
static tcs34725IntegrationTime_t	_tcs34725IntegrationTime = TCS34725_INTEGRATIONTIME_2_4MS;

os_timer_t tcs34725_timer; // timer for forced mode readout
sint32_t cb_tcs_en;

/**************************************************************************/
/*!
		@brief	Writes an 8 bit values over I2C
*/
/**************************************************************************/
uint8_t tcs34725Write8 (uint8_t reg, uint8_t value)
{
	platform_i2c_send_start(TCS34725_BUS_ID);
	platform_i2c_send_address(TCS34725_BUS_ID, TCS34725_ADDRESS, PLATFORM_I2C_DIRECTION_TRANSMITTER);
	platform_i2c_send_byte(TCS34725_BUS_ID, TCS34725_COMMAND_BIT | reg );
	platform_i2c_send_byte(TCS34725_BUS_ID, value);
	platform_i2c_send_stop(TCS34725_BUS_ID);
	return 0;
}

/**************************************************************************/
/*!
		@brief	Reads a 8 bit values over I2C
*/
/**************************************************************************/
uint8_t tcs34725Read8(uint8_t reg)
{
	uint8_t value;
	
	platform_i2c_send_start(TCS34725_BUS_ID);
	platform_i2c_send_address(TCS34725_BUS_ID, TCS34725_ADDRESS, PLATFORM_I2C_DIRECTION_TRANSMITTER);
	platform_i2c_send_byte(TCS34725_BUS_ID, TCS34725_COMMAND_BIT | reg);
	platform_i2c_send_stop(TCS34725_BUS_ID);
	
	platform_i2c_send_start(TCS34725_BUS_ID);
	platform_i2c_send_address(TCS34725_BUS_ID, TCS34725_ADDRESS, PLATFORM_I2C_DIRECTION_RECEIVER);
	value = platform_i2c_recv_byte(TCS34725_BUS_ID, 0);
	platform_i2c_send_stop(TCS34725_BUS_ID);
	
	return value;
}

/**************************************************************************/
/*!
		@brief	Reads a 16 bit values over I2C
*/
/**************************************************************************/
uint16_t tcs34725Read16(uint8_t reg)
{		
	uint8_t low = tcs34725Read8(reg);
	uint8_t high = tcs34725Read8(++reg);
	
	return (high << 8) | low;
}
/**************************************************************************/
/*!
		@brief	Finishes enabling the device
*/
/**************************************************************************/
uint8_t tcs34725EnableDone()
{
	dbg_printf("Enable finished\n");
	lua_State *L = lua_getstate();
	os_timer_disarm (&tcs34725_timer);
	tcs34725Write8(TCS34725_ENABLE, TCS34725_ENABLE_PON | TCS34725_ENABLE_AEN);

	/* Ready to go ... set the initialised flag */
	_tcs34725Initialised = true;
	
	/* This needs to take place after the initialisation flag! */
	tcs34725SetIntegrationTime(TCS34725_INTEGRATIONTIME_2_4MS, L);
	tcs34725SetGain(TCS34725_GAIN_60X, L);	
	
	lua_rawgeti(L, LUA_REGISTRYINDEX, cb_tcs_en); // Get the callback to call
	luaL_unref(L, LUA_REGISTRYINDEX, cb_tcs_en); // Unregister the callback to avoid leak
	cb_tcs_en = LUA_NOREF;
	lua_call(L, 0, 0);
	
	return 0;
}

/**************************************************************************/
/*!
		@brief	Enables the device
*/
/**************************************************************************/
uint8_t tcs34725Enable(lua_State* L)
{
	dbg_printf("Enable begun\n");

	if (lua_type(L, 1) == LUA_TFUNCTION || lua_type(L, 1) == LUA_TLIGHTFUNCTION) {
		if (cb_tcs_en != LUA_NOREF) {
			luaL_unref(L, LUA_REGISTRYINDEX, cb_tcs_en);
		}
		lua_pushvalue(L, 1);
		cb_tcs_en = luaL_ref(L, LUA_REGISTRYINDEX);
	} else {
		return luaL_error(L, "Enable argument must be a function.");
	}
	
	tcs34725Write8(TCS34725_ENABLE, TCS34725_ENABLE_PON);
	// Start a timer to wait TCS34725_EN_DELAY before calling tcs34725EnableDone
	os_timer_disarm (&tcs34725_timer);
	os_timer_setfn (&tcs34725_timer, (os_timer_func_t *)tcs34725EnableDone, NULL);
	os_timer_arm (&tcs34725_timer, TCS34725_EN_DELAY, 0); // trigger callback when readout is ready

	return 0;
}

/**************************************************************************/
/*!
		@brief	Disables the device (putting it in lower power sleep mode)
*/
/**************************************************************************/
uint8_t tcs34725Disable(lua_State* L)
{
	/* Turn the device off to save power */
	uint8_t reg = 0;
	reg = tcs34725Read8(TCS34725_ENABLE);
	tcs34725Write8(TCS34725_ENABLE, reg & ~(TCS34725_ENABLE_PON | TCS34725_ENABLE_AEN));
	_tcs34725Initialised = false;
	return 0;
}

/**************************************************************************/
/*!
		@brief	Initialises the I2C block
*/
/**************************************************************************/
uint8_t tcs34725Setup(lua_State* L)
{
	uint8_t id = 0;

	/* Make sure we have the right IC (0x44 = TCS34725 and TCS34721) */
	id = tcs34725Read8(TCS34725_ID);
	dbg_printf("id: %x\n",id);
	if (id != 0x44) {
		return luaL_error(L, "No TCS34725 found.");
	}
	
	lua_pushinteger(L, 1);
	return 1;
}

/**************************************************************************/
/*!
		@brief	Sets the integration time to the specified value
*/
/**************************************************************************/
uint8_t tcs34725LuaSetIntegrationTime(lua_State* L)
{
	tcs34725IntegrationTime_t it = luaL_checkinteger(L, 1);
	return tcs34725SetIntegrationTime(it,L);
}

/**************************************************************************/
/*!
		@brief	Sets the integration time to the specified value
*/
/**************************************************************************/
uint8_t tcs34725SetIntegrationTime(tcs34725IntegrationTime_t it, lua_State* L)
{
	if (!_tcs34725Initialised)
	{
		tcs34725Setup(L);
	}

	tcs34725Write8(TCS34725_ATIME, it);
	_tcs34725IntegrationTime = it;

	return 0;
}

/**************************************************************************/
/*!
		@brief	Sets gain to the specified value from Lua
*/
/**************************************************************************/
uint8_t tcs34725LuaSetGain(lua_State* L)
{
	tcs34725Gain_t gain = luaL_checkinteger(L, 1);
	return tcs34725SetGain(gain,L);
}

/**************************************************************************/
/*!
		@brief	Sets gain to the specified value
*/
/**************************************************************************/
uint8_t tcs34725SetGain(tcs34725Gain_t gain, lua_State* L)
{
	if (!_tcs34725Initialised)
	{
		return luaL_error(L, "TCS34725 not initialised.");
	}

	tcs34725Write8(TCS34725_CONTROL, gain);
	_tcs34725Gain = gain;

	return 0;
}

/**************************************************************************/
/*!
		@brief	Reads the raw red, green, blue and clear channel values
*/
/**************************************************************************/
uint8_t tcs34725GetRawData(lua_State* L)
{
	uint16_t r;
	uint16_t g;
	uint16_t b;
	uint16_t c;
	
	if (!_tcs34725Initialised)
	{
		return luaL_error(L, "TCS34725 not initialised.");
	}

	c = tcs34725Read16(TCS34725_CDATAL);
	r = tcs34725Read16(TCS34725_RDATAL);
	g = tcs34725Read16(TCS34725_GDATAL);
	b = tcs34725Read16(TCS34725_BDATAL);
	lua_pushinteger(L, c);
	lua_pushinteger(L, r);
	lua_pushinteger(L, g);
	lua_pushinteger(L, b);
	return 4;
}


static const LUA_REG_TYPE tcs34725_map[] = {
	{ LSTRKEY( "setup" ), LFUNCVAL(tcs34725Setup)},
	{ LSTRKEY( "enable" ),  LFUNCVAL(tcs34725Enable)},
	{ LSTRKEY( "disable" ),  LFUNCVAL(tcs34725Disable)},
	{ LSTRKEY( "raw" ),  LFUNCVAL(tcs34725GetRawData)},
	{ LSTRKEY( "setGain" ),  LFUNCVAL(tcs34725LuaSetGain)},
	{ LSTRKEY( "setIntegrationTime" ),  LFUNCVAL(tcs34725LuaSetIntegrationTime)},
	{ LNILKEY, LNILVAL}
};

NODEMCU_MODULE(TCS34725, "tcs34725", tcs34725_map, NULL);