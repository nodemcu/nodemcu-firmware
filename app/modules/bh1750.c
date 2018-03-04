/*
 * Driver for BH1750 ambient light sensor.
 *
 */
#include "module.h"
#include "lauxlib.h"
#include "platform.h"

#define BH1750_ADDRESS 0x23

#define PLATFORM_BH1750_DEFAULT_SENSITIVITY  69
#define PLATFORM_BH1750_POWER_DOWN  0x00
#define PLATFORM_BH1750_MIN_SENSITIVITY  31
#define PLATFORM_BH1750_MAX_SENSITIVITY  254

typedef enum {
    PLATFORM_BH1750_CONTINUOUS_AUTO = 0,
    PLATFORM_BH1750_CONTINUOUS_HIGH_RES_MODE  = 0x10,
    PLATFORM_BH1750_CONTINUOUS_HIGH_RES_MODE_2 = 0x11,
    PLATFORM_BH1750_CONTINUOUS_LOW_RES_MODE = 0x13,
    PLATFORM_BH1750_ONE_TIME_HIGH_RES_MODE = 0x20,
    PLATFORM_BH1750_ONE_TIME_HIGH_RES_MODE_2 = 0x21,
    PLATFORM_BH1750_ONE_TIME_LOW_RES_MODE = 0x23,
} platform_bh1750_mode_t;

uint16_t platform_bh1750_sensitivity = PLATFORM_BH1750_DEFAULT_SENSITIVITY;
uint8_t platform_bh1750_mode = PLATFORM_BH1750_CONTINUOUS_AUTO;

static void send_command( uint32_t i2c_id, uint8_t command )
{
  platform_i2c_send_start(i2c_id);
  platform_i2c_send_address(i2c_id, BH1750_ADDRESS, PLATFORM_I2C_DIRECTION_TRANSMITTER);
  platform_i2c_send_byte(i2c_id, command);
  platform_i2c_send_stop(i2c_id);
}

static void set_sensitivity( uint32_t i2c_id, uint8_t sensitivity )
{
  send_command(i2c_id, (uint8_t) (0b01000 << 3) | (sensitivity >> 5));
  send_command(i2c_id, (uint8_t) (0b011 << 5 )  | (uint8_t) (sensitivity & 0b11111));

  if (platform_bh1750_mode == PLATFORM_BH1750_CONTINUOUS_HIGH_RES_MODE_2 ||
      platform_bh1750_mode == PLATFORM_BH1750_ONE_TIME_HIGH_RES_MODE_2 ||
      (platform_bh1750_mode == PLATFORM_BH1750_CONTINUOUS_AUTO && sensitivity == PLATFORM_BH1750_MAX_SENSITIVITY)) {
    platform_bh1750_sensitivity = (uint16_t) (((uint16_t) sensitivity) * 2);
  } else {
    platform_bh1750_sensitivity = sensitivity;
  }
}

static void platform_bh1750_setup( uint32_t i2c_id, platform_bh1750_mode_t mode, uint8_t sensitivity )
{
  platform_bh1750_mode = mode;

  if (mode == PLATFORM_BH1750_CONTINUOUS_AUTO) {
    set_sensitivity(i2c_id, PLATFORM_BH1750_MAX_SENSITIVITY);
    send_command(i2c_id, PLATFORM_BH1750_CONTINUOUS_HIGH_RES_MODE_2);
  } else {
    if (PLATFORM_BH1750_MIN_SENSITIVITY >= 31 && PLATFORM_BH1750_MAX_SENSITIVITY <= 254) {
      set_sensitivity(i2c_id, sensitivity);
    }
    send_command(i2c_id, mode);
  }
}

static void platform_bh1750_power_down(uint32_t i2c_id)
{
  send_command(i2c_id, PLATFORM_BH1750_POWER_DOWN);
}

static uint32_t platform_bh1750_read(uint32_t i2c_id)
{
  uint32_t value;
  platform_i2c_send_start(i2c_id);
  platform_i2c_send_address(i2c_id, BH1750_ADDRESS, PLATFORM_I2C_DIRECTION_RECEIVER);
  value = (uint16_t) platform_i2c_recv_byte(i2c_id, 1) << 8;
  value |= platform_i2c_recv_byte(i2c_id, 0);
  platform_i2c_send_stop(i2c_id);

  uint32_t lux_value = (uint32_t) (value * 57500 / platform_bh1750_sensitivity);

  if (platform_bh1750_mode == PLATFORM_BH1750_CONTINUOUS_AUTO) {
    if (lux_value < 7200000 && platform_bh1750_sensitivity == PLATFORM_BH1750_MIN_SENSITIVITY) {
      set_sensitivity(i2c_id, PLATFORM_BH1750_MAX_SENSITIVITY);
      send_command(i2c_id, PLATFORM_BH1750_CONTINUOUS_HIGH_RES_MODE_2);
    } else if (lux_value >= 7000000 && platform_bh1750_sensitivity == PLATFORM_BH1750_MAX_SENSITIVITY * 2) {
      set_sensitivity(i2c_id, PLATFORM_BH1750_MIN_SENSITIVITY);
      send_command(i2c_id, PLATFORM_BH1750_CONTINUOUS_HIGH_RES_MODE);
    }
  }

  return lux_value;
}

