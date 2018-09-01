#ifndef __I2C_MASTER_H__
#define __I2C_MASTER_H__

#include "user_config.h"

#ifdef I2C_MASTER_OLD_VERSION

#define I2C_MASTER_SDA_MUX (pin_mux[sda])
#define I2C_MASTER_SCL_MUX (pin_mux[scl])
#define I2C_MASTER_SDA_GPIO (i2c[id].pinSDA)
#define I2C_MASTER_SCL_GPIO (i2c[id].pinSCL)
#define I2C_MASTER_SDA_FUNC (pin_func[sda])
#define I2C_MASTER_SCL_FUNC (pin_func[scl])

#define I2C_MASTER_SDA_HIGH_SCL_HIGH()  \
    gpio_output_set(1<<I2C_MASTER_SDA_GPIO | 1<<I2C_MASTER_SCL_GPIO, 0, 1<<I2C_MASTER_SDA_GPIO | 1<<I2C_MASTER_SCL_GPIO, 0)

#define I2C_MASTER_SDA_HIGH_SCL_LOW()  \
    gpio_output_set(1<<I2C_MASTER_SDA_GPIO, 1<<I2C_MASTER_SCL_GPIO, 1<<I2C_MASTER_SDA_GPIO | 1<<I2C_MASTER_SCL_GPIO, 0)

#define I2C_MASTER_SDA_LOW_SCL_HIGH()  \
    gpio_output_set(1<<I2C_MASTER_SCL_GPIO, 1<<I2C_MASTER_SDA_GPIO, 1<<I2C_MASTER_SDA_GPIO | 1<<I2C_MASTER_SCL_GPIO, 0)

#define I2C_MASTER_SDA_LOW_SCL_LOW()  \
    gpio_output_set(0, 1<<I2C_MASTER_SDA_GPIO | 1<<I2C_MASTER_SCL_GPIO, 1<<I2C_MASTER_SDA_GPIO | 1<<I2C_MASTER_SCL_GPIO, 0)

#define i2c_master_wait    os_delay_us

#endif //I2C_MASTER_OLD_VERSION

typedef struct {
    uint8 m_nLastSDA;
    uint8 m_nLastSCL;
    uint8 pinSDA;
    uint8 pinSCL;
    uint32 pin_SDA_SCL_mask;
    uint32 pin_SDA_mask;
    uint32 pin_SCL_mask;
    uint32 speed;
    int cycles_delay;
} i2c_master_state;

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

#endif //__I2C_MASTER_H__
