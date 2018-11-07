#include "c_stdint.h"
#include "c_math.h"

#define MPU6050

uint16_t i2c_write(uint16_t ads_addr, uint8_t reg, uint8_t length, uint8_t *data);
    void i2c_read(uint16_t ads_addr, uint8_t reg, uint8_t length, uint8_t *data);
uint16_t IICwriteBit(uint8_t slave_addr, uint8_t reg_addr, uint8_t bitNum, uint8_t data);
uint16_t IICwriteBits(uint8_t slave_addr, uint8_t reg_addr, uint8_t bitStart, uint8_t length, uint8_t data);

