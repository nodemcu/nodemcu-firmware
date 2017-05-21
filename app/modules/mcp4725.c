/*
 * Driver for Microchip MCP4725 12-bit digital to analog converter.
 */

#include "module.h"
#include "lauxlib.h"
#include "platform.h"
#include "osapi.h"

#define MCP4725_I2C_ADDR_BASE      (0x60)
#define MCP4725_I2C_ADDR_A0_MASK   (0x01) // user configurable
#define MCP4725_I2C_ADDR_A1_MASK   (0x02) // hard wired at factory
#define MCP4725_I2C_ADDR_A2_MASK   (0x04) // hard wired at factory

#define MCP4725_COMMAND_WRITE_DAC (0x40)
#define MCP4725_COMMAND_WRITE_DAC_EEPROM (0x60)

#define MCP4725_POWER_DOWN_NORMAL (0x00)
#define MCP4725_POWER_DOWN_RES_1K (0x02)
#define MCP4725_POWER_DOWN_RES_100K (0x04)
#define MCP4725_POWER_DOWN_RES_500K (0x06)

static const unsigned mcp4725_i2c_id = 0;

static uint8 get_address(lua_State* L, uint8 i2c_address){
  uint8 addr_temp = i2c_address;
  uint16 temp_var = 0;
  lua_getfield(L, 1, "A2");
  if (!lua_isnil(L, -1))
  {
    if( lua_isnumber(L, -1) )
    {
      temp_var = lua_tonumber(L, -1);
      if(temp_var < 2){
        temp_var = MCP4725_I2C_ADDR_A2_MASK & (temp_var << 2);
        addr_temp|=temp_var;
      }
      else
        return luaL_argerror( L, 1, "A2: Must be 0 or 1" );
    }
    else
    {
      return luaL_argerror( L, 1, "A2: Must be number" );
    }
  }
  lua_pop(L, 1);

  lua_getfield(L, 1, "A1");
  if (!lua_isnil(L, -1))
  {
    if( lua_isnumber(L, -1) )
    {
      temp_var = lua_tonumber(L, -1);
      if(temp_var < 2){
        temp_var = MCP4725_I2C_ADDR_A1_MASK & (temp_var << 1);
        addr_temp|=temp_var;
      }
      else
        return luaL_argerror( L, 1, "A1: Must be 0 or 1" );
    }
    else
    {
      return luaL_argerror( L, 1, "A1: Must be number" );
    }
  }
  lua_pop(L, 1);

  lua_getfield(L, 1, "A0");
  if (!lua_isnil(L, -1))
  {
    if( lua_isnumber(L, -1) )
    {
      temp_var = lua_tonumber(L, -1);
      if(temp_var<2){
        temp_var = MCP4725_I2C_ADDR_A0_MASK & (temp_var);
        addr_temp|=temp_var;
      }
      else
        return luaL_argerror( L, 1, "A0: Must be 0 or 1" );
    }
    else
    {
      return luaL_argerror( L, 1, "A0: Must be number" );
    }
  }
  lua_pop(L, 1);

  return addr_temp;
}

