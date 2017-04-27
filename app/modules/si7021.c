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

#define SI7021_I2C_ADDRESS				(0x40)

//***************************************************************************
// COMMAND DEFINITON
//***************************************************************************

#define SI7021_CMD_MEASURE_RH_HOLD		(0xE5)
#define SI7021_CMD_MEASURE_RH_NOHOLD	(0xF5)
#define SI7021_CMD_MEASURE_TEMP_HOLD	(0xE3)
#define SI7021_CMD_MEASURE_TEMP_NOHOLD	(0xF3)
#define SI7021_CMD_READ_PREV_TEMP		(0xE0)
#define SI7021_CMD_RESET				(0xFE)
#define SI7021_CMD_WRITE_RHT_REG		(0xE6)
#define SI7021_CMD_READ_RHT_REG			(0xE7)
#define SI7021_CMD_WRITE_HEATER_REG		(0x51)
#define SI7021_CMD_READ_HEATER_REG		(0x11)
#define SI7021_CMD_ID1					(0xFA0F)
#define SI7021_CMD_ID2					(0xFCC9)
#define SI7021_CMD_FIRM_REV				(0x84B8)

//***************************************************************************
// REGISTER DEFINITON
//***************************************************************************

#define SI7021_RH12_TEMP14				(0x00)
#define SI7021_RH08_TEMP12				(0x01)
#define SI7021_RH10_TEMP13				(0x80)
#define SI7021_RH11_TEMP11				(0x81)
#define SI7021_HEATER_ENABLE			(0x04)
#define SI7021_HEATER_DISABLE			(0x00)

//***************************************************************************

static const uint32_t si7021_i2c_id = 0;
static uint8_t si7021_i2c_addr = SI7021_I2C_ADDRESS;
static uint8_t si7021_res = 0x00;
static uint8_t si7021_config = 0x3A;
static uint8_t si7021_heater = 0x00;
static uint8_t si7021_heater_setting = 0x00;

static uint8_t write_byte(uint8_t reg) {
	platform_i2c_send_start(si7021_i2c_id);
	platform_i2c_send_address(si7021_i2c_id, si7021_i2c_addr, PLATFORM_I2C_DIRECTION_TRANSMITTER);
	platform_i2c_send_byte(si7021_i2c_id, reg);
	platform_i2c_send_stop(si7021_i2c_id);
}

static uint8_t write_reg(uint8_t reg, uint8_t data) {
	platform_i2c_send_start(si7021_i2c_id);
	platform_i2c_send_address(si7021_i2c_id, si7021_i2c_addr, PLATFORM_I2C_DIRECTION_TRANSMITTER);
	platform_i2c_send_byte(si7021_i2c_id, reg);
	platform_i2c_send_byte(si7021_i2c_id, data);
	platform_i2c_send_stop(si7021_i2c_id);
}

static uint8_t read_reg(uint8_t reg, uint8_t *buf, uint8_t size) {
	platform_i2c_send_start(si7021_i2c_id);
	platform_i2c_send_address(si7021_i2c_id, si7021_i2c_addr, PLATFORM_I2C_DIRECTION_TRANSMITTER);
	platform_i2c_send_byte(si7021_i2c_id, reg);
	platform_i2c_send_stop(si7021_i2c_id);
	platform_i2c_send_start(si7021_i2c_id);
	platform_i2c_send_address(si7021_i2c_id, si7021_i2c_addr, PLATFORM_I2C_DIRECTION_RECEIVER);
	os_delay_us(25000);
	while (size-- > 0)
		*buf++ = platform_i2c_recv_byte(si7021_i2c_id, size > 0);
	platform_i2c_send_stop(si7021_i2c_id);
	return 1;
}

