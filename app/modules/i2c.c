// Module for interfacing with the I2C interface

#include "module.h"
#include "lauxlib.h"
#include "platform.h"

// Lua: speed = i2c.setup( id, sda, scl, speed )
static int i2c_setup( lua_State *L )
{
  unsigned id = luaL_checkinteger( L, 1 );
  unsigned sda = luaL_checkinteger( L, 2 );
  unsigned scl = luaL_checkinteger( L, 3 );

  MOD_CHECK_ID( i2c, id );
  MOD_CHECK_ID( gpio, sda );
  MOD_CHECK_ID( gpio, scl );

  if(scl==0 || sda==0)
    return luaL_error( L, "no i2c for D0" );

  s32 speed = ( s32 )luaL_checkinteger( L, 4 );
  if (speed <= 0)
    return luaL_error( L, "wrong arg range" );
  lua_pushinteger( L, platform_i2c_setup( id, sda, scl, (u32)speed ) );
  return 1;
}

// Lua: i2c.start( id )
static int i2c_start( lua_State *L )
{
  unsigned id = luaL_checkinteger( L, 1 );

  MOD_CHECK_ID( i2c, id );
  platform_i2c_send_start( id );
  return 0;
}

// Lua: i2c.stop( id )
static int i2c_stop( lua_State *L )
{
  unsigned id = luaL_checkinteger( L, 1 );

  MOD_CHECK_ID( i2c, id );
  platform_i2c_send_stop( id );
  return 0;
}

// Lua: status = i2c.address( id, address, direction )
static int i2c_address( lua_State *L )
{
  unsigned id = luaL_checkinteger( L, 1 );
  int address = luaL_checkinteger( L, 2 );
  int direction = luaL_checkinteger( L, 3 );

  MOD_CHECK_ID( i2c, id );
  if ( address < 0 || address > 127 )
    return luaL_error( L, "wrong arg range" );
  lua_pushboolean( L, platform_i2c_send_address( id, (u16)address, direction ) );
  return 1;
}

// Lua: wrote = i2c.write( id, data1, [data2], ..., [datan] )
// data can be either a string, a table or an 8-bit number
static int i2c_write( lua_State *L )
{
  unsigned id = luaL_checkinteger( L, 1 );
  const char *pdata;
  size_t datalen, i;
  int numdata;
  u32 wrote = 0;
  unsigned argn;

  MOD_CHECK_ID( i2c, id );
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
      if( platform_i2c_send_byte( id, numdata ) != 1 )
        break;
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
        if( platform_i2c_send_byte( id, numdata ) == 0 )
          break;
      }
      wrote += i;
      if( i < datalen )
        break;
    }
    else
    {
      pdata = luaL_checklstring( L, argn, &datalen );
      for( i = 0; i < datalen; i ++ )
        if( platform_i2c_send_byte( id, pdata[ i ] ) == 0 )
          break;
      wrote += i;
      if( i < datalen )
        break;
    }
  }
  lua_pushinteger( L, wrote );
  return 1;
}

// Lua: read = i2c.read( id, size )
static int i2c_read( lua_State *L )
{
  unsigned id = luaL_checkinteger( L, 1 );
  u32 size = ( u32 )luaL_checkinteger( L, 2 ), i;
  luaL_Buffer b;
  int data;

  MOD_CHECK_ID( i2c, id );
  if( size == 0 )
    return 0;
  luaL_buffinit( L, &b );
  for( i = 0; i < size; i ++ )
    if( ( data = platform_i2c_recv_byte( id, i < size - 1 ) ) == -1 )
      break;
    else
      luaL_addchar( &b, ( char )data );
  luaL_pushresult( &b );
  return 1;
}

// Module function map
static const LUA_REG_TYPE i2c_map[] = {
  { LSTRKEY( "setup" ),       LFUNCVAL( i2c_setup ) },
  { LSTRKEY( "start" ),       LFUNCVAL( i2c_start ) },
  { LSTRKEY( "stop" ),        LFUNCVAL( i2c_stop ) },
  { LSTRKEY( "address" ),     LFUNCVAL( i2c_address ) },
  { LSTRKEY( "write" ),       LFUNCVAL( i2c_write ) },
  { LSTRKEY( "read" ),        LFUNCVAL( i2c_read ) },
 //{ LSTRKEY( "FAST" ),       LNUMVAL( PLATFORM_I2C_SPEED_FAST ) },
  { LSTRKEY( "SLOW" ),        LNUMVAL( PLATFORM_I2C_SPEED_SLOW ) },
  { LSTRKEY( "TRANSMITTER" ), LNUMVAL( PLATFORM_I2C_DIRECTION_TRANSMITTER ) },
  { LSTRKEY( "RECEIVER" ),    LNUMVAL( PLATFORM_I2C_DIRECTION_RECEIVER ) },
  { LNILKEY, LNILVAL }
};

NODEMCU_MODULE(I2C, "i2c", i2c_map, NULL);
