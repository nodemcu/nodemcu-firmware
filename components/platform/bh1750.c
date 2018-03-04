/*
 * Driver for BH1750FVI ambient light sensor.
 *
 */
#include "platform.h"

#define BH1750_ADDRESS 0x23

uint16_t platform_bh1750_sensitivity = PLATFORM_BH1750_DEFAULT_SENSITIVITY;
uint8_t platform_bh1750_mode = PLATFORM_BH1750_CONTINUOUS_AUTO;

static void send_command( uint32_t i2c_id, uint8_t command )
{
  platform_i2c_send_start(i2c_id);
  platform_i2c_send_address(i2c_id, BH1750_ADDRESS, PLATFORM_I2C_DIRECTION_TRANSMITTER, 0);
  platform_i2c_send_byte(i2c_id, command, 0);
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

int8_t platform_bh1750_setup( uint32_t i2c_id, platform_bh1750_mode_t mode, uint8_t sensitivity )
{
  platform_bh1750_mode = mode;

  if (platform_i2c_exists(i2c_id) == PLATFORM_ERR)
    return PLATFORM_ERR;

  if (mode == PLATFORM_BH1750_CONTINUOUS_AUTO) {
    set_sensitivity(i2c_id, PLATFORM_BH1750_MAX_SENSITIVITY);
    send_command(i2c_id, PLATFORM_BH1750_CONTINUOUS_HIGH_RES_MODE_2);
  } else {
    if (PLATFORM_BH1750_MIN_SENSITIVITY >= 31 && PLATFORM_BH1750_MAX_SENSITIVITY <= 254) {
      set_sensitivity(i2c_id, sensitivity);
    }
    send_command(i2c_id, mode);
  }

  return PLATFORM_OK;
}

int8_t platform_bh1750_power_down(uint32_t i2c_id)
{
  if (platform_i2c_exists(i2c_id) == PLATFORM_ERR)
    return PLATFORM_ERR;

  send_command(i2c_id, PLATFORM_BH1750_POWER_DOWN);

  return PLATFORM_OK;
}

uint32_t platform_bh1750_read(uint32_t i2c_id)
{
  uint32_t value;
  platform_i2c_send_start(i2c_id);
  platform_i2c_send_address(i2c_id, BH1750_ADDRESS, PLATFORM_I2C_DIRECTION_RECEIVER, 0);
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