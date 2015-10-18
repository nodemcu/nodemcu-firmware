// Module for interfacing with the SPI interface

//#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
#include "platform.h"
#include "auxmods.h"
#include "lrotable.h"

static u8 spi_databits[NUM_SPI] = {0, 0};

// Lua: = spi.setup( id, mode, cpol, cpha, databits, clock_div )
static int spi_setup( lua_State *L )
{
  int id        = luaL_checkinteger( L, 1 );
  int mode      = luaL_checkinteger( L, 2 );
  int cpol      = luaL_checkinteger( L, 3 );
  int cpha      = luaL_checkinteger( L, 4 );
  int databits  = luaL_checkinteger( L, 5 );
  u32 clock_div = luaL_checkinteger( L, 6 );

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

  if (clock_div < 4) {
    // defaulting to 8
    clock_div = 8;
  }

  spi_databits[id] = databits;

  u32 res = platform_spi_setup(id, mode, cpol, cpha, clock_div);
  lua_pushinteger( L, res );
  return 1;
}

// Lua: wrote = spi.send( id, data1, [data2], ..., [datan] )
// data can be either a string, a table or an 8-bit number
static int spi_send( lua_State *L )
{
  unsigned id = luaL_checkinteger( L, 1 );
  const char *pdata;
  size_t datalen, i;
  int numdata;
  u32 wrote = 0;
  unsigned argn;

  MOD_CHECK_ID( spi, id );
  if( lua_gettop( L ) < 2 )
    return luaL_error( L, "wrong arg type" );

  for( argn = 2; argn <= lua_gettop( L ); argn ++ )
  {
    // lua_isnumber() would silently convert a string of digits to an integer
    // whereas here strings are handled separately.
    if( lua_type( L, argn ) == LUA_TNUMBER )
    {
      numdata = ( int )luaL_checkinteger( L, argn );
      if( numdata < 0 )
        return luaL_error( L, "wrong arg range" );
      platform_spi_send( id, spi_databits[id], numdata );
      wrote ++;
    }
    else if( lua_istable( L, argn ) )
    {
      datalen = lua_objlen( L, argn );
      for( i = 0; i < datalen; i ++ )
      {
        lua_rawgeti( L, argn, i + 1 );
        numdata = ( int )luaL_checkinteger( L, -1 );
        lua_pop( L, 1 );
        if( numdata < 0 )
          return luaL_error( L, "wrong arg range" );
        platform_spi_send( id, spi_databits[id], numdata );
      }
      wrote += i;
      if( i < datalen )
        break;
    }
    else
    {
      pdata = luaL_checklstring( L, argn, &datalen );
      for( i = 0; i < datalen; i ++ )
        platform_spi_send( id, spi_databits[id], pdata[ i ] );
      wrote += i;
      if( i < datalen )
        break;
    }
  }

  lua_pushinteger( L, wrote );
  return 1;
}

// Lua: read = spi.recv( id, size )
static int spi_recv( lua_State *L )
{
  int id   = luaL_checkinteger( L, 1 );
  int size = luaL_checkinteger( L, 2 ), i;

  luaL_Buffer b;

  MOD_CHECK_ID( spi, id );
  if (size == 0) {
    return 0;
  }

  luaL_buffinit( L, &b );
  for (i=0; i<size; i++)
  {
    if (PLATFORM_OK != platform_spi_transaction( id, 0, 0, 0, 0, 0, 0, spi_databits[id] )) {
      return luaL_error( L, "failed" );
    }
    luaL_addchar( &b, ( char )platform_spi_get_miso( id, 0, spi_databits[id] ) );
  }

  luaL_pushresult( &b );
  return 1;
}

// Lua: spi.set_mosi( id, offset, bitlen, data1, [data2], ..., [datan] )
static int spi_set_mosi( lua_State *L )
{
  int id       = luaL_checkinteger( L, 1 );
  int offset   = luaL_checkinteger( L, 2 );
  int bitlen   = luaL_checkinteger( L, 3 );
  int argn;

  MOD_CHECK_ID( spi, id );

  if (offset < 0 || offset > 511) {
    return luaL_error( L, "offset out of range" );
  }

  if (bitlen < 1 || bitlen > 32) {
    return luaL_error( L, "bitlen out of range" );
  }

  if (lua_gettop( L ) < 4) {
    return luaL_error( L, "too few args" );
  }

  for (argn = 4; argn <= lua_gettop( L ); argn++, offset += bitlen )
  {
    u32 data = ( u32 )luaL_checkinteger(L, argn );

    if (offset + bitlen > 512) {
      return luaL_error( L, "data range exceeded > 512 bits" );
    }

    if (PLATFORM_OK != platform_spi_set_mosi( id, offset, bitlen, data )) {
      return luaL_error( L, "failed" );
    }
  }

  return 0;
}

