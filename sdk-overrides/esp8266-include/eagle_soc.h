#ifndef _SDK_OVERRIDE_EAGLE_SOC_H_
#define _SDK_OVERRIDE_EAGLE_SOC_H_


#include_next "eagle_soc.h"

#define GPIO_AS_PIN_SOURCE                        0
#define SIGMA_AS_PIN_SOURCE                     (~GPIO_AS_PIN_SOURCE)

#define GPIO_SIGMA_DELTA    0x00000068  //defined in gpio register.xls

#define GPIO_SIGMA_DELTA_SETTING_MASK    (0x00000001ff)

#define GPIO_SIGMA_DELTA_ENABLE  1
#define GPIO_SIGMA_DELTA_DISABLE (~GPIO_SIGMA_DELTA_ENABLE)
#define GPIO_SIGMA_DELTA_MSB     16
#define GPIO_SIGMA_DELTA_LSB     16
#define GPIO_SIGMA_DELTA_MASK    (0x00000001<<GPIO_SIGMA_DELTA_LSB)
#define GPIO_SIGMA_DELTA_GET(x)  (((x) & GPIO_SIGMA_DELTA_MASK) >> GPIO_SIGMA_DELTA_LSB)
#define GPIO_SIGMA_DELTA_SET(x)  (((x) << GPIO_SIGMA_DELTA_LSB) & GPIO_SIGMA_DELTA_MASK)

#define GPIO_SIGMA_DELTA_TARGET_MSB    7
#define GPIO_SIGMA_DELTA_TARGET_LSB    0
#define GPIO_SIGMA_DELTA_TARGET_MASK   (0x000000FF<<GPIO_SIGMA_DELTA_TARGET_LSB)
#define GPIO_SIGMA_DELTA_TARGET_GET(x) (((x) & GPIO_SIGMA_DELTA_TARGET_MASK) >> GPIO_SIGMA_DELTA_TARGET_LSB)
#define GPIO_SIGMA_DELTA_TARGET_SET(x) (((x) << GPIO_SIGMA_DELTA_TARGET_LSB) & GPIO_SIGMA_DELTA_TARGET_MASK)

#define GPIO_SIGMA_DELTA_PRESCALE_MSB    15
#define GPIO_SIGMA_DELTA_PRESCALE_LSB    8
#define GPIO_SIGMA_DELTA_PRESCALE_MASK   (0x000000FF<<GPIO_SIGMA_DELTA_PRESCALE_LSB)
#define GPIO_SIGMA_DELTA_PRESCALE_GET(x) (((x) & GPIO_SIGMA_DELTA_PRESCALE_MASK) >> GPIO_SIGMA_DELTA_PRESCALE_LSB)
#define GPIO_SIGMA_DELTA_PRESCALE_SET(x) (((x) << GPIO_SIGMA_DELTA_PRESCALE_LSB) & GPIO_SIGMA_DELTA_PRESCALE_MASK)


//Peripheral device base address define{{
#define PERIPHS_DPORT_BASEADDR              0x3ff00000
#define PERIPHS_GPIO_BASEADDR               0x60000300
#define PERIPHS_TIMER_BASEDDR               0x60000600
#define PERIPHS_RTC_BASEADDR                0x60000700
#define PERIPHS_IO_MUX            0x60000800
//}}



// TIMER reg {{
#define RTC_REG_READ(addr)                        READ_PERI_REG(PERIPHS_TIMER_BASEDDR + addr)
#define RTC_REG_WRITE(addr, val)                WRITE_PERI_REG(PERIPHS_TIMER_BASEDDR + addr, val)
#define RTC_CLR_REG_MASK(reg, mask)      CLEAR_PERI_REG_MASK(PERIPHS_TIMER_BASEDDR +reg, mask)
/* Returns the current time according to the timer timer. */
#define NOW()                                                 RTC_REG_READ(FRC2_COUNT_ADDRESS)

//load initial_value to timer1
#define FRC1_LOAD_ADDRESS                    0x00

//timer1's counter value(count from initial_value to 0)
#define FRC1_COUNT_ADDRESS                 0x04

