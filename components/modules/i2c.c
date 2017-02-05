// Module for interfacing with the I2C interface

#include "module.h"
#include "lauxlib.h"
#include "lextra.h"
#include "platform.h"

#include "i2c_common.h"


// Lua: i2c.setup( id, sda, scl, speed )
static int i2c_setup( lua_State *L )
{
  unsigned id = luaL_checkinteger( L, 1 );
  unsigned sda = luaL_checkinteger( L, 2 );
  unsigned scl = luaL_checkinteger( L, 3 );
  uint32_t speed = (uint32_t)luaL_checkinteger( L, 4 );

  luaL_argcheck( L, id < I2C_ID_MAX, 1, "invalid id" );
  MOD_CHECK_ID( gpio, sda );
  MOD_CHECK_ID( gpio, scl );

  if (id == I2C_ID_SW) {
    luaL_argcheck( L, speed <= PLATFORM_I2C_SPEED_SLOW, 4, "wrong arg range" );
    if (!platform_i2c_setup( id, sda, scl, (uint32_t)speed ))
      luaL_error( L, "setup failed" );
  } else {
    luaL_argcheck( L, speed <= PLATFORM_I2C_SPEED_FASTPLUS, 4, "wrong arg range" );
    li2c_hw_master_setup( L, id, sda, scl, speed );
  }

  return 0;
}

// Lua: i2c.start( id )
static int i2c_start( lua_State *L )
{
  unsigned id = luaL_checkinteger( L, 1 );

  luaL_argcheck( L, id < I2C_ID_MAX, 1, "invalid id" );

  if (id == I2C_ID_SW) {
    if (!platform_i2c_send_start( id ))
      luaL_error( L, "command failed");
  } else {
    li2c_hw_master_start( L, id );
  }

  return 0;
}

// Lua: i2c.stop( id )
static int i2c_stop( lua_State *L )
{
  unsigned id = luaL_checkinteger( L, 1 );

  luaL_argcheck( L, id < I2C_ID_MAX, 1, "invalid id" );

  if (id == I2C_ID_SW) {
    if (!platform_i2c_send_stop( id ))
      luaL_error( L, "command failed" );
  } else {
    li2c_hw_master_stop( L, id );
  }

  return 0;
}

// Lua: status = i2c.address( id, address, direction )
static int i2c_address( lua_State *L )
{
  int stack = 0;

  unsigned id = luaL_checkinteger( L, ++stack );
  luaL_argcheck( L, id < I2C_ID_MAX, stack, "invalid id" );

  int address = luaL_checkinteger( L, ++stack );
  luaL_argcheck( L, address >= 0 && address <= 127, stack, "wrong arg range" );

  int direction = luaL_checkinteger( L, ++stack );
  luaL_argcheck( L,
		 direction == PLATFORM_I2C_DIRECTION_TRANSMITTER ||
		 direction == PLATFORM_I2C_DIRECTION_RECEIVER,
		 stack, "wrong arg range" );

  bool ack_check_en = luaL_optbool( L, ++stack, true );

  int ret;
  if (id == I2C_ID_SW) {
    ret = platform_i2c_send_address( id, (uint16_t)address, direction, ack_check_en ? 1 : 0 );
  } else {
    ret = li2c_hw_master_address( L, id, (uint16_t)address, direction, ack_check_en );
  }
  lua_pushboolean( L,  ret > 0 );

  return 1;
}

// Lua: wrote = i2c.write( id, data1, [data2], ..., [datan] )
// data can be either a string, a table or an 8-bit number
static int i2c_write( lua_State *L )
{
  unsigned id = luaL_checkinteger( L, 1 );
  luaL_argcheck( L, id < I2C_ID_MAX, 1, "invalid id" );

  const char *pdata;
  size_t datalen, i;
  int numdata;
  uint32_t wrote = 0;
  unsigned argn;

  bool ack_check_en = true;
  if (lua_isboolean( L, -1 )) {
    ack_check_en = lua_toboolean( L, -1 );
    lua_pop( L, 1 );
  }

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
      if (id == I2C_ID_SW) {
	if( platform_i2c_send_byte( id, numdata, 1) != 1 && ack_check_en )
	  break;
      } else {
	li2c_hw_master_write( L, id, numdata, ack_check_en );
      }
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
	if (id == I2C_ID_SW) {
	  if( platform_i2c_send_byte( id, numdata, 1 ) == 0 && ack_check_en)
	    break;
	} else {
	  li2c_hw_master_write( L, id, numdata, ack_check_en );
	}
      }
      wrote += i;
      if( i < datalen )
        break;
    }
    else
    {
      pdata = luaL_checklstring( L, argn, &datalen );
      for( i = 0; i < datalen; i ++ )
	if (id == I2C_ID_SW) {
	  if( platform_i2c_send_byte( id, pdata[ i ], 1 ) == 0 && ack_check_en)
	    break;
	} else {
	  li2c_hw_master_write( L, id, pdata[ i ], ack_check_en );
	}
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
  luaL_argcheck( L, id < I2C_ID_MAX, 1, "invalid id" );

  uint32_t size = ( uint32_t )luaL_checkinteger( L, 2 ), i;

  if (id == I2C_ID_SW) {
    luaL_Buffer b;
    int data;

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
  } else {
    li2c_hw_master_read( L, id, size );
    return 0;
  }
}

static const LUA_REG_TYPE i2c_map[] = {
  { LSTRKEY( "setup" ),       LFUNCVAL( i2c_setup ) },
  { LSTRKEY( "start" ),       LFUNCVAL( i2c_start ) },
  { LSTRKEY( "stop" ),        LFUNCVAL( i2c_stop ) },
  { LSTRKEY( "address" ),     LFUNCVAL( i2c_address ) },
  { LSTRKEY( "write" ),       LFUNCVAL( i2c_write ) },
  { LSTRKEY( "read" ),        LFUNCVAL( i2c_read ) },
  { LSTRKEY( "transfer" ),    LFUNCVAL( li2c_hw_master_transfer ) },
  { LSTRKEY( "slave"),        LROVAL( li2c_slave_map ) },
  { LSTRKEY( "FASTPLUS" ),    LNUMVAL( PLATFORM_I2C_SPEED_FASTPLUS ) },
  { LSTRKEY( "FAST" ),        LNUMVAL( PLATFORM_I2C_SPEED_FAST ) },
  { LSTRKEY( "SLOW" ),        LNUMVAL( PLATFORM_I2C_SPEED_SLOW ) },
  { LSTRKEY( "TRANSMITTER" ), LNUMVAL( PLATFORM_I2C_DIRECTION_TRANSMITTER ) },
  { LSTRKEY( "RECEIVER" ),    LNUMVAL( PLATFORM_I2C_DIRECTION_RECEIVER ) },
  { LSTRKEY( "SW" ),          LNUMVAL( I2C_ID_SW ) },
  { LSTRKEY( "HW0" ),         LNUMVAL( I2C_ID_HW0 ) },
  { LSTRKEY( "HW1" ),         LNUMVAL( I2C_ID_HW1 ) },
  {LNILKEY, LNILVAL}
};


int luaopen_i2c( lua_State *L ) {
  li2c_hw_master_init( L );
  li2c_hw_slave_init( L );
  return 0;
}


NODEMCU_MODULE(I2C, "i2c", i2c_map, luaopen_i2c);
