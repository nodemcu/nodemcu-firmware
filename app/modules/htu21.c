/*
 * Driver for HTU21D/SHT21 humidity/temperature sensor.
 *
 */
#include "module.h"
#include "lauxlib.h"
#include "platform.h"

#define HTU21_ADDRESS           0x40
/* HTU21 Commands */
#define HTU21_T_MEASUREMENT_HM    0xE3
#define HTU21_RH_MEASUREMENT_HM    0xE5
#define HTU21_CRC_ERROR (uint16_t) 1

//Give this function the 2 byte message (measurement) and the check_value byte from the HTU21D
//If it returns 0, then the transmission was good
//If it returns something other than 0, then the communication was corrupted
//From: http://www.nongnu.org/avr-libc/user-manual/group__util__crc.html
//POLYNOMIAL = 0x0131 = x^8 + x^5 + x^4 + 1 : http://en.wikipedia.org/wiki/Computation_of_cyclic_redundancy_checks
#define SHIFTED_DIVISOR 0x988000 //This is the 0x0131 polynomial shifted to farthest left of three bytes

static inline int32_t htu21_temp_ticks_to_millicelsius(uint32_t ticks)
{
  ticks &= ~0x0003; /* clear status bits */
  /*
   * Formula T = -46.85 + 175.72 * ST / 2^16 from datasheet p14,
   * optimized for integer fixed point (3 digits) arithmetic
   */
  return ((21965 * ticks) >> 13) - 46850;
}

static inline int32_t htu21_rh_ticks_to_per_cent_mille(uint32_t ticks)
{
  ticks &= ~0x0003; /* clear status bits */
  /*
   * Formula RH = -6 + 125 * SRH / 2^16 from datasheet p14,
   * optimized for integer fixed point (3 digits) arithmetic
   */
  return ((15625 * ticks) >> 13) - 6000;
}

static uint8_t checkCRC(uint16_t ravValue, uint8_t checksum)
{
  uint32_t remainder = (uint32_t) ravValue << 8; //Pad with 8 bits because we have to add in the check value
  remainder |= checksum; //Add on the check value

  uint32_t divsor = (uint32_t) SHIFTED_DIVISOR;

  //Operate on only 16 positions of max 24. The remaining 8 are our remainder and should be zero when we're done.
  for (int i = 0; i < 16; i++)
  {
    if (remainder & (uint32_t) 1 << (23 - i)) //Check if there is a one in the left position
      remainder ^= divsor;

    divsor >>= 1; //Rotate the divsor max 16 times so that we have 8 bits left of a remainder
  }

  return (uint8_t) remainder;
}

static uint16_t read_with_crc_check(uint32_t i2c_id, uint8_t reg)
{
  uint16_t rawValue;
  uint8_t checksum;

  platform_i2c_send_start(i2c_id);
  platform_i2c_send_address(i2c_id, HTU21_ADDRESS, PLATFORM_I2C_DIRECTION_TRANSMITTER);
  platform_i2c_send_byte(i2c_id, reg);
  platform_i2c_send_start(i2c_id);
  platform_i2c_send_address(i2c_id, HTU21_ADDRESS, PLATFORM_I2C_DIRECTION_RECEIVER);

  rawValue = (uint16_t) platform_i2c_recv_byte(i2c_id, 1) << 8;
  rawValue |= platform_i2c_recv_byte(i2c_id, 1);
  checksum = (uint8_t) platform_i2c_recv_byte(i2c_id, 0);

  platform_i2c_send_stop(i2c_id);

  return checkCRC(rawValue, checksum) != 0 ? HTU21_CRC_ERROR : rawValue;
}

static int htu21_read(lua_State *L)
{
  uint32_t i2c_id = (uint32_t) luaL_optinteger( L, 1, 0 );

  uint16_t rawT = read_with_crc_check(i2c_id, HTU21_T_MEASUREMENT_HM);
  uint16_t rawRH = read_with_crc_check(i2c_id, HTU21_RH_MEASUREMENT_HM);

  if (rawT == HTU21_CRC_ERROR || rawRH == HTU21_CRC_ERROR) {
    luaL_error(L, "htu21 invalid CRC");
  }

  lua_pushinteger(L, htu21_temp_ticks_to_millicelsius(rawT));
  lua_pushinteger(L, htu21_rh_ticks_to_per_cent_mille(rawRH));

  return 2;
}

static const LUA_REG_TYPE htu21_map[] = {
    {LSTRKEY("read"), LFUNCVAL(htu21_read)},
    {LNILKEY, LNILVAL}
};

NODEMCU_MODULE(HTU21, "htu21", htu21_map, NULL);