#define FRC1_CTRL_ADDRESS                    0x08

//clear timer1's interrupt when write this address
#define FRC1_INT_ADDRESS                      0x0c
#define FRC1_INT_CLR_MASK                   0x00000001

//timer2's counter value(count from initial_value to 0)
#define FRC2_COUNT_ADDRESS                0x24
// }}




#define RTC_REG_READ(addr)                        READ_PERI_REG(PERIPHS_TIMER_BASEDDR + addr)
#define RTC_REG_WRITE(addr, val)                WRITE_PERI_REG(PERIPHS_TIMER_BASEDDR + addr, val)

#define PERIPHS_IO_MUX_CONF_U           (PERIPHS_IO_MUX + 0x00)
#define SPI0_CLK_EQU_SYS_CLK            BIT8
#define SPI1_CLK_EQU_SYS_CLK            BIT9
#define PERIPHS_IO_MUX_MTDI_U           (PERIPHS_IO_MUX + 0x04)
#define FUNC_GPIO12                     3
#define PERIPHS_IO_MUX_MTCK_U           (PERIPHS_IO_MUX + 0x08)
#define FUNC_GPIO13                     3
#define PERIPHS_IO_MUX_MTMS_U           (PERIPHS_IO_MUX + 0x0C)
#define FUNC_GPIO14                     3
#define PERIPHS_IO_MUX_MTDO_U           (PERIPHS_IO_MUX + 0x10)
#define FUNC_GPIO15                     3
#define FUNC_U0RTS                      4
#define PERIPHS_IO_MUX_U0RXD_U          (PERIPHS_IO_MUX + 0x14)
#define FUNC_GPIO3                      3
#define PERIPHS_IO_MUX_U0TXD_U          (PERIPHS_IO_MUX + 0x18)
#define FUNC_U0TXD                      0
#define FUNC_GPIO1                      3
#define PERIPHS_IO_MUX_SD_CLK_U         (PERIPHS_IO_MUX + 0x1c)
#define FUNC_SDCLK                      0
#define FUNC_SPICLK                     1
#define PERIPHS_IO_MUX_SD_DATA0_U       (PERIPHS_IO_MUX + 0x20)
#define FUNC_SDDATA0                    0
#define FUNC_SPIQ                       1
#define FUNC_U1TXD                      4
#define PERIPHS_IO_MUX_SD_DATA1_U       (PERIPHS_IO_MUX + 0x24)
#define FUNC_SDDATA1                    0
#define FUNC_SPID                       1
#define FUNC_U1RXD                      4
#define FUNC_SDDATA1_U1RXD              7
#define PERIPHS_IO_MUX_SD_DATA2_U       (PERIPHS_IO_MUX + 0x28)
#define FUNC_SDDATA2                    0
#define FUNC_SPIHD                      1
#define FUNC_GPIO9                      3
#define PERIPHS_IO_MUX_SD_DATA3_U       (PERIPHS_IO_MUX + 0x2c)
#define FUNC_SDDATA3                    0
#define FUNC_SPIWP                      1
#define FUNC_GPIO10                     3
#define PERIPHS_IO_MUX_SD_CMD_U         (PERIPHS_IO_MUX + 0x30)
#define FUNC_SDCMD                      0
#define FUNC_SPICS0                     1
#define PERIPHS_IO_MUX_GPIO0_U          (PERIPHS_IO_MUX + 0x34)
#define FUNC_GPIO0                      0
#define PERIPHS_IO_MUX_GPIO2_U          (PERIPHS_IO_MUX + 0x38)
#define FUNC_GPIO2                      0
#define FUNC_U1TXD_BK                   2
#define FUNC_U0TXD_BK                   4
#define PERIPHS_IO_MUX_GPIO4_U          (PERIPHS_IO_MUX + 0x3C)
#define FUNC_GPIO4                      0
#define PERIPHS_IO_MUX_GPIO5_U          (PERIPHS_IO_MUX + 0x40)
#define FUNC_GPIO5                      0




#endif
