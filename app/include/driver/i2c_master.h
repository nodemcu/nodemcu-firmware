#ifndef __I2C_MASTER_H__
#define __I2C_MASTER_H__

uint32 i2c_master_setup(uint16 id, uint8 sda, uint8 scl, uint32 speed);
void i2c_master_init(uint16 id);
bool i2c_master_configured(uint16 id);
void i2c_master_stop(uint16 id);
void i2c_master_start(uint16 id);
uint8 i2c_master_readByte(uint16 id, sint16 ack);
uint8 i2c_master_writeByte(uint16 id, uint8 wrdata);

#endif //__I2C_MASTER_H__
