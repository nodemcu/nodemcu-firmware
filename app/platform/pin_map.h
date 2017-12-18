
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
typedef struct {
  // These values have 15 bits of count, and the top bit
  // in 'seen' is set if we are missing a task post
  volatile uint16_t seen;
  volatile uint16_t reported;
} GPIO_INT_COUNTER;
extern GPIO_INT_COUNTER pin_counter[GPIO_PIN_NUM];
#endif

void get_pin_map(void);

#endif // #ifndef __PIN_MAP_H__
