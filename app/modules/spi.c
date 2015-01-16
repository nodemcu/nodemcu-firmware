// Module for interfacing with the SPI interface

//#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
#include "platform.h"
#include "auxmods.h"
#include "lrotable.h"

// Lua: = spi.setup( id, mode, cpol, cpha, databits, clock )
static int spi_setup( lua_State *L )
{
  unsigned id = luaL_checkinteger( L, 1 );
  unsigned mode = luaL_checkinteger( L, 2 );
  unsigned cpol = luaL_checkinteger( L, 3 );
  unsigned cpha = luaL_checkinteger( L, 4 );
  unsigned databits = luaL_checkinteger( L, 5 );
  uint32_t clock = luaL_checkinteger( L, 6 );

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

  if (databits != PLATFORM_SPI_DATABITS_8 && databits != PLATFORM_SPI_DATABITS_16) {
    return luaL_error( L, "wrong arg type" );
  }

  u32 res = platform_spi_setup(id, mode, cpol, cpha, databits, clock);
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
      if( numdata < 0 || numdata > 255 )
        return luaL_error( L, "wrong arg range" );
      platform_spi_send_recv( id, numdata );
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
        if( numdata < 0 || numdata > 255 )
          return luaL_error( L, "wrong arg range" );
        platform_spi_send_recv( id, numdata );
      }
      wrote += i;
      if( i < datalen )
        break;
    }
    else
    {
      pdata = luaL_checklstring( L, argn, &datalen );
      for( i = 0; i < datalen; i ++ )
        platform_spi_send_recv( id, pdata[ i ] );
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
  unsigned id = luaL_checkinteger( L, 1 );
  u32 size = ( u32 )luaL_checkinteger( L, 2 ), i;

  luaL_Buffer b;
  spi_data_type data;

  MOD_CHECK_ID( spi, id );
  if (size == 0) {
    return 0;
  }

  luaL_buffinit( L, &b );
  for (i=0; i<size; i++) {
    data = platform_spi_send_recv(id, 0xFF);
    luaL_addchar( &b, ( char )data);
  }

  luaL_pushresult( &b );
  return 1;
}

// Module function map
#define MIN_OPT_LEVEL   2
#include "lrodefs.h"
const LUA_REG_TYPE spi_map[] = 
{
  { LSTRKEY( "setup" ),  LFUNCVAL( spi_setup ) },
  { LSTRKEY( "send" ),   LFUNCVAL( spi_send ) },
  { LSTRKEY( "recv" ),   LFUNCVAL( spi_recv ) },
#if LUA_OPTIMIZE_MEMORY > 0
  { LSTRKEY( "MASTER" ),    LNUMVAL( PLATFORM_SPI_MASTER ) },
  { LSTRKEY( "SLAVE" ),     LNUMVAL( PLATFORM_SPI_SLAVE) },
  { LSTRKEY( "CPHA_LOW" ),  LNUMVAL( PLATFORM_SPI_CPHA_LOW) },
  { LSTRKEY( "CPHA_HIGH" ), LNUMVAL( PLATFORM_SPI_CPHA_HIGH) },
  { LSTRKEY( "CPOL_LOW" ),  LNUMVAL( PLATFORM_SPI_CPOL_LOW) },
  { LSTRKEY( "CPOL_HIGH" ), LNUMVAL( PLATFORM_SPI_CPOL_HIGH) },
  { LSTRKEY( "DATABITS_8" ), LNUMVAL( PLATFORM_SPI_DATABITS_8) },
  { LSTRKEY( "DATABITS_16" ), LNUMVAL( PLATFORM_SPI_DATABITS_16) },
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
  MOD_REG_NUMBER( L, "DATABITS_8" , PLATFORM_SPI_DATABITS_8);
  MOD_REG_NUMBER( L, "DATABITS_16" , PLATFORM_SPI_DATABITS_16);

  return 1;
#endif // #if LUA_OPTIMIZE_MEMORY > 0
}

