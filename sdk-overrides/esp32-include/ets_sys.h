#ifndef _SDK_OVERRIDES_ETS_SYS_H_
#define _SDK_OVERRIDES_ETS_SYS_H_

#include_next <rom/ets_sys.h>

#include <freertos/xtensa_api.h>
#define ETS_UART_INTR_ATTACH(fn,arg) xt_set_interrupt_handler(ETS_UART_INUM, fn, arg)
#define ETS_UART_INTR_ENABLE()  xt_ints_on(1 << ETS_UART_INUM)
#define ETS_UART_INTR_DISABLE() xt_ints_off(1 << ETS_UART_INUM)

#define ETS_SPI_INTR_ATTACH(fn,arg) xt_set_interrupt_handler(ETS_SPI_INUM, fn, arg)
#define ETS_SPI_INTR_ENABLE()  xt_ints_on(1 << ETS_SPI_INUM)
#define ETS_SPI_INTR_DISABLE() xt_ints_off(1 << ETS_SPI_INUM)

#define ETS_GPIO_INTR_ATTACH(fn,arg) xt_set_interrupt_handler(ETS_GPIO_INUM, fn, arg)
#define ETS_GPIO_INTR_ENABLE()  xt_ints_on(1 << ETS_GPIO_INUM)
#define ETS_GPIO_INTR_DISABLE() xt_ints_off(1 << ETS_GPIO_INUM)

#define ETS_FRC_TIMER1_INTR_ATTACH(fn,arg) xt_set_interrupt_handler(ETS_FRC_TIMER1_INUM, fn, arg)
#define ETS_FRC1_INTR_ENABLE()  xt_ints_on(1 << ETS_FRC_TIMER1_INUM)
#define ETS_FRC1_INTR_DISABLE() xt_ints_off(1 << ETS_FRC_TIMER1_INUM)

// FIXME: double-check this is the correct bit!
#define TM1_EDGE_INT_ENABLE()       SET_PERI_REG_MASK(EDGE_INT_ENABLE_REG, BIT1)
#define TM1_EDGE_INT_DISABLE()      CLEAR_PERI_REG_MASK(EDGE_INT_ENABLE_REG, BIT1)

#endif
