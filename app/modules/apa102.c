#include "lualib.h"
#include "lauxlib.h"
#include "platform.h"
#include "auxmods.h"
#include "lrotable.h"
#include "c_stdlib.h"
#include "c_string.h"

static void ICACHE_FLASH_ATTR apa102_write_str(uint8_t id, const char *buffer, size_t length, int rearrange_grb) {
	// Initialize spi:
	platform_spi_setup(id, PLATFORM_SPI_MASTER, PLATFORM_SPI_CPOL_LOW, PLATFORM_SPI_CPHA_LOW, 0);

	// Ignore incomplete Byte triples at the end of buffer:
	length -= length % 3;

	size_t i;
	ets_intr_lock();
	// Send startframe
	platform_spi_send_recv(id, 8, 0x00);
	platform_spi_send_recv(id, 8, 0x00);
	platform_spi_send_recv(id, 8, 0x00);
	platform_spi_send_recv(id, 8, 0x00);
	// Send the buffer
	for (i = 0; i < length; i+=3) {
		platform_spi_send_recv(id, 8, 0xff);
		if (rearrange_grb) {
			platform_spi_send_recv(id, 8, buffer[i+2]);
			platform_spi_send_recv(id, 8, buffer[i]);
			platform_spi_send_recv(id, 8, buffer[i+1]);
		} else {
			platform_spi_send_recv(id, 8, buffer[i+2]);
			platform_spi_send_recv(id, 8, buffer[i+1]);
			platform_spi_send_recv(id, 8, buffer[i]);
		}
	}
	// Send endframe
	for (i = 0; i < length; i+=16) {
		platform_spi_send_recv(id, 8, 0xff);
	}
	ets_intr_unlock();
}

// Lua: apa102.writergb(pin, "string")
// Byte triples in the string are interpreted as R G B values and sent to the hardware as G R B.

// apa102.writergb(4, string.char(255, 0, 0)) uses GPIO2 and sets the first LED red.
// apa102.writergb(3, string.char(0, 0, 255):rep(10)) uses GPIO0 and sets ten LEDs blue.
// apa102.writergb(4, string.char(0, 255, 0, 255, 255, 255)) first LED green, second LED white.
static int ICACHE_FLASH_ATTR apa102_writergb(lua_State* L) {
	const uint8_t id = luaL_checkinteger(L, 1);
	size_t length;
	const char *rgb = luaL_checklstring(L, 2, &length);
	// dont modify lua-internal lstring - make a copy instead
	char *buffer = (char *)c_malloc(length);
	c_memcpy(buffer, rgb, length);
	apa102_write_str(id, buffer, length, false);
	c_free(buffer);

	return 0;
}

// Lua: apa102.write(pin, "string")
// Byte triples in the string are interpreted as G R B values.
// This function does not corrupt your buffer.
//
// apa102.write(4, string.char(0, 255, 0)) uses GPIO2 and sets the first LED red.
// apa102.write(3, string.char(0, 0, 255):rep(10)) uses GPIO0 and sets ten LEDs blue.
// apa102.write(4, string.char(255, 0, 0, 255, 255, 255)) first LED green, second LED white.
static int ICACHE_FLASH_ATTR apa102_writegrb(lua_State* L) {
	const uint8_t id = luaL_checkinteger(L, 1);
	size_t length;
	const char *rgb = luaL_checklstring(L, 2, &length);
	// dont modify lua-internal lstring - make a copy instead
	char *buffer = (char *)c_malloc(length);
	c_memcpy(buffer, rgb, length);
	apa102_write_str(id, buffer, length, true);
	c_free(buffer);

	return 0;
}


#define MIN_OPT_LEVEL 2
#include "lrodefs.h"
const LUA_REG_TYPE apa102_map[] =
{
	{ LSTRKEY( "writergb" ), LFUNCVAL( apa102_writergb )},
	{ LSTRKEY( "write" ), LFUNCVAL( apa102_writegrb )},
	{ LNILKEY, LNILVAL}
};

LUALIB_API int luaopen_apa102(lua_State *L) {
	LREGISTER(L, "apa102", apa102_map);
	return 1;
}
