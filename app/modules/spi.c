// Module for interfacing with the SPI interface

#include "module.h"
#include "lauxlib.h"
#include "platform.h"

#include "driver/spi.h"

#define SPI_HALFDUPLEX 0
#define SPI_FULLDUPLEX 1

static u8 spi_databits[NUM_SPI] = {0, 0};
static u8 spi_duplex[NUM_SPI] = {SPI_HALFDUPLEX, SPI_HALFDUPLEX};

// Lua: = spi.setup( id, mode, cpol, cpha, databits, clock_div, [duplex_mode] )
static int spi_setup( lua_State *L )
{
  int id          = luaL_checkinteger( L, 1 );
  int mode        = luaL_checkinteger( L, 2 );
  int cpol        = luaL_checkinteger( L, 3 );
  int cpha        = luaL_checkinteger( L, 4 );
  int databits    = luaL_checkinteger( L, 5 );
  u32 clock_div   = luaL_checkinteger( L, 6 );
  int duplex_mode = luaL_optinteger( L, 7, SPI_HALFDUPLEX );

  MOD_CHECK_ID( spi, id );

  if (mode != PLATFORM_SPI_SLAVE && mode != PLATFORM_SPI_MASTER) {
    return luaL_error( L, "wrong arg type" );
  }

  if (cpol != PLATFORM_SPI_CPOL_LOW && cpol != PLATFORM_SPI_CPOL_HIGH) {
    return luaL_error( L, "wrong arg type" );
  }

  if (cpha != PLATFORM_SPI_CPHA_LOW && cpha != PLATFORM_SPI_CPHA_HIGH) {
    return luaL_error( L, "wrong arg type" );
  }

  if (databits < 0 || databits > 32) {
    return luaL_error( L, "out of range" );
  }

  if (clock_div == 0) {
    // defaulting to 8 for backward compatibility
    clock_div = 8;
  }

  if (duplex_mode == SPI_HALFDUPLEX || duplex_mode == SPI_FULLDUPLEX)
  {
    spi_duplex[id] = duplex_mode;
  }
  else
  {
    return luaL_error( L, "out of range" );
  }

  spi_databits[id] = databits;

  u32 res = platform_spi_setup(id, mode, cpol, cpha, clock_div);
  lua_pushinteger( L, res );
  return 1;
}

// Half-duplex mode:
// Lua: wrote  = spi.send( id, data1, [data2], ..., [datan] )
// Full-duplex mode:
// Lua: wrote, [data1], ..., [datan]  = spi.send_recv( id, data1, [data2], ..., [datan] )
// data can be either a string, a table or an 8-bit number
static int spi_send_recv( lua_State *L )
{
  unsigned id = luaL_checkinteger( L, 1 );
  const char *pdata;
  size_t datalen, i;
  u32 numdata;
  u32 wrote = 0;
  int pushed = 1;
  unsigned argn, tos;
  u8 recv = spi_duplex[id] == SPI_FULLDUPLEX ? 1 : 0;

  MOD_CHECK_ID( spi, id );
  if( (tos = lua_gettop( L )) < 2 )
    return luaL_error( L, "wrong arg type" );

  // prepare first returned item 'wrote' - value is yet unknown
  // position on stack is tos+1
  lua_pushinteger( L, 0 );

  for( argn = 2; argn <= tos; argn ++ )
  {
    // *** Send integer value and return received data as integer ***
    // lua_isnumber() would silently convert a string of digits to an integer
    // whereas here strings are handled separately.
    if( lua_type( L, argn ) == LUA_TNUMBER )
    {
      numdata = luaL_checkinteger( L, argn );
      if (recv > 0)
      {
        lua_pushinteger( L, platform_spi_send_recv( id, spi_databits[id], numdata ) );
        pushed ++;
      }
      else
      {
        platform_spi_send( id, spi_databits[id], numdata );
      }
      wrote ++;
    }

    // *** Send table elements and return received data items as a table ***
    else if( lua_istable( L, argn ) )
    {
      datalen = lua_objlen( L, argn );

      if (recv > 0 && datalen > 0) {
        // create a table for the received data
        lua_createtable( L, datalen, 0 );
        pushed ++;
      }

      for( i = 0; i < datalen; i ++ )
      {
        lua_rawgeti( L, argn, i + 1 );
        numdata = luaL_checkinteger( L, -1 );
        lua_pop( L, 1 );
        if (recv > 0) {
          lua_pushinteger( L, platform_spi_send_recv( id, spi_databits[id], numdata ) );
          lua_rawseti( L, -2, i + 1 );
        }
        else
        {
          platform_spi_send( id, spi_databits[id], numdata );
        }
      }
      wrote += i;
      if( i < datalen )
        break;
    }

    // *** Send characters of a string and return received data items as string ***
    else
    {
      luaL_Buffer b;

      pdata = luaL_checklstring( L, argn, &datalen );
      if (recv > 0) {
        luaL_buffinit( L, &b );
      }

      for( i = 0; i < datalen; i ++ )
      {
        if (recv > 0)
        {
          luaL_addchar( &b, (char)platform_spi_send_recv( id, spi_databits[id], pdata[ i ] ) );
        }
        else
        {
          platform_spi_send( id, spi_databits[id], pdata[ i ] );
        }
      }
      if (recv > 0 && datalen > 0) {
        luaL_pushresult( &b );
        pushed ++;
      }
      wrote += i;
      if( i < datalen )
        break;
    }
  }

  // update item 'wrote' on stack
  lua_pushinteger( L, wrote );
  lua_replace( L, tos+1 );
  return pushed;
}

