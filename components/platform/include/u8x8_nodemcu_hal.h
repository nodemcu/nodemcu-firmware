
#ifndef _U8X8_NODEMCU_HAL_H
#define _U8X8_NODEMCU_HAL_H

#include "u8g2.h"


// extend standard u8g2_t struct with info that's needed in the communication callbacks
typedef struct {
  u8g2_t u8g2;
  void *hal;
} u8g2_nodemcu_t;


uint8_t u8x8_gpio_and_delay_nodemcu(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr);
uint8_t u8x8_byte_nodemcu_i2c(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr);
uint8_t u8x8_byte_nodemcu_spi(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr);

#endif  /* _U8X8_NODEMCU_HAL_H */
