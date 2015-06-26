// Module for DHT11/21/22 temp/humidity modules

#include "platform.h"
#include "auxmods.h"
#include "osapi.h"
#include "gpio.h"

typedef enum {
	DHT11 = 11,
	DHT21 = 21,
	DHT22 = 22,
} DHT_TYPE;

static short int temp = -10000, hum = -10000;

#define DHT_MAXTIMINGS	10000
#define DHT_BREAKTIME	20
#define DHT_MAXCOUNT	32000
// #define DHT_DEBUG		true

#ifdef DHT_DEBUG
#undef DHT_DEBUG
#define DHT_DEBUG(...) os_printf(__VA_ARGS__);
#else
#define DHT_DEBUG(...)
#endif

#define sleepms(x) os_delay_us(x*1000);

static bool DHTRead(int pin, DHT_TYPE type)
{
	int counter = 0;
	int laststate = 1;
	int i = 0;
	int j = 0;
	int checksum = 0;
	unsigned char data[100];
	data[0] = data[1] = data[2] = data[3] = data[4] = 0;
	uint8_t platform_pin = GPIO_ID_PIN(pin_num[pin]);

	platform_gpio_mode(pin, PLATFORM_GPIO_INPUT, PLATFORM_GPIO_PULLUP);

	// Wake up device, 250ms of high
	GPIO_OUTPUT_SET(platform_pin, 1);
	sleepms(250);
	// Hold low for 20ms
	GPIO_OUTPUT_SET(platform_pin, 0);
	sleepms(20);
	// High for 40ns
	GPIO_OUTPUT_SET(platform_pin, 1);
	os_delay_us(40);
	// Set DHT_PIN pin as an input
	GPIO_DIS_OUTPUT(platform_pin);

	// wait for pin to drop?
	while (GPIO_INPUT_GET(platform_pin) == 1 && i < DHT_MAXCOUNT) {
		os_delay_us(1);
		i++;
	}

	if(i == DHT_MAXCOUNT)
	{
		DHT_DEBUG("DHT: Failed to get reading from GPIO%d, dying\r\n", pin);
		temp = hum = -10000;
	    return false;
	}

	// read data
	for (i = 0; i < DHT_MAXTIMINGS; i++)
	{
		// Count high time (in approx us)
		counter = 0;
		while (GPIO_INPUT_GET(platform_pin) == laststate)
		{
			counter++;
			os_delay_us(1);
			if (counter == 1000)
				break;
		}
		laststate = GPIO_INPUT_GET(platform_pin);
		if (counter == 1000 || j >= sizeof(data) * 8)
			break;
		// store data after 3 reads
		if ((i>3) && (i%2 == 0)) {
			// shove each bit into the storage bytes
			data[j/8] <<= 1;
			if (counter > DHT_BREAKTIME)
				data[j/8] |= 1;
			j++;
		}
	}

	if (j >= 39) {
		checksum = (data[0] + data[1] + data[2] + data[3]) & 0xFF;
	    DHT_DEBUG("DHT: %02x %02x %02x %02x [%02x] CS: %02x (GPIO%d)\r\n",
	              data[0], data[1], data[2], data[3], data[4], checksum, pin);
		if (data[4] == checksum) {
			// checksum is valid
			switch (type) {
			case DHT11:
				hum = data[0];
				temp = data[2];
				break;
			case DHT21:
			case DHT22:
				hum = (data[0] << 8) + data[1];
				temp = (data[2] << 8) + data[3];
				break;
			}
			DHT_DEBUG("DHT: Temperature =  %d *C, Humidity = %d %% (GPIO%d)\n",
		          (int) (temp), (int) (hum), pin);
		}
		else {
			DHT_DEBUG("DHT: Checksum was incorrect after %d bits. Expected %d but got %d (GPIO%d)\r\n",
		                j, data[4], checksum, pin);
			temp = hum = -10000;
		    return false;
		}
	}
	else {
	    DHT_DEBUG("DHT: Got too few bits: %d should be at least 40 (GPIO%d)\r\n", j, pin);
		temp = hum = -10000;
	    return false;
	}
	return true;
}

// Lua: dht.read11(gpio_pin)
static int dht_read11(lua_State *L)
{
  unsigned pin = luaL_checkinteger(L, 1);
  if (DHTRead(pin, DHT11)) {
    lua_pushinteger(L, temp);
    lua_pushinteger(L, hum);
    return 2;
  }
  lua_pushnil(L);
  return 1;
}

// Lua: dht.read22(gpio_pin)
static int dht_read22(lua_State *L)
{
  unsigned pin = luaL_checkinteger(L, 1);
  if (DHTRead(pin, DHT22)) {
    lua_pushinteger(L, temp);
    lua_pushinteger(L, hum);
    return 2;
  }
  lua_pushnil(L);
  return 1;
}

static int dht_getTemperature(lua_State *L)
{
  if (temp == -10000)
    lua_pushnil(L);
  else
    lua_pushinteger(L, temp);
  return 1;
}

static int dht_getHumidity(lua_State *L)
{
  if (hum == -10000)
    lua_pushnil(L);
  else
    lua_pushinteger(L, hum);
  return 1;
}

// Module function map
#define MIN_OPT_LEVEL 2
#include "lrodefs.h"
const LUA_REG_TYPE dht_map[] =
{
  { LSTRKEY("read11"), LFUNCVAL(dht_read11) },
  { LSTRKEY("read22"), LFUNCVAL(dht_read22) },
  { LSTRKEY("getTemperature"), LFUNCVAL(dht_getTemperature) },
  { LSTRKEY("getHumidity"), LFUNCVAL(dht_getHumidity) },

#if LUA_OPTIMIZE_MEMORY > 0

#endif
  { LNILKEY, LNILVAL }
};

LUALIB_API int luaopen_dht(lua_State *L)
{
#if LUA_OPTIMIZE_MEMORY > 0
  return 0;
#else // #if LUA_OPTIMIZE_MEMORY > 0
  luaL_register(L, AUXLIB_DHT, dht_map);
  // Add constants

  return 1;
#endif // #if LUA_OPTIMIZE_MEMORY > 0
}
