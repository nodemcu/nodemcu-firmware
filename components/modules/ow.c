// Module for interfacing with the OneWire interface

#include "module.h"
#include "lauxlib.h"
#include <stdint.h>
#include "platform.h"


static int ow_bus_table_ref;

// Lua: ow.setup( pin )
static int ow_setup( lua_State *L )
{
  int pin = luaL_checkint( L, 1 );
  luaL_argcheck( L, pin >= 0, 1, "invalid pin" );

  if (platform_onewire_init( pin ) != PLATFORM_OK)
    luaL_error( L, "failed" );

  return 0;
}

// Lua: r = ow.reset( pin )
static int ow_reset( lua_State *L )
{
  int pin = luaL_checkint( L, 1 );
  luaL_argcheck( L, pin >= 0, 1, "invalid pin" );

  uint8_t presence;
  if (platform_onewire_reset( pin, &presence ) != PLATFORM_OK)
    luaL_error( L, "failed" );

  lua_pushinteger( L, presence );
  return 1;
}

// Lua: ow.write( pin, v, power)
static int ow_write( lua_State *L )
{
  int stack = 0;

  int pin = luaL_checkint( L, ++stack );
  luaL_argcheck( L, pin >= 0, stack, "invalid pin" );

  int data = luaL_checkint( L, ++stack );
  luaL_argcheck( L, data >= 0 && data < 256, stack, "invalid data" );

  int power = luaL_optint( L, ++stack, 0 ) != 0 ? 1 : 0;

  uint8_t d = data;
  if (platform_onewire_write_bytes( pin, &d, 1, power ) != PLATFORM_OK)
    luaL_error( L, "failed" );

  return 0;
}

// Lua: ow.write_bytes( pin, buf, power)
static int ow_write_bytes( lua_State *L )
{
  int stack = 0;
  size_t datalen;

  int pin = luaL_checkint( L, ++stack );
  luaL_argcheck( L, pin >= 0, stack, "invalid pin" );

  const char *pdata = luaL_checklstring( L, ++stack, &datalen );

  int power = luaL_optint( L, ++stack, 0 ) != 0 ? 1 : 0;

  platform_onewire_write_bytes( pin, (uint8_t *)pdata, datalen, power);

  return 0;
}

// Lua: r = ow.read( pin )
static int ow_read( lua_State *L )
{
  int pin = luaL_checkint( L, 1 );
  luaL_argcheck( L, pin >= 0, 1, "invalid pin" );

  uint8_t data;
  if (platform_onewire_read_bytes( pin, &data, 1 ) != PLATFORM_OK)
    luaL_error( L, "failed" );

  lua_pushinteger( L, data );
  return 1;
}

// Lua: r = ow.read_bytes( pin, size )
static int ow_read_bytes( lua_State *L )
{
  int stack = 0;

  int pin = luaL_checkint( L, ++stack );
  luaL_argcheck( L, pin >= 0, stack, "invalid pin" );

  int size = luaL_checkint( L, ++stack );
  if (size == 0)
    return 0;
  luaL_argcheck( L, size <= LUAL_BUFFERSIZE, stack, "Attempt to read too many characters" );

  luaL_Buffer b;
  luaL_buffinit( L, &b );
  char *p = luaL_prepbuffer( &b );

  if (platform_onewire_read_bytes( pin, (uint8_t *)p, size ) != PLATFORM_OK)
    luaL_error( L, "failed" );

  luaL_addsize( &b, size );
  luaL_pushresult( &b );
  return 1;
}

// Lua: ow.select( pin, buf[8] )
static int ow_select( lua_State *L )
{
  int stack = 0;
  uint8_t rom[1+8];
  size_t datalen;
  int numdata, i;
  const char *pdata;

  int pin = luaL_checkint( L, ++stack );
  luaL_argcheck( L, pin >= 0, stack, "invalid pin" );

  if( lua_istable( L, ++stack ) )
  {
    datalen = lua_objlen( L, stack );
    luaL_argcheck( L, datalen == 8, stack, "wrong arg range" );
    for (i = 0; i < datalen; i ++) {
      lua_rawgeti( L, stack, i + 1 );
      numdata = ( int )luaL_checkinteger( L, -1 );
      lua_pop( L, 1 );
      if (numdata > 255)
        return luaL_error( L, "wrong arg range" );
      rom[1+i] = (uint8_t)numdata;
    }
  } else {
    pdata = luaL_checklstring( L, stack, &datalen );
    luaL_argcheck( L, datalen == 8, stack, "wrong arg range" );
    for (i = 0; i < datalen; i++) {
      rom[1+i] = pdata[i];
    }
  }

  rom[0] = 0x55;  // select command
  if (platform_onewire_write_bytes( pin, rom, 1+8, 0 ) != PLATFORM_OK)
    luaL_error( L, "write failed" );

  return 0;
}