static uint8_t read_serial(uint16_t reg, uint8_t *buf, uint8_t size) {
	platform_i2c_send_start(si7021_i2c_id);
	platform_i2c_send_address(si7021_i2c_id, si7021_i2c_addr, PLATFORM_I2C_DIRECTION_TRANSMITTER);
	platform_i2c_send_byte(si7021_i2c_id, (uint8_t)(reg >> 8));
	platform_i2c_send_byte(si7021_i2c_id, (uint8_t)(reg & 0xFF));
	// platform_i2c_send_stop(si7021_i2c_id);
	platform_i2c_send_start(si7021_i2c_id);
	platform_i2c_send_address(si7021_i2c_id, si7021_i2c_addr, PLATFORM_I2C_DIRECTION_RECEIVER);
	while (size-- > 0)
		*buf++ = platform_i2c_recv_byte(si7021_i2c_id, size > 0);
	platform_i2c_send_stop(si7021_i2c_id);
	return 1;
}

// CRC8
uint8_t si7021_crc8(uint8_t crc, uint8_t *buf, uint8_t size) {
	while (size--) {
		crc ^= *buf++;
		for (uint8_t i = 0; i < 8; i++) {
			if (crc & 0x80) {
				crc = (crc << 1) ^ 0x31;
			} else crc <<= 1;
		}
	}
	return crc;
}

static int si7021_lua_setup(lua_State* L) {

	write_byte(SI7021_CMD_RESET);
	os_delay_us(50000);

	// check for device on i2c bus
	uint8_t buf_r[1];
	read_reg(SI7021_CMD_READ_RHT_REG, buf_r, 1);
	if (buf_r[0] != 0x3A)
		return luaL_error(L, "found no device");

	return 0;
}

// Change sensor settings and returns them
// Lua: 	res, vdds, heater[, heater_set] = si7021.settings(RESOLUTION[,HEATER,HEATER_SETTING])
static int si7021_lua_setting(lua_State* L) {

	// check variable
	if (!lua_isnumber(L, 1)) {
		return luaL_error(L, "wrong arg range");
	}

	si7021_res = luaL_checkinteger(L, 1);
	if (!((si7021_res == SI7021_RH12_TEMP14) || (si7021_res == SI7021_RH08_TEMP12) || (si7021_res == SI7021_RH10_TEMP13) || (si7021_res == SI7021_RH11_TEMP11))) {
		return luaL_error(L, "Invalid argument: resolution");
	}

	si7021_config = (si7021_res | 0x3A);
	write_reg(SI7021_CMD_WRITE_RHT_REG,si7021_config);

	// Parse optional parameters
	if (lua_isnumber(L, 2)) {

		if (!lua_isnumber(L, 2) || !lua_isnumber(L, 3)) {
			return luaL_error(L, "wrong arg range");
		}

		si7021_heater = luaL_checkinteger(L, 2);
		if (!((si7021_heater == SI7021_HEATER_ENABLE) || (si7021_heater == SI7021_HEATER_DISABLE))) {
			return luaL_error(L, "Invalid argument: heater");
		}

		si7021_heater_setting = luaL_checkinteger(L, 3);
		if ((si7021_heater_setting < 0x00) || (si7021_heater_setting > 0x0F)) {
			return luaL_error(L, "Invalid argument: heater_setting");
		}

		si7021_config = (si7021_res | si7021_heater | 0x3A);
		write_reg(SI7021_CMD_WRITE_RHT_REG,si7021_config);
		write_reg(SI7021_CMD_WRITE_HEATER_REG,(si7021_heater_setting & 0x0F));
	}

	uint8_t buf_c[1];
	uint8_t buf_h[1];
	read_reg(SI7021_CMD_READ_RHT_REG, buf_c, 1);
	read_reg(SI7021_CMD_READ_HEATER_REG, buf_h, 1);

	lua_pushinteger(L, ((buf_c[0] >> 6) + (buf_c[0] & 0x01)));
	lua_pushinteger(L, ((buf_c[0] >> 6) & 0x01));
	lua_pushinteger(L, ((buf_c[0] >> 2) & 0x01));
	lua_pushinteger(L, (buf_h[0] & 0x0F));

	return 4;
}

