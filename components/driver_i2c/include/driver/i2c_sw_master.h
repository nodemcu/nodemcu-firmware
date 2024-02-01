#ifndef __I2C_SW_MASTER_H__
#define __I2C_SW_MASTER_H__

#include "esp_rom_sys.h"

#define I2C_NUM_MAX 1

void i2c_sw_master_gpio_init(uint8_t sda, uint8_t scl);
void i2c_sw_master_init(void);

#define i2c_sw_master_wait esp_rom_delay_us
void i2c_sw_master_stop(void);
void i2c_sw_master_start(void);
void i2c_sw_master_setAck(uint8_t level);
uint8_t i2c_sw_master_getAck(void);
uint8_t i2c_sw_master_readByte(void);
void i2c_sw_master_writeByte(uint8_t wrdata);

bool i2c_sw_master_checkAck(void);
void i2c_sw_master_send_ack(void);
void i2c_sw_master_send_nack(void);

uint8_t i2c_sw_master_get_pinSDA();
uint8_t i2c_sw_master_get_pinSCL();

#endif
