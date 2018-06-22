//***************************************************************************
// DS18B20 module for ESP8266 with nodeMCU
// fetchbot @github
// MIT license, http://opensource.org/licenses/MIT
//***************************************************************************

#include "module.h"
#include "lauxlib.h"
#include "platform.h"
#include "osapi.h"
#include "driver/onewire.h"
#include "c_stdio.h"
#include "c_stdlib.h"

//***************************************************************************
// OW ROM COMMANDS
//***************************************************************************

#define DS18B20_ROM_SEARCH				(0xF0)
#define DS18B20_ROM_READ				(0x33)
#define DS18B20_ROM_MATCH				(0x55)
#define DS18B20_ROM_SKIP				(0xCC)
#define DS18B20_ROM_SEARCH_ALARM		(0xEC)

//***************************************************************************
// OW FUNCTION COMMANDS
//***************************************************************************

#define DS18B20_FUNC_CONVERT			(0x44)
#define DS18B20_FUNC_SCRATCH_WRITE		(0x4E)
#define DS18B20_FUNC_SCRATCH_READ		(0xBE)
#define DS18B20_FUNC_SCRATCH_COPY		(0x48)
#define DS18B20_FUNC_E2_RECALL			(0xB8)
#define DS18B20_FUNC_POWER_READ			(0xB4)

//***************************************************************************
// Initial EEPROM values
//***************************************************************************

#define DS18B20_EEPROM_TH				(0x4B)		// 75 degree
#define DS18B20_EEPROM_TL				(0x46)		// 70 degree
#define DS18B20_EEPROM_RES				(0x7F)		// 12 bit resolution

//***************************************************************************

static uint8_t ds18b20_bus_pin;
static uint8_t ds18b20_device_family;
static uint8_t ds18b20_device_search = 0;
static uint8_t ds18b20_device_index;
static uint8_t ds18b20_device_par;
static uint8_t ds18b20_device_conf[3];
static uint8_t ds18b20_device_rom[8];
static uint8_t ds18b20_device_scratchpad[9];
static double ds18b20_device_scratchpad_temp;
static int ds18b20_device_scratchpad_temp_dec;
static uint8_t ds18b20_device_scratchpad_conf;
static uint8_t ds18b20_device_res = 12;			// 12 bit resolution (750ms conversion time)

os_timer_t ds18b20_timer;						// timer for conversion delay
int ds18b20_timer_ref;							// callback when readout is ready

int ds18b20_table_ref;
static int ds18b20_table_offset;

static int ds18b20_lua_readoutdone(void);

// Setup onewire bus for DS18B20 temperature sensors
// Lua: ds18b20.setup(OW_BUS_PIN)
static int ds18b20_lua_setup(lua_State *L) {
	// check ow bus pin value
	if (!lua_isnumber(L, 1) || lua_isnumber(L, 1) == 0) {
		return luaL_error(L, "wrong 1-wire pin");
	}
	
	ds18b20_bus_pin = luaL_checkinteger(L, 1);
	MOD_CHECK_ID(ow, ds18b20_bus_pin);
	onewire_init(ds18b20_bus_pin);
}

static int ds18b20_set_device(uint8_t *ds18b20_device_rom) {
	onewire_reset(ds18b20_bus_pin);
	onewire_select(ds18b20_bus_pin, ds18b20_device_rom);
	onewire_write(ds18b20_bus_pin, DS18B20_FUNC_SCRATCH_WRITE, 0);
	onewire_write_bytes(ds18b20_bus_pin, ds18b20_device_conf, 3, 0);
}

