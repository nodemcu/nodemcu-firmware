#ifndef __I2C_MASTER_H__
#define __I2C_MASTER_H__

uint32 i2c_master_gpio_init(uint16 id, uint8 sda, uint8 scl, uint32 speed);
void i2c_master_init(uint16 id);
bool i2c_master_configured(uint16 id);
void i2c_master_stop(uint16 id);
void i2c_master_start(uint16 id);
void i2c_master_setAck(uint16 id, uint8 level);
uint8 i2c_master_getAck(uint16 id);
uint8 i2c_master_readByte(uint16 id);
void i2c_master_writeByte(uint16 id, uint8 wrdata);

bool i2c_master_checkAck(uint16 id);
void i2c_master_send_ack(uint16 id);
void i2c_master_send_nack(uint16 id);

uint8 i2c_master_get_pinSDA(uint16 id);
uint8 i2c_master_get_pinSCL(uint16 id);
uint32 i2c_master_get_speed(uint16 id);

#endif //__I2C_MASTER_H__
