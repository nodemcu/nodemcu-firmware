
#ifndef __PIN_MAP_H__
#define __PIN_MAP_H__

#include "c_types.h"
#include "user_config.h"
#include "gpio.h"

#define GPIO_PIN_NUM 13
#define GPIO_PIN_NUM_INV 17

extern uint32_t pin_mux[GPIO_PIN_NUM];
extern uint8_t  pin_num[GPIO_PIN_NUM];
extern uint8_t  pin_func[GPIO_PIN_NUM];
#ifdef GPIO_INTERRUPT_ENABLE
extern uint8_t  pin_num_inv[GPIO_PIN_NUM_INV];
extern uint8_t  pin_int_type[GPIO_PIN_NUM];
#endif

void get_pin_map(void);

#endif // #ifndef __PIN_MAP_H__
