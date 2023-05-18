/******************************************************************************
 * Copyright 2013-2014 Espressif Systems (Wuxi)
 *
 * FileName: i2c_sw_master.c
 *
 * Description: i2c master API
 *
 * Modification history:
 *     2014/3/12, v1.0 create this file.
*******************************************************************************/
#include "driver/gpio.h"

#include "driver/i2c_sw_master.h"

static uint8_t m_nLastSDA;
static uint8_t m_nLastSCL;

static uint8_t pinSDA;
static uint8_t pinSCL;

/******************************************************************************
 * FunctionName : i2c_sw_master_setDC
 * Description  : Internal used function -
 *                    set i2c SDA and SCL bit value for half clk cycle
 * Parameters   : uint8_t SDA
 *                uint8_t SCL
 * Returns      : NONE
*******************************************************************************/
static void i2c_sw_master_setDC(uint8_t SDA, uint8_t SCL)
{
  uint8_t sclLevel;

  SDA &= 0x01;
  SCL &= 0x01;
  m_nLastSDA = SDA;
  m_nLastSCL = SCL;

  gpio_set_level(pinSDA, SDA);
  gpio_set_level(pinSCL, SCL);
  if(1 == SCL) {
    do {
      sclLevel = gpio_get_level(pinSCL);
    } while(sclLevel == 0);
  }
}

/******************************************************************************
 * FunctionName : i2c_sw_master_getDC
 * Description  : Internal used function -
 *                    get i2c SDA bit value
 * Parameters   : NONE
 * Returns      : uint8_t - SDA bit value
*******************************************************************************/
static uint8_t i2c_sw_master_getDC(void)
{
  return gpio_get_level(pinSDA);
}

/******************************************************************************
 * FunctionName : i2c_sw_master_init
 * Description  : initilize I2C bus to enable i2c operations
 * Parameters   : NONE
 * Returns      : NONE
*******************************************************************************/
void i2c_sw_master_init(void)
{
  uint8_t i;

  i2c_sw_master_setDC(1, 0);
  i2c_sw_master_wait(5);

  // when SCL = 0, toggle SDA to clear up
  i2c_sw_master_setDC(0, 0);
  i2c_sw_master_wait(5);
  i2c_sw_master_setDC(1, 0);
  i2c_sw_master_wait(5);

  // set data_cnt to max value
  for (i = 0; i < 28; i++) {
    i2c_sw_master_setDC(1, 0);
    i2c_sw_master_wait(5);	// sda 1, scl 0
    i2c_sw_master_setDC(1, 1);
    i2c_sw_master_wait(5);	// sda 1, scl 1
  }

  // reset all
  i2c_sw_master_stop();
  return;
}

uint8_t i2c_sw_master_get_pinSDA()
{
  return pinSDA;
}

uint8_t i2c_sw_master_get_pinSCL()
{
  return pinSCL;
}

/******************************************************************************
 * FunctionName : i2c_sw_master_gpio_init
 * Description  : config SDA and SCL gpio to open-drain output mode,
 *                mux and gpio num defined in i2c_sw_master.h
 * Parameters   : NONE
 * Returns      : NONE
*******************************************************************************/
void i2c_sw_master_gpio_init(uint8_t sda, uint8_t scl)
{
  pinSDA = sda;
  pinSCL = scl;

  gpio_config_t cfg;

  cfg.pin_bit_mask = 1ULL << sda | 1ULL << scl;
  cfg.mode = GPIO_MODE_INPUT_OUTPUT_OD;
  cfg.pull_up_en = GPIO_PULLUP_ENABLE;
  cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
  cfg.intr_type = GPIO_INTR_DISABLE;

  if (ESP_OK != gpio_config(&cfg))
    return;

  gpio_set_level(scl, 1);
  gpio_set_level(sda, 1);

  i2c_sw_master_init();
}

/******************************************************************************
 * FunctionName : i2c_sw_master_start
 * Description  : set i2c to send state
 * Parameters   : NONE
 * Returns      : NONE
*******************************************************************************/
void i2c_sw_master_start(void)
{
  i2c_sw_master_setDC(1, m_nLastSCL);
  i2c_sw_master_wait(5);
  i2c_sw_master_setDC(1, 1);
  i2c_sw_master_wait(5); // sda 1, scl 1
  i2c_sw_master_setDC(0, 1);
  i2c_sw_master_wait(5); // sda 0, scl 1
}

/******************************************************************************
 * FunctionName : i2c_sw_master_stop
 * Description  : set i2c to stop sending state
 * Parameters   : NONE
 * Returns      : NONE
*******************************************************************************/
void i2c_sw_master_stop(void)
{
  i2c_sw_master_wait(5);

  i2c_sw_master_setDC(0, m_nLastSCL);
  i2c_sw_master_wait(5); // sda 0
  i2c_sw_master_setDC(0, 1);
  i2c_sw_master_wait(5); // sda 0, scl 1
  i2c_sw_master_setDC(1, 1);
  i2c_sw_master_wait(5); // sda 1, scl 1
}