static int mcp4725_write(lua_State* L){

  uint8 i2c_address = MCP4725_I2C_ADDR_BASE;
  uint16 dac_value = 0;
  uint8  cmd_byte = 0;

  if(lua_istable(L, 1))
  {
    i2c_address = get_address(L, i2c_address);
    uint16 temp_var=0;
    lua_getfield(L, 1, "value");
    if (!lua_isnil(L, -1))
    {
      if( lua_isnumber(L, -1) )
      {
        temp_var = lua_tonumber(L, -1);
        if(temp_var >= 0 && temp_var<=4095){
          dac_value = temp_var<<4;
        }
        else
          return luaL_argerror( L, 1, "value: Valid range 0-4095" );
      }
      else
      {
        return luaL_argerror( L, 1, "value: Must be number" );
      }
    }
    else
    {
      return luaL_argerror( L, 1, "value: value is required" );
    }
    lua_pop(L, 1);

    lua_getfield(L, 1, "save");
    if (!lua_isnil(L, -1))
    {
      if( lua_isboolean(L, -1) )
      {
        if(lua_toboolean(L, -1)){
          cmd_byte |= MCP4725_COMMAND_WRITE_DAC_EEPROM;
        }
        else{
          cmd_byte |= MCP4725_COMMAND_WRITE_DAC;
        }
      }
      else
      {
        return luaL_argerror( L, 1, "save: must be boolean" );
      }
    }
    else
    {
      cmd_byte |= MCP4725_COMMAND_WRITE_DAC;
    }
    lua_pop(L, 1);

    lua_getfield(L, 1, "pwrdn");
    if (!lua_isnil(L, -1))
    {
      if( lua_isnumber(L, -1) )
      {
        temp_var = lua_tonumber(L, -1);
        if(temp_var >= 0 && temp_var <= 3){
          cmd_byte |= temp_var << 1;
        }
        else{
          return luaL_argerror( L, 1, "pwrdn: Valid range 0-3" );
        }
      }
      else
      {
        return luaL_argerror( L, 1, "pwrdn: Must be number" );
      }
    }
    lua_pop(L, 1);

 }
  uint8 *dac_value_byte = (uint8*) & dac_value;

  platform_i2c_send_start(mcp4725_i2c_id);
  platform_i2c_send_address(mcp4725_i2c_id, i2c_address, PLATFORM_I2C_DIRECTION_TRANSMITTER);
  platform_i2c_send_byte(mcp4725_i2c_id, cmd_byte);
  platform_i2c_send_byte(mcp4725_i2c_id, dac_value_byte[1]);
  platform_i2c_send_byte(mcp4725_i2c_id, dac_value_byte[0]);
  platform_i2c_send_stop(mcp4725_i2c_id);

  return 0;
}

static int mcp4725_read(lua_State* L){
  uint8 i2c_address = MCP4725_I2C_ADDR_BASE;
  uint8 recieve_buffer[5] = {0};

  if(lua_istable(L, 1))
   {
    i2c_address = get_address(L, i2c_address);
   }

  platform_i2c_send_start(mcp4725_i2c_id);
  platform_i2c_send_address(mcp4725_i2c_id, i2c_address, PLATFORM_I2C_DIRECTION_RECEIVER);
  for(int i=0;i<5;i++){
    recieve_buffer[i] = platform_i2c_recv_byte(mcp4725_i2c_id, 1);
  }
  platform_i2c_send_stop(mcp4725_i2c_id);

  lua_pushnumber(L, (recieve_buffer[0] & 0x06)>>1);
  lua_pushnumber(L, (recieve_buffer[1] << 4) | (recieve_buffer[2] >> 4));
  lua_pushnumber(L, (recieve_buffer[3] & 0x60) >> 5);
  lua_pushnumber(L, ((recieve_buffer[3] & 0xf) << 8) | recieve_buffer[4]);
  lua_pushnumber(L, (recieve_buffer[0] & 0x80) >> 7);
  lua_pushnumber(L, (recieve_buffer[0] & 0x40) >> 6);

  return 6;
}


static const LUA_REG_TYPE mcp4725_map[] = {
    { LSTRKEY( "write" ),      LFUNCVAL( mcp4725_write ) },
    { LSTRKEY( "read" ),      LFUNCVAL( mcp4725_read ) },
    { LSTRKEY( "PWRDN_NONE" ),    LNUMVAL(MCP4725_POWER_DOWN_NORMAL) },
    { LSTRKEY( "PWRDN_1K" ),    LNUMVAL((MCP4725_POWER_DOWN_RES_1K)>>1) },
    { LSTRKEY( "PWRDN_100K" ),    LNUMVAL((MCP4725_POWER_DOWN_RES_100K)>>1) },
    { LSTRKEY( "PWRDN_500K" ),    LNUMVAL((MCP4725_POWER_DOWN_RES_500K)>>1) },
    { LNILKEY, LNILVAL}
};

NODEMCU_MODULE(MCP4725, "mcp4725", mcp4725_map, NULL);
