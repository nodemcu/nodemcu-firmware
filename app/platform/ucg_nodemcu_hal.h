
#ifndef _UCG_NODEMCU_HAL_H
#define _UCG_NODEMCU_HAL_H

#include "ucg.h"


// extend standard ucg_t struct with info that's needed in the communication callbacks
typedef struct {
  ucg_t ucg;
  void *hal;
} ucg_nodemcu_t;


int16_t ucg_com_nodemcu_hw_spi(ucg_t *ucg, int16_t msg, uint16_t arg, uint8_t *data);

#endif  /* _UCG_NODEMCU_HAL_H */