// Change sensor settings
// Lua: ds18b20.setting(ROM, RES)
static int ds18b20_lua_setting(lua_State *L) {
	// check rom table and resolution setting
	if (!lua_istable(L, 1) || !lua_isnumber(L, 2)) {
		return luaL_error(L, "wrong arg range");
	}
	
	ds18b20_device_res = luaL_checkinteger(L, 2);
	
	if (!((ds18b20_device_res == 9) || (ds18b20_device_res == 10) || (ds18b20_device_res == 11) || (ds18b20_device_res == 12))) {
		return luaL_error(L, "Invalid argument: resolution");
	}
	
	// no change to th and tl setting
	ds18b20_device_conf[0] = DS18B20_EEPROM_TH;
	ds18b20_device_conf[1] = DS18B20_EEPROM_TL;
	ds18b20_device_conf[2] = ((ds18b20_device_res - 9) << 5) + 0x1F;
	
	uint8_t table_len = lua_objlen(L, 1);
	
	const char *str[table_len];
	const char *sep = ":";
	
	uint8_t string_index = 0;
	
	lua_pushnil(L);
	while (lua_next(L, -3)) {
		str[string_index] = lua_tostring(L, -1);
		lua_pop(L, 1);
		string_index++;
	}
	lua_pop(L, 1);
	
	for (uint8_t i = 0; i < string_index; i++) {
		for (uint8_t j = 0; j < 8; j++) {
			ds18b20_device_rom[j] = strtoul(str[i], NULL, 16);
			str[i] = strchr(str[i], *sep);
			if (str[i] == NULL || *str[i] == '\0') break;
			str[i]++;
		}
		ds18b20_set_device(ds18b20_device_rom);
	}
	
	// set conversion delay once to max if sensors with higher resolution still on the bus
	ds18b20_device_res = 12;
	
	return 0;
}

#include "pm/swtimer.h"

// Reads sensor values from all devices
// Lua: 	ds18b20.read(function(INDEX, ROM, RES, TEMP, TEMP_DEC, PAR) print(INDEX, ROM, RES, TEMP, TEMP_DEC, PAR) end, ROM[, FAMILY])
static int ds18b20_lua_read(lua_State *L) {
	
	luaL_argcheck(L, (lua_type(L, 1) == LUA_TFUNCTION || lua_type(L, 1) == LUA_TLIGHTFUNCTION), 1, "Must be function");
	
	lua_pushvalue(L, 1);
	ds18b20_timer_ref = luaL_ref(L, LUA_REGISTRYINDEX);
	
	if (!lua_istable(L, 2)) {
		return luaL_error(L, "wrong arg range");
	}
	
	if (lua_isnumber(L, 3)) {
		ds18b20_device_family = luaL_checkinteger(L, 3);
		onewire_target_search(ds18b20_bus_pin, ds18b20_device_family);
		ds18b20_table_offset = -3;
	} else {
		ds18b20_table_offset = -2;
	}
	
	lua_pushvalue(L, 2);
	ds18b20_table_ref = luaL_ref(L, LUA_REGISTRYINDEX);
	
	lua_pushnil(L);
	if (lua_next(L, ds18b20_table_offset)) {
		lua_pop(L, 2);
		ds18b20_device_search = 0;
	} else {
		ds18b20_device_search = 1;
	}
	
	os_timer_disarm(&ds18b20_timer);
	
	// perform a temperature conversion for all sensors and set timer
	onewire_reset(ds18b20_bus_pin);
	onewire_write(ds18b20_bus_pin, DS18B20_ROM_SKIP, 0);
	onewire_write(ds18b20_bus_pin, DS18B20_FUNC_CONVERT, 1);
	os_timer_setfn(&ds18b20_timer, (os_timer_func_t *)ds18b20_lua_readoutdone, NULL);
	SWTIMER_REG_CB(ds18b20_lua_readoutdone, SWTIMER_DROP);
		//The function ds18b20_lua_readoutdone reads the temperature from the sensor(s) after a set amount of time depending on temperature resolution
	  //MY guess: If this timer manages to get suspended before it fires and the temperature data is time sensitive then resulting data would be invalid and should be discarded
	
	switch (ds18b20_device_res) {
		case (9):
			os_timer_arm(&ds18b20_timer, 95, 0);
			break;
		case (10):
			os_timer_arm(&ds18b20_timer, 190, 0);
			break;
		case (11):
			os_timer_arm(&ds18b20_timer, 380, 0);
			break;
		case (12):
			os_timer_arm(&ds18b20_timer, 760, 0);
			break;
	}
}