// Lua: read = spi.recv( id, size, [default data] )
static int spi_recv( lua_State *L )
{
  int id   = luaL_checkinteger( L, 1 );
  int size = luaL_checkinteger( L, 2 ), i;
  int def  = luaL_optinteger( L, 3, 0xffffffff );

  luaL_Buffer b;

  MOD_CHECK_ID( spi, id );
  if (size == 0) {
    return 0;
  }

  luaL_buffinit( L, &b );
  for (i=0; i<size; i++)
  {
    luaL_addchar( &b, ( char )platform_spi_send_recv( id, spi_databits[id], def ) );
  }

  luaL_pushresult( &b );
  return 1;
}

// Lua: spi.set_mosi( id, offset, bitlen, data1, [data2], ..., [datan] )
// Lua: spi.set_mosi( id, string )
static int spi_set_mosi( lua_State *L )
{
  int id = luaL_checkinteger( L, 1 );

  MOD_CHECK_ID( spi, id );

  if (lua_type( L, 2 ) == LUA_TSTRING) {
    size_t len;
    const char *data = luaL_checklstring( L, 2, &len );
    luaL_argcheck( L, 2, len <= 64, "out of range" );

    spi_mast_blkset( id, len * 8, data );

  } else {
    int offset = luaL_checkinteger( L, 2 );
    int bitlen = luaL_checkinteger( L, 3 );

    luaL_argcheck( L, 2, offset >= 0 && offset <= 511, "out of range" );
    luaL_argcheck( L, 3, bitlen >= 1 && bitlen <= 32, "out of range" );

    for (int argn = 4; argn <= lua_gettop( L ); argn++, offset += bitlen ) {
      u32 data = ( u32 )luaL_checkinteger(L, argn );

      if (offset + bitlen > 512) {
        return luaL_error( L, "data range exceeded > 512 bits" );
      }

      spi_mast_set_mosi( id, offset, bitlen, data );
    }
  }

  return 0;
}

// Lua: data = spi.get_miso( id, offset, bitlen, num )
// Lua: string = spi.get_miso( id, len )
static int spi_get_miso( lua_State *L )
{
  int id = luaL_checkinteger( L, 1 );

  MOD_CHECK_ID( spi, id );

  if (lua_gettop( L ) == 2) {
    uint8_t data[64];
    int len = luaL_checkinteger( L, 2 );

    luaL_argcheck( L, 2, len >= 1 && len <= 64, "out of range" );

    spi_mast_blkget( id, len * 8, data );

    lua_pushlstring( L, data, len );
    return 1;

  } else {
    int offset = luaL_checkinteger( L, 2 );
    int bitlen = luaL_checkinteger( L, 3 );
    int num    = luaL_checkinteger( L, 4 ), i;

    luaL_argcheck( L, 2, offset >= 0 && offset <= 511, "out of range" );
    luaL_argcheck( L, 3, bitlen >= 1 && bitlen <= 32, "out of range" );

    if (offset + bitlen * num > 512) {
      return luaL_error( L, "out of range" );
    }

    for (i = 0; i < num; i++) {
      lua_pushinteger( L, spi_mast_get_miso( id, offset + (bitlen * i), bitlen ) );
    }
    return num;
  }
}