// Lua: data = spi.get_miso( id, offset, bitlen, num )
static int spi_get_miso( lua_State *L )
{
  int id       = luaL_checkinteger( L, 1 );
  int offset   = luaL_checkinteger( L, 2 );
  int bitlen   = luaL_checkinteger( L, 3 );
  int num      = luaL_checkinteger( L, 4 ), i;

  MOD_CHECK_ID( spi, id );

  if (offset < 0 || offset > 511) {
    return luaL_error( L, "out of range" );
  }

  if (bitlen < 1 || bitlen > 32) {
    return luaL_error( L, "bitlen out of range" );
  }

  if (offset + bitlen * num > 512) {
    return luaL_error( L, "out of range" );
  }

  for (i = 0; i < num; i++)
  {
    lua_pushinteger( L, platform_spi_get_miso( id, offset + (bitlen * i), bitlen ) );
  }
  return num;
}

// Lua: spi.transaction( id, cmd_bitlen, cmd_data, addr_bitlen, addr_data, mosi_bitlen, dummy_bitlen, miso_bitlen )
static int spi_transaction( lua_State *L )
{
  int id           = luaL_checkinteger( L, 1 );
  int cmd_bitlen   = luaL_checkinteger( L, 2 );
  u16 cmd_data     = ( u16 )luaL_checkinteger( L, 3 );
  int addr_bitlen  = luaL_checkinteger( L, 4 );
  u32 addr_data    = ( u32 )luaL_checkinteger( L, 5 );
  int mosi_bitlen  = luaL_checkinteger( L, 6 );
  int dummy_bitlen = luaL_checkinteger( L, 7 );
  int miso_bitlen  = luaL_checkinteger( L, 8 );

  MOD_CHECK_ID( spi, id );

  if (cmd_bitlen < 0 || cmd_bitlen > 16) {
    return luaL_error( L, "cmd_bitlen out of range" );
  }

  if (addr_bitlen < 0 || addr_bitlen > 32) {
    return luaL_error( L, "addr_bitlen out of range" );
  }

  if (mosi_bitlen < 0 || mosi_bitlen > 512) {
    return luaL_error( L, "mosi_bitlen out of range" );
  }

  if (dummy_bitlen < 0 || dummy_bitlen > 256) {
    return luaL_error( L, "dummy_bitlen out of range" );
  }

  if (miso_bitlen < 0 || miso_bitlen > 511) {
    return luaL_error( L, "miso_bitlen out of range" );
  }

  if (PLATFORM_OK != platform_spi_transaction( id, cmd_bitlen, cmd_data, addr_bitlen, addr_data,
                                               mosi_bitlen, dummy_bitlen, miso_bitlen) ) {
    return luaL_error( L, "failed" );
  }

  return 0;
}


// Module function map
#define MIN_OPT_LEVEL   2
#include "lrodefs.h"
const LUA_REG_TYPE spi_map[] = 
{
  { LSTRKEY( "setup" ),       LFUNCVAL( spi_setup ) },
  { LSTRKEY( "send" ),        LFUNCVAL( spi_send ) },
  { LSTRKEY( "recv" ),        LFUNCVAL( spi_recv ) },
  { LSTRKEY( "set_mosi" ),    LFUNCVAL( spi_set_mosi ) },
  { LSTRKEY( "get_miso" ),    LFUNCVAL( spi_get_miso ) },
  { LSTRKEY( "transaction" ), LFUNCVAL( spi_transaction ) },
#if LUA_OPTIMIZE_MEMORY > 0
  { LSTRKEY( "MASTER" ),    LNUMVAL( PLATFORM_SPI_MASTER ) },
  { LSTRKEY( "SLAVE" ),     LNUMVAL( PLATFORM_SPI_SLAVE) },
  { LSTRKEY( "CPHA_LOW" ),  LNUMVAL( PLATFORM_SPI_CPHA_LOW) },
  { LSTRKEY( "CPHA_HIGH" ), LNUMVAL( PLATFORM_SPI_CPHA_HIGH) },
  { LSTRKEY( "CPOL_LOW" ),  LNUMVAL( PLATFORM_SPI_CPOL_LOW) },
  { LSTRKEY( "CPOL_HIGH" ), LNUMVAL( PLATFORM_SPI_CPOL_HIGH) },
  { LSTRKEY( "DATABITS_8" ), LNUMVAL( 8 ) },
#endif // #if LUA_OPTIMIZE_MEMORY > 0
  { LNILKEY, LNILVAL }
};

LUALIB_API int luaopen_spi( lua_State *L )
{
#if LUA_OPTIMIZE_MEMORY > 0
  return 0;
#else // #if LUA_OPTIMIZE_MEMORY > 0
  luaL_register( L, AUXLIB_SPI, spi_map );
  
  // Add constants
  MOD_REG_NUMBER( L, "MASTER",  PLATFORM_SPI_MASTER);
  MOD_REG_NUMBER( L, "SLAVE",   PLATFORM_SPI_SLAVE);
  MOD_REG_NUMBER( L, "CPHA_LOW" , PLATFORM_SPI_CPHA_LOW);
  MOD_REG_NUMBER( L, "CPHA_HIGH", PLATFORM_SPI_CPHA_HIGH);
  MOD_REG_NUMBER( L, "CPOL_LOW" , PLATFORM_SPI_CPOL_LOW);
  MOD_REG_NUMBER( L, "CPOL_HIGH", PLATFORM_SPI_CPOL_HIGH);
  MOD_REG_NUMBER( L, "DATABITS_8", 8 );

  return 1;
#endif // #if LUA_OPTIMIZE_MEMORY > 0
}
