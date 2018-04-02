/*
 * app/modules/am2320.c
 *
 * Copyright (c) 2016 Henk Vergonet <Henk.Vergonet@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */
#include "module.h"
#include "lauxlib.h"
#include "platform.h"
#include "lwip/udp.h"
#include <errno.h>

static const uint32_t am2320_i2c_id = 0;
static const uint8_t am2320_i2c_addr = 0xb8 >> 1;

static uint16_t crc16(uint8_t *ptr, unsigned int len)
{
    uint16_t	crc =0xFFFF;
    uint8_t	i;

    while(len--) {
	crc ^= *ptr++;
	for(i=0;i<8;i++) {
	    if(crc & 0x01) {
		crc >>= 1;
		crc ^= 0xA001;
	    } else {
		crc >>= 1;
	    }
	}
    }
    return crc;
}

/* make sure buf has lenght len+2 in order to accommodate for extra bytes */
static int _read(uint32_t id, void *buf, uint8_t len, uint8_t off)
{
    int i;
    uint8_t *b = (uint8_t *)buf;
    uint16_t crc;

    // step 1: Wake sensor
    platform_i2c_send_start(id);
    platform_i2c_send_address(id, am2320_i2c_addr, PLATFORM_I2C_DIRECTION_TRANSMITTER);
    os_delay_us(800);
    platform_i2c_send_stop(id);

    // step 2: Send read command
    platform_i2c_send_start(id);
    platform_i2c_send_address(id, am2320_i2c_addr, PLATFORM_I2C_DIRECTION_TRANSMITTER);
    platform_i2c_send_byte(id, 0x03);
    platform_i2c_send_byte(id, off);
    platform_i2c_send_byte(id, len);
    platform_i2c_send_stop(id);

    // step 3: Read the data
    os_delay_us(1500);
    platform_i2c_send_start(id);
    platform_i2c_send_address(id, am2320_i2c_addr, PLATFORM_I2C_DIRECTION_RECEIVER);
    os_delay_us(30);
    for(i=0; i<len+2; i++)
	b[i] = platform_i2c_recv_byte(id,1);
    crc  = platform_i2c_recv_byte(id,1);
    crc |= platform_i2c_recv_byte(id,0) << 8;
    platform_i2c_send_stop(id);

    if(b[0] != 0x3 || b[1] != len)
	return -EIO;
    if(crc != crc16(b,len+2))
	return -EIO;
    return 0;
}

static int am2320_setup(lua_State* L)
{
    int ret;
    struct {
    	uint8_t  cmd;
    	uint8_t  len;
	uint16_t model;
	uint8_t	 version;
	uint32_t id;
    } nfo;

    os_delay_us(1500); // give some time to settle things down
    ret = _read(am2320_i2c_id, &nfo, sizeof(nfo)-2, 0x08);
    if(ret)
        return luaL_error(L, "transmission error");

    lua_pushinteger(L, ntohs(nfo.model));
    lua_pushinteger(L, nfo.version);
    lua_pushinteger(L, ntohl(nfo.id));
    return 3;
}

static int am2320_read(lua_State* L)
{
    int ret;
    struct {
    	uint8_t  cmd;
    	uint8_t  len;
	uint16_t rh;
	uint16_t temp;
    } nfo;
 
    ret = _read(am2320_i2c_id, &nfo, sizeof(nfo)-2, 0x00);
    if(ret)
        return luaL_error(L, "transmission error");

    ret = ntohs(nfo.temp);
    if(ret & 0x8000)
    	ret = -(ret & 0x7fff);

    lua_pushinteger(L, ntohs(nfo.rh));
    lua_pushinteger(L, ret);
    return 2;
}

static const LUA_REG_TYPE am2320_map[] = {
    { LSTRKEY( "read" ),  LFUNCVAL( am2320_read )},
    { LSTRKEY( "setup" ), LFUNCVAL( am2320_setup )},
    { LNILKEY, LNILVAL}
};

NODEMCU_MODULE(AM2320, "am2320", am2320_map, NULL);