// Lua: spi.transaction( id, cmd_bitlen, cmd_data, addr_bitlen, addr_data, mosi_bitlen, dummy_bitlen, miso_bitlen )
static int spi_transaction( lua_State *L )
{
  int id = luaL_checkinteger( L, 1 );

  MOD_CHECK_ID( spi, id );

  int cmd_bitlen = luaL_checkinteger( L, 2 );
  u16 cmd_data   = ( u16 )luaL_checkinteger( L, 3 );
  luaL_argcheck( L, 2, cmd_bitlen >= 0 && cmd_bitlen <= 16, "out of range" );

  int addr_bitlen = luaL_checkinteger( L, 4 );
  u32 addr_data   = ( u32 )luaL_checkinteger( L, 5 );
  luaL_argcheck( L, 4, addr_bitlen >= 0 && addr_bitlen <= 32, "out of range" );

  int mosi_bitlen = luaL_checkinteger( L, 6 );
  luaL_argcheck( L, 6, mosi_bitlen >= 0 && mosi_bitlen <= 512, "out of range" );

  int dummy_bitlen = luaL_checkinteger( L, 7 );
  luaL_argcheck( L, 7, dummy_bitlen >= 0 && dummy_bitlen <= 256, "out of range" );

  int miso_bitlen = luaL_checkinteger( L, 8 );
  luaL_argcheck( L, 8, miso_bitlen >= -512 && miso_bitlen <= 512, "out of range" );

  spi_mast_transaction( id, cmd_bitlen, cmd_data, addr_bitlen, addr_data,
			mosi_bitlen, dummy_bitlen, miso_bitlen );

  return 0;
}


// Module function map
static const LUA_REG_TYPE spi_map[] = {
  { LSTRKEY( "setup" ),       LFUNCVAL( spi_setup ) },
  { LSTRKEY( "send" ),        LFUNCVAL( spi_send_recv ) },
  { LSTRKEY( "recv" ),        LFUNCVAL( spi_recv ) },
  { LSTRKEY( "set_mosi" ),    LFUNCVAL( spi_set_mosi ) },
  { LSTRKEY( "get_miso" ),    LFUNCVAL( spi_get_miso ) },
  { LSTRKEY( "transaction" ), LFUNCVAL( spi_transaction ) },
  { LSTRKEY( "MASTER" ),      LNUMVAL( PLATFORM_SPI_MASTER ) },
  { LSTRKEY( "SLAVE" ),       LNUMVAL( PLATFORM_SPI_SLAVE) },
  { LSTRKEY( "CPHA_LOW" ),    LNUMVAL( PLATFORM_SPI_CPHA_LOW) },
  { LSTRKEY( "CPHA_HIGH" ),   LNUMVAL( PLATFORM_SPI_CPHA_HIGH) },
  { LSTRKEY( "CPOL_LOW" ),    LNUMVAL( PLATFORM_SPI_CPOL_LOW) },
  { LSTRKEY( "CPOL_HIGH" ),   LNUMVAL( PLATFORM_SPI_CPOL_HIGH) },
  { LSTRKEY( "DATABITS_8" ),  LNUMVAL( 8 ) },
  { LSTRKEY( "HALFDUPLEX" ),  LNUMVAL( SPI_HALFDUPLEX ) },
  { LSTRKEY( "FULLDUPLEX" ),  LNUMVAL( SPI_FULLDUPLEX ) },
  { LNILKEY, LNILVAL }
};

NODEMCU_MODULE(SPI, "spi", spi_map, NULL);