// Lua: ow.skip( pin )
static int ow_skip( lua_State *L )
{
  int pin = luaL_checkint( L, 1 );
  luaL_argcheck( L, pin >= 0, 1, "invalid pin" );

  const uint8_t cmd = 0xCC;  // skip command
  if (platform_onewire_write_bytes( pin, &cmd, 1, 0 ) != PLATFORM_OK)
    luaL_error( L, "write failed" );

  return 0;
}

// Lua: ow.depower( pin )
static int ow_depower( lua_State *L )
{
  int pin = luaL_checkint( L, 1 );
  luaL_argcheck( L, pin >= 0, 1, "invalid pin" );

  platform_onewire_depower( pin );

  return 0;
}

// Clear the search state so that if will start from the beginning again.
// Lua: ow.reset_search( pin )
static int ow_reset_search( lua_State *L )
{
  int pin = luaL_checkint( L, 1 );
  luaL_argcheck( L, pin >= 0, 1, "invalid pin" );

  // set up new userdata for this pin
  lua_rawgeti( L, LUA_REGISTRYINDEX, ow_bus_table_ref );
  platform_onewire_bus_t *bus = (platform_onewire_bus_t *)lua_newuserdata( L, sizeof( platform_onewire_bus_t ));
  lua_rawseti( L, -2, pin );
  // remove table from stack
  lua_pop( L, 1 );

  platform_onewire_reset_search( bus );
  return 0;
}

// Setup the search to find the device type 'family_code' on the next call
// to search(*newAddr) if it is present.
// Lua: ow.target_search( pin, family_code )
static int ow_target_search( lua_State *L )
{
  int stack = 0;

  int pin = luaL_checkint( L, ++stack );
  luaL_argcheck( L, pin >= 0, stack, "invalid pin" );

  int code = (int)luaL_checkinteger( L, ++stack );
  luaL_argcheck( L, code >= 0 && code < 256, stack, "wrong arg range" );

  // set up new userdata for this pin
  lua_rawgeti( L, LUA_REGISTRYINDEX, ow_bus_table_ref );
  platform_onewire_bus_t *bus = (platform_onewire_bus_t *)lua_newuserdata( L, sizeof( platform_onewire_bus_t ));
  lua_rawseti( L, -2, pin );
  // remove table from stack
  lua_pop( L, 1 );

  platform_onewire_target_search( (uint8_t)code, bus );

  return 0;
}

// Look for the next device. Returns 1 if a new address has been
// returned. A zero might mean that the bus is shorted, there are
// no devices, or you have already retrieved all of them.  It
// might be a good idea to check the CRC to make sure you didn't
// get garbage.  The order is deterministic. You will always get
// the same devices in the same order.

// Lua: r = ow.search( pin )
static int ow_search( lua_State *L )
{
  int pin = luaL_checkint( L, 1 );
  luaL_argcheck( L, pin >= 0, 1, "invalid pin" );

  // look-up bus userdata for this pin
  lua_rawgeti( L, LUA_REGISTRYINDEX, ow_bus_table_ref );
  lua_rawgeti( L, -1, pin );
  platform_onewire_bus_t *bus = (platform_onewire_bus_t *)lua_touserdata( L, -1 );
  if (!bus)
    return luaL_error( L, "search not initialized" );
  // removed temps from stack
  lua_pop( L, 2 );

  luaL_Buffer b;
  luaL_buffinit( L, &b );
  char *p = luaL_prepbuffer( &b );

  if (platform_onewire_search( pin, (uint8_t *)p, bus )){
    luaL_addsize( &b, 8 );
    luaL_pushresult( &b );
  } else {
    luaL_pushresult( &b );  /* close buffer */
    lua_pop( L, 1 );

    // remove bus userdata from table
    lua_rawgeti( L, LUA_REGISTRYINDEX, ow_bus_table_ref );
    lua_pushnil( L );
    lua_rawseti( L, -2, pin );
    // remove table from stack
    lua_pop( L, 1 );

    lua_pushnil( L );
  }
  return 1; 
}