// Reads sensor values from device and returns them
// Lua: 	hum, temp, humdec, tempdec = si7021.read()
static int si7021_lua_read(lua_State* L) {

	uint8_t buf_h[3];	// first two byte data, third byte crc
	read_reg(SI7021_CMD_MEASURE_RH_HOLD, buf_h, 3);
	if (buf_h[2] != si7021_crc8(0, buf_h, 2))	//crc check
		return luaL_error(L, "crc error");
	double hum = (uint16_t)((buf_h[0] << 8) | buf_h[1]);
	hum = ((hum * 125) / 65536 - 6);
	int humdec = (int)((hum - (int)hum) * 1000);

	uint8_t buf_t[2];	// two byte data, no crc on combined temp measurement
	read_reg(SI7021_CMD_READ_PREV_TEMP, buf_t, 2);
	double temp = (uint16_t)((buf_t[0] << 8) | buf_t[1]);
	temp = ((temp * 175.72) / 65536 - 46.85);
	int tempdec = (int)((temp - (int)temp) * 1000);

	lua_pushnumber(L, hum);
	lua_pushnumber(L, temp);
	lua_pushinteger(L, humdec);
	lua_pushinteger(L, tempdec);

	return 4;
}

// Reads electronic serial number from device and returns them
// Lua: 	serial = si7021.serial()
static int si7021_lua_serial(lua_State* L) {

	uint32_t serial_a;
	uint8_t crc = 0;
	uint8_t buf_s_1[8];	// every second byte contains crc
	read_serial(SI7021_CMD_ID1, buf_s_1, 8);
	for(uint8_t i = 0; i <= 6; i+=2) {
		crc = si7021_crc8(crc, buf_s_1+i, 1);
		if (buf_s_1[i+1] != crc)
			return luaL_error(L, "crc error");
		serial_a = (serial_a << 8) + buf_s_1[i];
	}

	uint32_t serial_b;
	crc = 0;
	uint8_t buf_s_2[6]; // every third byte contains crc
	read_serial(SI7021_CMD_ID2, buf_s_2, 6);
	for(uint8_t i = 0; i <=3; i+=3) {
		crc = si7021_crc8(crc, buf_s_2+i, 2);
		if (buf_s_2[i+2] != crc)
			return luaL_error(L, "crc error");
		serial_b = (serial_b << 16) + (buf_s_2[i] << 8) + buf_s_2[i+1];
	}

	lua_pushinteger(L, serial_a);
	lua_pushinteger(L, serial_b);

	return 2;
}

// Reads electronic firmware revision from device and returns them
// Lua: 	firmware = si7021.firmware()
static int si7021_lua_firmware(lua_State* L) {

	uint8_t firmware;
	uint8_t buf_f[1];
	read_serial(SI7021_CMD_FIRM_REV, buf_f, 1);
	firmware = buf_f[0];

	lua_pushinteger(L, firmware);

	return 1;
}

static const LUA_REG_TYPE si7021_map[] = {
	{	LSTRKEY( "setup" ),				LFUNCVAL(si7021_lua_setup)		},
	{	LSTRKEY( "setting" ),			LFUNCVAL(si7021_lua_setting)	},
	{	LSTRKEY( "read" ),				LFUNCVAL(si7021_lua_read)		},
	{	LSTRKEY( "serial" ),			LFUNCVAL(si7021_lua_serial)		},
	{	LSTRKEY( "firmware" ),			LFUNCVAL(si7021_lua_firmware)	},
	{	LSTRKEY( "RH12_TEMP14" ),		LNUMVAL(SI7021_RH12_TEMP14)		},
	{	LSTRKEY( "RH08_TEMP12" ),		LNUMVAL(SI7021_RH08_TEMP12)		},
	{	LSTRKEY( "RH10_TEMP13" ),		LNUMVAL(SI7021_RH10_TEMP13)		},
	{	LSTRKEY( "RH11_TEMP11" ),		LNUMVAL(SI7021_RH11_TEMP11)		},
	{	LSTRKEY( "HEATER_ENABLE" ),		LNUMVAL(SI7021_HEATER_ENABLE)	},
	{	LSTRKEY( "HEATER_DISABLE" ),	LNUMVAL(SI7021_HEATER_DISABLE)	},
	{	LNILKEY, LNILVAL												}
};

NODEMCU_MODULE(SI7021, "si7021", si7021_map, NULL);