static int ds18b20_read_device(uint8_t *ds18b20_device_rom) {
	lua_State *L = lua_getstate();
	int16_t ds18b20_raw_temp;

	if (onewire_crc8(ds18b20_device_rom,7) == ds18b20_device_rom[7]) {
		
		onewire_reset(ds18b20_bus_pin);
		onewire_select(ds18b20_bus_pin, ds18b20_device_rom);
		onewire_write(ds18b20_bus_pin, DS18B20_FUNC_POWER_READ, 0);
		
		if (onewire_read(ds18b20_bus_pin)) ds18b20_device_par = 0;
		else ds18b20_device_par = 1;
			
		onewire_reset(ds18b20_bus_pin);
		onewire_select(ds18b20_bus_pin, ds18b20_device_rom);
		onewire_write(ds18b20_bus_pin, DS18B20_FUNC_SCRATCH_READ, 0);
		onewire_read_bytes(ds18b20_bus_pin, ds18b20_device_scratchpad, 9);
		
		if (onewire_crc8(ds18b20_device_scratchpad,8) == ds18b20_device_scratchpad[8]) {
			
			lua_rawgeti(L, LUA_REGISTRYINDEX, ds18b20_timer_ref);
			
			lua_pushinteger(L, ds18b20_device_index);
			
			lua_pushfstring(L, "%d:%d:%d:%d:%d:%d:%d:%d", ds18b20_device_rom[0], ds18b20_device_rom[1], ds18b20_device_rom[2], ds18b20_device_rom[3], ds18b20_device_rom[4], ds18b20_device_rom[5], ds18b20_device_rom[6], ds18b20_device_rom[7]);
			
			ds18b20_device_scratchpad_conf = (ds18b20_device_scratchpad[4] >> 5) + 9;
			ds18b20_raw_temp = ((ds18b20_device_scratchpad[1] << 8) | ds18b20_device_scratchpad[0]);
			ds18b20_device_scratchpad_temp = (double)ds18b20_raw_temp / 16;
			ds18b20_device_scratchpad_temp_dec = (ds18b20_raw_temp - (ds18b20_raw_temp / 16 * 16)) * 1000 / 16;
			
			if (ds18b20_device_scratchpad_conf >= ds18b20_device_res) {
				ds18b20_device_res = ds18b20_device_scratchpad_conf;
			}
			
			lua_pushinteger(L, ds18b20_device_scratchpad_conf);
			lua_pushnumber(L, ds18b20_device_scratchpad_temp);
			lua_pushinteger(L, ds18b20_device_scratchpad_temp_dec);
			
			lua_pushinteger(L, ds18b20_device_par);
			
			lua_pcall(L, 6, 0, 0);
			
			ds18b20_device_index++;
		}
	}
}

static int ds18b20_lua_readoutdone(void) {
	
	lua_State *L = lua_getstate();
	os_timer_disarm(&ds18b20_timer);
	
	ds18b20_device_index = 1;
	// set conversion delay to min and change it after finding the sensor with the highest resolution setting
	ds18b20_device_res = 9;
	
	if (ds18b20_device_search) {
		// iterate through all sensors on the bus and read temperature, resolution and parasitc settings
		while (onewire_search(ds18b20_bus_pin, ds18b20_device_rom)) {
			ds18b20_read_device(ds18b20_device_rom);
		}
	} else {
		lua_rawgeti(L, LUA_REGISTRYINDEX, ds18b20_table_ref);
		uint8_t table_len = lua_objlen(L, -1);
		
		const char *str[table_len];
		const char *sep = ":";
		
		uint8_t string_index = 0;
		
		lua_pushnil(L);
		while (lua_next(L, -2)) {
			str[string_index] = lua_tostring(L, -1);
			lua_pop(L, 1);
			string_index++;
		}
		lua_pop(L, 1);
		
		for (uint8_t i = 0; i < string_index; i++) {
			for (uint8_t j = 0; j < 8; j++) {
				ds18b20_device_rom[j] = strtoul(str[i], NULL, 16);
				str[i] = strchr(str[i], *sep);
				if (str[i] == NULL || *str[i] == '\0') break;
				str[i]++;
			}
			ds18b20_read_device(ds18b20_device_rom);
		}
	}
	
	luaL_unref(L, LUA_REGISTRYINDEX, ds18b20_table_ref);
	ds18b20_table_ref = LUA_NOREF;
	
	luaL_unref(L, LUA_REGISTRYINDEX, ds18b20_timer_ref);
	ds18b20_timer_ref = LUA_NOREF;
}

static const LUA_REG_TYPE ds18b20_map[] = {
	{	LSTRKEY( "read" ),				LFUNCVAL(ds18b20_lua_read)		},
	{	LSTRKEY( "setting" ),			LFUNCVAL(ds18b20_lua_setting)	},
	{	LSTRKEY( "setup" ),				LFUNCVAL(ds18b20_lua_setup)		},
	{	LNILKEY, LNILVAL												}
};

NODEMCU_MODULE(DS18B20, "ds18b20", ds18b20_map, NULL);