// uint8_t onewire_crc8(const uint8_t *addr, uint8_t len);
// Lua: r = ow.crc8( buf )
static int ow_crc8( lua_State *L )
{
  size_t datalen;
  const char *pdata = luaL_checklstring( L, 1, &datalen );
  if(datalen > 255)
    return luaL_error( L, "wrong arg range" );
  lua_pushinteger( L, platform_onewire_crc8((uint8_t *)pdata, datalen) );
  return 1;
}

// bool onewire_check_crc16(const uint8_t* input, uint16_t len, const uint8_t* inverted_crc, uint16_t crc);
// Lua: b = ow.check_crc16( buf, inverted_crc0, inverted_crc1, crc )
static int ow_check_crc16( lua_State *L )
{
  size_t datalen;
  uint8_t inverted_crc[2];
  const char *pdata = luaL_checklstring( L, 1, &datalen );
  if(datalen > 65535)
    return luaL_error( L, "wrong arg range" );

  int crc = 0;
  crc = luaL_checkinteger( L, 2 );
  if(datalen > 255)
    return luaL_error( L, "wrong arg range" );
  inverted_crc[0] = (uint8_t)crc;

  crc = luaL_checkinteger( L, 3 );
  if(datalen > 255)
    return luaL_error( L, "wrong arg range" );
  inverted_crc[1] = (uint8_t)crc;

  crc = 0;
  if(lua_isnumber(L, 4))
    crc = lua_tointeger(L, 4);
  if(crc > 65535)
    return luaL_error( L, "wrong arg range" );

  lua_pushboolean( L, platform_onewire_check_crc16((uint8_t *)pdata, datalen, inverted_crc, crc) );

  return 1;
}

// uint16_t onewire_crc16(const uint8_t* input, uint16_t len, uint16_t crc);
// Lua: r = ow.crc16( buf, crc )
static int ow_crc16( lua_State *L )
{
  int stack = 0;
  size_t datalen;

  const char *pdata = luaL_checklstring( L, ++stack, &datalen );
  luaL_argcheck( L, datalen <= 65535, stack, "wrong arg range" );

  int crc = 0;
  if(lua_isnumber(L, ++stack))
    crc = lua_tointeger(L, stack);
  luaL_argcheck( L, crc >= 0 && crc <= 65535, stack, "wrong arg range" );

  lua_pushinteger( L, platform_onewire_crc16((uint8_t *)pdata, datalen, crc) );

  return 1;
}

LROT_BEGIN(ow, NULL, 0)
  LROT_FUNCENTRY( setup,         ow_setup )
  LROT_FUNCENTRY( reset,         ow_reset )
  LROT_FUNCENTRY( skip,          ow_skip )
  LROT_FUNCENTRY( select,        ow_select )
  LROT_FUNCENTRY( write,         ow_write )
  LROT_FUNCENTRY( write_bytes,   ow_write_bytes )
  LROT_FUNCENTRY( read,          ow_read )
  LROT_FUNCENTRY( read_bytes,    ow_read_bytes )
  LROT_FUNCENTRY( depower,       ow_depower )
  LROT_FUNCENTRY( reset_search,  ow_reset_search )
  LROT_FUNCENTRY( target_search, ow_target_search )
  LROT_FUNCENTRY( search,        ow_search )
  LROT_FUNCENTRY( crc8,          ow_crc8 )
  LROT_FUNCENTRY( check_crc16,   ow_check_crc16 )
  LROT_FUNCENTRY( crc16,         ow_crc16 )
LROT_END(ow, NULL, 0)

int luaopen_ow( lua_State *L )
{
  // create the table to store bus userdata for search operations
  lua_newtable( L );
  ow_bus_table_ref = luaL_ref( L, LUA_REGISTRYINDEX );

  return 0;
}

NODEMCU_MODULE(OW, "ow", ow, luaopen_ow);