/******************************************************************************
 * FunctionName : i2c_sw_master_setAck
 * Description  : set ack to i2c bus as level value
 * Parameters   : uint8_t level - 0 or 1
 * Returns      : NONE
*******************************************************************************/
void i2c_sw_master_setAck(uint8_t level)
{
  i2c_sw_master_setDC(m_nLastSDA, 0);
  i2c_sw_master_wait(5);
  i2c_sw_master_setDC(level, 0);
  i2c_sw_master_wait(5); // sda level, scl 0
  i2c_sw_master_setDC(level, 1);
  i2c_sw_master_wait(8); // sda level, scl 1
  i2c_sw_master_setDC(level, 0);
  i2c_sw_master_wait(5); // sda level, scl 0
  i2c_sw_master_setDC(1, 0);
  i2c_sw_master_wait(5);
}

/******************************************************************************
 * FunctionName : i2c_sw_master_getAck
 * Description  : confirm if peer send ack
 * Parameters   : NONE
 * Returns      : uint8_t - ack value, 0 or 1
*******************************************************************************/
uint8_t i2c_sw_master_getAck(void)
{
  uint8_t retVal;
  i2c_sw_master_setDC(m_nLastSDA, 0);
  i2c_sw_master_wait(5);
  i2c_sw_master_setDC(1, 0);
  i2c_sw_master_wait(5);
  i2c_sw_master_setDC(1, 1);
  i2c_sw_master_wait(5);

  retVal = i2c_sw_master_getDC();
  i2c_sw_master_wait(5);
  i2c_sw_master_setDC(1, 0);
  i2c_sw_master_wait(5);

  return retVal;
}

/******************************************************************************
* FunctionName : i2c_sw_master_checkAck
* Description  : get dev response
* Parameters   : NONE
* Returns      : true : get ack ; false : get nack
*******************************************************************************/
bool i2c_sw_master_checkAck(void)
{
  if (i2c_sw_master_getAck()) {
    return false;
  } else {
    return true;
  }
}

/******************************************************************************
* FunctionName : i2c_sw_master_send_ack
* Description  : response ack
* Parameters   : NONE
* Returns      : NONE
*******************************************************************************/
void i2c_sw_master_send_ack(void)
{
  i2c_sw_master_setAck(0x0);
}

/******************************************************************************
* FunctionName : i2c_sw_master_send_nack
* Description  : response nack
* Parameters   : NONE
* Returns      : NONE
*******************************************************************************/
void i2c_sw_master_send_nack(void)
{
  i2c_sw_master_setAck(0x1);
}

/******************************************************************************
 * FunctionName : i2c_sw_master_readByte
 * Description  : read Byte from i2c bus
 * Parameters   : NONE
 * Returns      : uint8_t - readed value
*******************************************************************************/
uint8_t i2c_sw_master_readByte(void)
{
  uint8_t retVal = 0;
  uint8_t k, i;

  i2c_sw_master_wait(5);
  i2c_sw_master_setDC(m_nLastSDA, 0);
  i2c_sw_master_wait(5); // sda 1, scl 0

  for (i = 0; i < 8; i++) {
    i2c_sw_master_wait(5);
    i2c_sw_master_setDC(1, 0);
    i2c_sw_master_wait(5); // sda 1, scl 0
    i2c_sw_master_setDC(1, 1);
    i2c_sw_master_wait(5); // sda 1, scl 1

    k = i2c_sw_master_getDC();
    i2c_sw_master_wait(5);

    if (i == 7) {
      i2c_sw_master_wait(3);
    }

    k <<= (7 - i);
    retVal |= k;
  }

  i2c_sw_master_setDC(1, 0);
  i2c_sw_master_wait(5); // sda 1, scl 0

  return retVal;
}

/******************************************************************************
 * FunctionName : i2c_sw_master_writeByte
 * Description  : write wrdata value(one byte) into i2c
 * Parameters   : uint8_t wrdata - write value
 * Returns      : NONE
*******************************************************************************/
void i2c_sw_master_writeByte(uint8_t wrdata)
{
  uint8_t dat;
  int8_t i;

  i2c_sw_master_wait(5);

  i2c_sw_master_setDC(m_nLastSDA, 0);
  i2c_sw_master_wait(5);

  for (i = 7; i >= 0; i--) {
    dat = wrdata >> i;
    i2c_sw_master_setDC(dat, 0);
    i2c_sw_master_wait(5);
    i2c_sw_master_setDC(dat, 1);
    i2c_sw_master_wait(5);

    if (i == 0) {
      i2c_sw_master_wait(3);
    }

    i2c_sw_master_setDC(dat, 0);
    i2c_sw_master_wait(5);
  }
}
