#include "I2C.h"
#include "mpu6050.h"

#include "../platform/platform.h"
#include "mpu6050.h"

uint16_t i2c_write(uint16_t ads_addr, uint8_t reg, uint8_t length, uint8_t *data) {
    MPU6050_DEBUG("i2c_write:  ads_addr(%04x), reg(%04x), length(%d)\n",ads_addr, reg, length);
    uint8_t n;
    for (n = 0; n < length; n++) {
        platform_i2c_send_start(0);
        platform_i2c_send_address(0, ads_addr, PLATFORM_I2C_DIRECTION_TRANSMITTER);
        platform_i2c_send_byte(0, reg);
        int i;
        for (i = 0; i < length; i++) {
            if (platform_i2c_send_byte(0, data[i]) == 0)
                break;
        }
        platform_i2c_send_stop(0);
    }
    return 0;
}

void i2c_read(uint16_t ads_addr, uint8_t reg, uint8_t length, uint8_t *data) {
    MPU6050_DEBUG("i2c_read:  ads_addr(%04x), reg(%04x), length(%d)\n",ads_addr, reg, length);
    platform_i2c_send_start(0);
    platform_i2c_send_address(0, ads_addr, PLATFORM_I2C_DIRECTION_TRANSMITTER);
    platform_i2c_send_byte(0, reg);
    platform_i2c_send_stop(0);
    platform_i2c_send_start(0);
    platform_i2c_send_address(0, ads_addr, PLATFORM_I2C_DIRECTION_RECEIVER);
    int i;
    for (i = 0; i < length; i++) {
        if ((data[i] = platform_i2c_recv_byte(0, i < length - 1)) == -1)
            break;
    }
    platform_i2c_send_stop(0);
}

uint16_t IICwriteBit(uint8_t slave_addr, uint8_t reg_addr, uint8_t bitNum, uint8_t data) {
  uint8_t tmp;
  i2c_read(slave_addr, reg_addr, 1, &tmp);
  tmp = (data != 0) ? (tmp | (1 << bitNum)) : (tmp & ~(1 << bitNum));
  return i2c_write(slave_addr, reg_addr, 1, &tmp);
}
;

uint16_t IICwriteBits(uint8_t slave_addr, uint8_t reg_addr, uint8_t bitStart, uint8_t length, uint8_t data) {
    uint8_t tmp, dataShift;
    i2c_read(slave_addr, reg_addr, 1, &tmp);
    uint8_t mask = (((1 << length) - 1) << (bitStart - length + 1));
    dataShift = data << (bitStart - length + 1);
    tmp &= mask;
    tmp |= dataShift;

    return i2c_write(slave_addr, reg_addr, 1, &tmp);
}

