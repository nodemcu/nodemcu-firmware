#ifndef SDK_OVERRIDES_INCLUDE_ETS_SYS_H_
#define SDK_OVERRIDES_INCLUDE_ETS_SYS_H_

#include "c_types.h"
#include "os_type.h"
#include "espressif/esp_timer.h"

#include "freertos/portmacro.h"
#include_next "ets_sys.h"

#define ets_vsprintf vsprintf

#define ETS_FRC_TIMER1_INTR_ATTACH(fn,arg) _xt_isr_attach(ETS_FRC_TIMER1_INUM, fn, arg)
//#define ETS_FRC_TIMER1_NMI_INTR_ATTACH(fn) NmiTimSetFunc(fn)

#define ETS_GPIO_INTR_ATTACH(fn,arg) _xt_isr_attach(ETS_GPIO_INUM, fn, arg)
#define ETS_UART_INTR_ATTACH(fn,arg) _xt_isr_attach(ETS_UART_INUM, fn, arg)
#define ETS_SPI_INTR_ATTACH(fn,arg) _xt_isr_attach(ETS_SPI_INUM, fn, arg)

#define ETS_UART_INTR_ENABLE()  _xt_isr_unmask(1 << ETS_UART_INUM)
#define ETS_UART_INTR_DISABLE() _xt_isr_mask(1 << ETS_UART_INUM)
#define ETS_GPIO_INTR_ENABLE()  _xt_isr_unmask(1 << ETS_GPIO_INUM)
#define ETS_GPIO_INTR_DISABLE() _xt_isr_mask(1 << ETS_GPIO_INUM)
#define ETS_SPI_INTR_ENABLE()  _xt_isr_unmask(1 << ETS_SPI_INUM)
#define ETS_SPI_INTR_DISABLE() _xt_isr_mask(1 << ETS_SPI_INUM)
#define ETS_FRC1_INTR_ENABLE() _xt_isr_unmask(1 << ETS_FRC_TIMER1_INUM)
#define ETS_FRC1_INTR_DISABLE() _xt_isr_mask(1 << ETS_FRC_TIMER1_INUM)

#endif /* SDK_OVERRIDES_INCLUDE_ETS_SYS_H_ */