static int bh1750_setup(lua_State *L)
{
  uint32_t i2c_id = (uint32_t) luaL_optinteger( L, 1, 0 );
  platform_bh1750_mode_t mode = (platform_bh1750_mode_t) luaL_optinteger( L, 2, PLATFORM_BH1750_CONTINUOUS_AUTO );
  uint8_t sensitivity = (uint8_t) luaL_optinteger( L, 3, PLATFORM_BH1750_DEFAULT_SENSITIVITY );

  luaL_argcheck( L,
                 mode == PLATFORM_BH1750_CONTINUOUS_AUTO ||
                 mode == PLATFORM_BH1750_CONTINUOUS_HIGH_RES_MODE ||
                 mode == PLATFORM_BH1750_CONTINUOUS_HIGH_RES_MODE_2 ||
                 mode == PLATFORM_BH1750_CONTINUOUS_LOW_RES_MODE ||
                 mode == PLATFORM_BH1750_ONE_TIME_HIGH_RES_MODE ||
                 mode == PLATFORM_BH1750_ONE_TIME_HIGH_RES_MODE_2 ||
                 mode == PLATFORM_BH1750_ONE_TIME_LOW_RES_MODE, 1, "wrong mode" );

  luaL_argcheck( L, sensitivity >= PLATFORM_BH1750_MIN_SENSITIVITY && sensitivity <= PLATFORM_BH1750_MAX_SENSITIVITY, 2,
                 "invalid sensitivity" );

  platform_bh1750_setup(i2c_id, mode, sensitivity);

  return 0;
}

static int bh1750_power_down(lua_State *L)
{
  uint32_t i2c_id = (uint32_t) luaL_optinteger( L, 1, 0 );
  platform_bh1750_power_down(i2c_id);
  return 0;
}

static int bh1750_read(lua_State *L)
{
  uint32_t i2c_id = (uint32_t) luaL_optinteger( L, 1, 0 );
  lua_pushinteger(L, platform_bh1750_read(i2c_id));
  return 1;
}


static const LUA_REG_TYPE bh1750_map[] = {
    { LSTRKEY( "setup" ),                      LFUNCVAL( bh1750_setup ) },
    { LSTRKEY( "power_down" ),                 LFUNCVAL( bh1750_power_down ) },
    { LSTRKEY( "read" ),                       LFUNCVAL( bh1750_read ) },
    { LSTRKEY( "CONTINUOUS_AUTO" ),            LNUMVAL( PLATFORM_BH1750_CONTINUOUS_AUTO ) },
    { LSTRKEY( "CONTINUOUS_HIGH_RES_MODE" ),   LNUMVAL( PLATFORM_BH1750_CONTINUOUS_HIGH_RES_MODE ) },
    { LSTRKEY( "CONTINUOUS_HIGH_RES_MODE_2" ), LNUMVAL( PLATFORM_BH1750_CONTINUOUS_HIGH_RES_MODE_2 ) },
    { LSTRKEY( "CONTINUOUS_LOW_RES_MODE" ),    LNUMVAL( PLATFORM_BH1750_CONTINUOUS_LOW_RES_MODE ) },
    { LSTRKEY( "ONE_TIME_HIGH_RES_MODE" ),     LNUMVAL( PLATFORM_BH1750_ONE_TIME_HIGH_RES_MODE ) },
    { LSTRKEY( "ONE_TIME_HIGH_RES_MODE_2" ),   LNUMVAL( PLATFORM_BH1750_ONE_TIME_HIGH_RES_MODE_2 ) },
    { LSTRKEY( "ONE_TIME_LOW_RES_MODE" ),      LNUMVAL( PLATFORM_BH1750_ONE_TIME_LOW_RES_MODE ) },
    {LNILKEY,         LNILVAL}
};

NODEMCU_MODULE(BH1750, "bh1750", bh1750_map, NULL);