// Module for interfacing with the ESP32 I2C interfaces

#include "module.h"
#include "lauxlib.h"
#include "lextra.h"
#include "platform.h"

#include "i2c_common.h"


// Lua: speed = i2c.setup( id, sda, scl, speed [,stretchfactor] )
static int i2c_setup( lua_State *L )
{
  unsigned id = luaL_checkinteger( L, 1 );
  unsigned sda = luaL_checkinteger( L, 2 );
  unsigned scl = luaL_checkinteger( L, 3 );
  uint32_t speed = (uint32_t)luaL_checkinteger( L, 4 );
  int stretchfactor = 1;
  int timeout;

  luaL_argcheck( L, id < I2C_ID_MAX, 1, "invalid i2c interface id" );
  MOD_CHECK_ID( gpio, sda );
  MOD_CHECK_ID( gpio, scl );

  if (id == I2C_ID_SW) {
    luaL_argcheck( L, speed <= PLATFORM_I2C_SPEED_SLOW, 4, "i2c speed too high for i2c.SW interface" );
    if (!platform_i2c_setup( id, sda, scl, (uint32_t)speed ))
      luaL_error( L, "setup failed" );
    lua_pushinteger(L,speed);
  } else {
    luaL_argcheck( L, speed <= PLATFORM_I2C_SPEED_FASTPLUS, 4, "i2c speed too high for i2c.HWx interfaces" );
    if (lua_gettop( L ) > 4) stretchfactor = luaL_checkinteger( L, 5 );

    timeout=li2c_hw_master_setup( L, id, sda, scl, speed, stretchfactor);
    lua_pushinteger(L,timeout);
  }

  return 1;
}

// Lua: i2c.start( id )
static int i2c_start( lua_State *L )
{
  unsigned id = luaL_checkinteger( L, 1 );

  luaL_argcheck( L, id < I2C_ID_MAX, 1, "invalid i2c interface id" );

  if (id == I2C_ID_SW) {
    if (!platform_i2c_send_start( id ))
      luaL_error( L, "start command failed on i2c.SW interface");
  } else {
    li2c_hw_master_start( L, id );
  }

  return 0;
}

// Lua: i2c.stop( id )
static int i2c_stop( lua_State *L )
{
  unsigned id = luaL_checkinteger( L, 1 );

  luaL_argcheck( L, id < I2C_ID_MAX, 1, "invalid i2c interface id" );

  if (id == I2C_ID_SW) {
    if (!platform_i2c_send_stop( id ))
      luaL_error( L, "stop command failed on i2c.SW interface" );
  } else {
    li2c_hw_master_stop( L, id );
  }

  return 0;
}

// Lua: status = i2c.address( id , address , direction [, ack_check_en] )
static int i2c_address( lua_State *L )
{
  int stack = 0;

  unsigned id = luaL_checkinteger( L, ++stack );
  luaL_argcheck( L, id < I2C_ID_MAX, stack, "invalid i2c interface id" );

  int address = luaL_checkinteger( L, ++stack );
  luaL_argcheck( L, address >= 0 && address <= 127, stack, "i2c address out of 7-bit range" );
// *** FIX ME what about 10-bit addresses ???

  int direction = luaL_checkinteger( L, ++stack );
  luaL_argcheck( L,
		 direction == PLATFORM_I2C_DIRECTION_TRANSMITTER ||
		 direction == PLATFORM_I2C_DIRECTION_RECEIVER,
		 stack, "invalid i2c address direction" );

  bool ack_check_en = luaL_optbool( L, ++stack, true );

  int ret;
  if (id == I2C_ID_SW) {
    ret = platform_i2c_send_address( id, (uint16_t)address, direction, ack_check_en ? 1 : 0 );
  } else {
    ret = li2c_hw_master_address( L, id, (uint16_t)address, direction, ack_check_en );
// bad parameters could cause failure to enqueue, but all have been checked, so it should always return true
  }
  lua_pushboolean( L,  ret > 0 );

  return 1;
}

// Lua: wrote = i2c.write( id, data1, [data2], ..., [datan] [, ack_check_en] )
// each of the data elements can be either a string, a table or an 8-bit number
static int i2c_write( lua_State *L )
{
  unsigned id = luaL_checkinteger( L, 1 );
  luaL_argcheck( L, id < I2C_ID_MAX, 1, "invalid i2c interface id" );

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
    return luaL_error( L, "not enough or wrong arg types for i2c.write" );

  for( argn = 2; argn <= lua_gettop( L ); argn ++ )
  {
    // lua_isnumber() would silently convert a string of digits to an integer
    // whereas here strings are handled separately.
    if( lua_type( L, argn ) == LUA_TNUMBER )
    {
      numdata = ( int )luaL_checkinteger( L, argn );
      if( numdata < 0 || numdata > 255 )
        return luaL_error( L, "numeric value for i2c.write does not fit in 1 byte" );
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
          return luaL_error( L, "table value for i2c.write does not fit in 1 byte" );
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

// Lua: read = i2c.read( id, size ) for SW interface or i2c.read( id, size ) for HWx interfaces
static int i2c_read( lua_State *L )
{
  unsigned id = luaL_checkinteger( L, 1 );
  luaL_argcheck( L, id < I2C_ID_MAX, 1, "invalid i2c interface id" );

  uint32_t size = ( uint32_t ) luaL_checkinteger( L, 2 );
  luaL_argcheck( L, size > 0, 2, "i2c read size of 0 is invalid" );

  if (id == I2C_ID_SW) {
    luaL_Buffer b;
    uint32_t i;
    int data;
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

LROT_BEGIN(i2c)
  LROT_FUNCENTRY( setup,       i2c_setup )
  LROT_FUNCENTRY( start,       i2c_start )
  LROT_FUNCENTRY( stop,        i2c_stop )
  LROT_FUNCENTRY( address,     i2c_address )
  LROT_FUNCENTRY( write,       i2c_write )
  LROT_FUNCENTRY( read,        i2c_read )
  LROT_FUNCENTRY( transfer,    li2c_hw_master_transfer )
  LROT_TABENTRY ( slave,       li2c_slave )
  LROT_NUMENTRY ( FASTPLUS,    PLATFORM_I2C_SPEED_FASTPLUS )
  LROT_NUMENTRY ( FAST,        PLATFORM_I2C_SPEED_FAST )
  LROT_NUMENTRY ( SLOW,        PLATFORM_I2C_SPEED_SLOW )
  LROT_NUMENTRY ( TRANSMITTER, PLATFORM_I2C_DIRECTION_TRANSMITTER )
  LROT_NUMENTRY ( RECEIVER,    PLATFORM_I2C_DIRECTION_RECEIVER )
  LROT_NUMENTRY ( SW,          I2C_ID_SW )
  LROT_NUMENTRY ( HW0,         I2C_ID_HW0 )
  LROT_NUMENTRY ( HW1,         I2C_ID_HW1 )
LROT_END(i2c, NULL, 0)


int luaopen_i2c( lua_State *L ) {
  li2c_hw_master_init( L );
  li2c_hw_slave_init( L );
  return 0;
}


NODEMCU_MODULE(I2C, "i2c", i2c, luaopen_i2c);
