/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2016 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>
 *
 * Permission is hereby granted for use on ESPRESSIF SYSTEMS ESP8266 only, in which case,
 * it is free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 *
 * Rework of original driver: Natalia Sorokina <sonaux@gmail.com>, 2018
 */

#include "../libc/c_stdlib.h"
#include "ets_sys.h"
#include "osapi.h"
#include "gpio.h"
#include "user_interface.h"

#include "cpu_esp8266.h"
#include "pin_map.h"

#include "user_config.h"

#include "driver/i2c_master.h"


#ifndef I2C_MASTER_OLD_VERSION
/******************************************************************************
* NEW driver
* Enabled if I2C_MASTER_OLD_VERSION is not defined in user_config.h
*******************************************************************************/

// enable use GPIO16 (D0) pin as SCL line. ~450 kHz maximum frequency
#ifdef I2C_MASTER_GPIO16_ENABLED
#define IS_PIN16(n) ((n)==16)
#define CPU_CYCLES_BETWEEN_DELAYS 80
#define CPU_CYCLES_GPIO16 90
#else
#define IS_PIN16(n) (0) //removes GPIO16-related code during compile
#define CPU_CYCLES_BETWEEN_DELAYS 74
#endif //I2C_MASTER_GPIO16_ENABLED

#define MIN_SPEED 25000
#define MAX_NUMBER_OF_I2C NUM_I2C

typedef struct {
    uint8 last_SDA;
    uint8 last_SCL;
    uint8 pin_SDA;
    uint8 pin_SCL;
    uint32 pin_SDA_SCL_mask;
    uint32 pin_SDA_mask;
    uint32 pin_SCL_mask;
    uint32 speed;
    sint16 cycles_delay;
} i2c_master_state_t;
static i2c_master_state_t *i2c[MAX_NUMBER_OF_I2C];

/******************************************************************************
 * FunctionName : i2c_master_set_DC_delay
 * Description  : Internal used function - calculate delay for i2c_master_setDC
 * Parameters   : bus id
 * Returns      : NONE
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
i2c_master_set_DC_delay(uint16 id)
{
  // [cpu cycles per half SCL clock period] - [cpu cycles that code takes to run]
  i2c[id]->cycles_delay = system_get_cpu_freq() * 500000 / i2c[id]->speed - CPU_CYCLES_BETWEEN_DELAYS;
  #ifdef I2C_MASTER_GPIO16_ENABLED
  if(IS_PIN16(i2c[id]->pin_SCL)){ //if GPIO16
      i2c[id]->cycles_delay -= CPU_CYCLES_GPIO16; //decrease delay
  }
  #endif //I2C_MASTER_GPIO16_ENABLED
  if(i2c[id]->cycles_delay < 0){
      i2c[id]->cycles_delay = 0;
  }
}
/******************************************************************************
 * FunctionName : i2c_master_wait_cpu_cycles
 * Description  : Internal used function - wait for given count of cpu cycles
 * Parameters   : sint16 cycles_delay
 * Returns      : NONE
*******************************************************************************/
static inline void i2c_master_wait_cpu_cycles(sint16 cycles_delay)
{
    uint32 cycles_start;
    uint32 cycles_curr;
    // uses special 'ccount' register which is increased every CPU cycle
    // to make precise delay
    asm volatile("rsr %0, ccount":"=a"(cycles_start));
    do{
        asm volatile("rsr %0, ccount":"=a"(cycles_curr));
    } while (cycles_curr - cycles_start <  cycles_delay);
}

/******************************************************************************
 * FunctionName : i2c_master_wait_gpio_SCL_high
 * Description  : Internal used function - wait until SCL line in a high state
                  (slave device may hold SCL line low until it is ready to proceed)
 * Parameters   : bus id
 * Returns      : NONE
*******************************************************************************/
static inline void i2c_master_wait_gpio_SCL_high(uint16 id)
{
    // retrieves bitmask of all GPIOs from memory-mapped gpio register and exits if SCL bit is set
    // equivalent, but slow variant:
    // while(!(READ_PERI_REG(PERIPHS_GPIO_BASEADDR + GPIO_IN_ADDRESS) & i2c[id]->pin_SCL_mask)) {};
    // even slower: while (!(gpio_input_get() & i2c[id]->pin_SCL_mask)) {};
    asm volatile("l_wait:"
                 "l16ui %0, %[gpio_in_addr], 0;" //read gpio state into register %0
                 "memw;"                         //wait for read completion
                 "bnall %0, %[gpio_SCL_mask], l_wait;" // test if SCL bit not set
                 ::[gpio_SCL_mask] "r" (i2c[id]->pin_SCL_mask),
                 [gpio_in_addr] "r" (PERIPHS_GPIO_BASEADDR + GPIO_IN_ADDRESS)
    );
}

/******************************************************************************
 * FunctionName : i2c_master_setDC
 * Description  : Internal used function -
 *                    set i2c SDA and SCL bit value for half clock cycle
 * Parameters   : bus id, uint8 SDA, uint8 SCL
 * Returns      : NONE
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
i2c_master_setDC(uint16 id, uint8 SDA, uint8 SCL)
{
    uint32 this_SDA_SCL_set_mask;
    uint32 this_SDA_SCL_clear_mask;
    i2c[id]->last_SDA = SDA;
    i2c[id]->last_SCL = SCL;

    if(i2c[id]->cycles_delay > 0){
        i2c_master_wait_cpu_cycles(i2c[id]->cycles_delay);
    }
    if (IS_PIN16(i2c[id]->pin_SCL)){ //GPIO16 wired differently, it has it's own register address
        WRITE_PERI_REG(RTC_GPIO_OUT, SCL); // write SCL value
        if(1 == SDA){
            GPIO_REG_WRITE(GPIO_OUT_W1TS_ADDRESS, i2c[id]->pin_SDA_mask); //SDA = 1
        }else{
            GPIO_REG_WRITE(GPIO_OUT_W1TC_ADDRESS, i2c[id]->pin_SDA_mask); // SDA = 0
        }
        if(1 == SCL){ //clock stretching, GPIO16 version
            while(!(READ_PERI_REG(RTC_GPIO_IN_DATA) & 1)) {}; //read SCL value until SCL goes high
        }else{
            // dummy read operation and empty CPU cycles to maintain equal times for low and high state
            READ_PERI_REG(RTC_GPIO_IN_DATA) & 1; asm volatile("nop;nop;nop;nop;");
        }
    }
    else{
        this_SDA_SCL_set_mask = (SDA << i2c[id]->pin_SDA) | (SCL << i2c[id]->pin_SCL);
        this_SDA_SCL_clear_mask = i2c[id]->pin_SDA_SCL_mask ^ this_SDA_SCL_set_mask;
        GPIO_REG_WRITE(GPIO_OUT_W1TC_ADDRESS, this_SDA_SCL_clear_mask);
        GPIO_REG_WRITE(GPIO_OUT_W1TS_ADDRESS, this_SDA_SCL_set_mask);
        if(1 == SCL) { //clock stretching
            i2c_master_wait_gpio_SCL_high(id);
        }else{
            asm volatile("nop;nop;nop;"); // empty CPU cycles to maintain equal times for low and high state
        }
    }
}

/******************************************************************************
 * FunctionName : i2c_master_getDC
 * Description  : Internal used function -
 *                    get i2c SDA bit value
 * Parameters   : bus id
 * Returns      : uint8 - SDA bit value
*******************************************************************************/
static inline uint8 ICACHE_FLASH_ATTR
i2c_master_getDC(uint16 id)
{
    uint8 sda_out;
    uint32 gpio_in_mask;
    //instead of: gpio_in_mask = gpio_input_get();
    asm volatile("l16ui %[gpio_in_mask], %[gpio_in_addr], 0;" //read gpio state
                 "memw;"                         //wait for read completion
                 :[gpio_in_mask] "=r" (gpio_in_mask)
                 :[gpio_in_addr] "r" (PERIPHS_GPIO_BASEADDR + GPIO_IN_ADDRESS)
    );
    sda_out = (gpio_in_mask >> i2c[id]->pin_SDA) & 1;
    return sda_out;
}

/******************************************************************************
 * FunctionName : i2c_master_get_pin_SDA
 * Description  : returns SDA pin number
 * Parameters   : bus id
 * Returns      : SDA pin number
*******************************************************************************/
uint8 ICACHE_FLASH_ATTR
i2c_master_get_pin_SDA(uint16 id){
    return (NULL == i2c[id]) ? 0 : i2c[id]->pin_SDA;
}

/******************************************************************************
 * FunctionName : i2c_master_get_pin_SCL
 * Description  : returns SCL pin number
 * Parameters   : bus id
 * Returns      : SCL pin number
*******************************************************************************/
uint8 ICACHE_FLASH_ATTR
i2c_master_get_pin_SCL(uint16 id){
    return (NULL == i2c[id]) ? 0 : i2c[id]->pin_SCL;
}

/******************************************************************************
 * FunctionName : i2c_master_get_speed
 * Description  : returns configured bus speed
 * Parameters   : bus id
 * Returns      : i2c speed
*******************************************************************************/
uint32 ICACHE_FLASH_ATTR
i2c_master_get_speed(uint16 id){
    return (NULL == i2c[id]) ? 0 : i2c[id]->speed;
}

/******************************************************************************
 * FunctionName : i2c_master_configured
 * Description  : checks if i2c bus is configured
 * Parameters   : bus id
 * Returns      : boolean value, true if configured
*******************************************************************************/
bool ICACHE_FLASH_ATTR
i2c_master_configured(uint16 id){
    return !(NULL == i2c[id]);
}

/******************************************************************************
 * FunctionName : i2c_master_init
 * Description  : initialize I2C bus to enable i2c operations
                  (reset state of all slave devices)
 * Parameters   : bus id
 * Returns      : NONE
*******************************************************************************/
void ICACHE_FLASH_ATTR
i2c_master_init(uint16 id)
{
    uint8 i;

    i2c_master_setDC(id, 1, 0);

    // when SCL = 0, toggle SDA to clear up
    i2c_master_setDC(id, 0, 0) ;
    i2c_master_setDC(id, 1, 0) ;

    // set data_cnt to max value
    for (i = 0; i < 28; i++) {
        i2c_master_setDC(id, 1, 0);
        i2c_master_setDC(id, 1, 1);
    }

    // reset all
    i2c_master_stop(id);
    return;
}

/******************************************************************************
 * FunctionName : i2c_master_gpio_init
 * Description  : Initializes and configures the driver on given bus ID
 * Parameters   : bus id
 * Returns      : configured speed
*******************************************************************************/
uint32 ICACHE_FLASH_ATTR
i2c_master_gpio_init(uint16 id, uint8 sda, uint8 scl, uint32 speed)
{
    if(NULL == i2c[id]){
        i2c[id] = (i2c_master_state_t*) c_malloc(sizeof(i2c_master_state_t));
    }
    if(NULL == i2c[id]){  // if malloc failed
        return 0;
    }
    i2c[id]->last_SDA = 1; //default idle state
    i2c[id]->last_SCL = 1;
    i2c[id]->pin_SDA = pin_num[sda];
    i2c[id]->pin_SCL = pin_num[scl];
    i2c[id]->pin_SDA_mask = 1 << i2c[id]->pin_SDA;
    i2c[id]->pin_SCL_mask = 1 << i2c[id]->pin_SCL;
    i2c[id]->pin_SDA_SCL_mask = i2c[id]->pin_SDA_mask | i2c[id]->pin_SCL_mask;
    i2c[id]->speed = speed;
    i2c[id]->cycles_delay = 0;

    if(i2c[id]->speed < MIN_SPEED){
        i2c[id]->speed = MIN_SPEED;
    }
    i2c_master_set_DC_delay(id); // recalibrate clock

    ETS_GPIO_INTR_DISABLE(); //disable gpio interrupts

    if (IS_PIN16(i2c[id]->pin_SCL)){ //if GPIO16
        //SET_PERI_REG_MASK(PAD_XPD_DCDC_CONF, 0x1); // select function RTC_GPIO0 for pin XPD_DCDC
        WRITE_PERI_REG(PAD_XPD_DCDC_CONF,
                       (READ_PERI_REG(PAD_XPD_DCDC_CONF) & 0xffffffbc) | (uint32)0x1); 	// mux configuration for XPD_DCDC to output rtc_gpio0
        //CLEAR_PERI_REG_MASK(RTC_GPIO_CONF, 0x1); //mux configuration for out enable
        WRITE_PERI_REG(RTC_GPIO_CONF,
                       (READ_PERI_REG(RTC_GPIO_CONF) & (uint32)0xfffffffe) | (uint32)0x0);	//mux configuration for out enable
        //SET_PERI_REG_MASK(RTC_GPIO_ENABLE, 0x1); //out enable
        WRITE_PERI_REG(RTC_GPIO_ENABLE,
                       (READ_PERI_REG(RTC_GPIO_ENABLE) & (uint32)0xfffffffe) | (uint32)0x1);	//out enable
        //SET_PERI_REG_MASK(RTC_GPIO_OUT, 0x1); // set SCL high
        WRITE_PERI_REG(RTC_GPIO_OUT,
                       (READ_PERI_REG(RTC_GPIO_OUT) & (uint32)0xfffffffe) | (uint32)0x1);	//set high
    }
    else{
        PIN_FUNC_SELECT(pin_mux[scl], pin_func[scl]);
        SET_PERI_REG_MASK(PERIPHS_GPIO_BASEADDR + GPIO_PIN_ADDR(GPIO_ID_PIN(i2c[id]->pin_SCL)),
                          GPIO_PIN_PAD_DRIVER_SET(GPIO_PAD_DRIVER_ENABLE)); //open drain
        gpio_output_set(i2c[id]->pin_SCL_mask, 0, i2c[id]->pin_SCL_mask, 0); //enable and set high
    }
    PIN_FUNC_SELECT(pin_mux[sda], pin_func[sda]);
    SET_PERI_REG_MASK(PERIPHS_GPIO_BASEADDR + GPIO_PIN_ADDR(GPIO_ID_PIN(i2c[id]->pin_SDA)),
                      GPIO_PIN_PAD_DRIVER_SET(GPIO_PAD_DRIVER_ENABLE)); //open drain
    gpio_output_set(i2c[id]->pin_SDA_mask, 0, i2c[id]->pin_SDA_mask, 0); //enable and set high

    ETS_GPIO_INTR_ENABLE(); //enable gpio interrupts

    i2c_master_init(id);
    return i2c[id]->speed;
}

/******************************************************************************
 * FunctionName : i2c_master_start
 * Description  : set i2c to send state
 * Parameters   : bus id
 * Returns      : NONE
*******************************************************************************/
void ICACHE_FLASH_ATTR
i2c_master_start(uint16 id)
{
    i2c_master_set_DC_delay(id); // recalibrate clock
    i2c_master_setDC(id, 1, i2c[id]->last_SCL);
    i2c_master_setDC(id, 1, 1);
    i2c_master_setDC(id, 0, 1);
}

/******************************************************************************
 * FunctionName : i2c_master_stop
 * Description  : set i2c to stop sending state
 * Parameters   : bus id
 * Returns      : NONE
*******************************************************************************/
void ICACHE_FLASH_ATTR
i2c_master_stop(uint16 id)
{
    i2c_master_setDC(id, 0, i2c[id]->last_SCL);
    i2c_master_setDC(id, 0, 1);
    i2c_master_setDC(id, 1, 1);
}

/******************************************************************************
 * FunctionName : i2c_master_setAck
 * Description  : set ack to i2c bus as level value
 * Parameters   : bus id, uint8 level - 0 or 1
 * Returns      : NONE
*******************************************************************************/
void ICACHE_FLASH_ATTR
i2c_master_setAck(uint16 id, uint8 level)
{
    i2c_master_setDC(id, i2c[id]->last_SDA, 0);
    i2c_master_setDC(id, level, 0);
    i2c_master_setDC(id, level, 1);
    i2c_master_setDC(id, level, 0);
    i2c_master_setDC(id, 1, 0);
}

/******************************************************************************
 * FunctionName : i2c_master_getAck
 * Description  : confirm if peer send ack
 * Parameters   : bus id
 * Returns      : uint8 - ack value, 0 or 1
*******************************************************************************/
uint8 ICACHE_FLASH_ATTR
i2c_master_getAck(uint16 id)
{
    uint8 retVal;
    i2c_master_setDC(id, i2c[id]->last_SDA, 0);
    i2c_master_setDC(id, 1, 0);
    i2c_master_setDC(id, 1, 1);
    retVal = i2c_master_getDC(id);
    i2c_master_setDC(id, 1, 0);
    return retVal;
}

/******************************************************************************
* FunctionName : i2c_master_checkAck
* Description  : get dev response
* Parameters   : bus id
* Returns      : true : get ack ; false : get nack
*******************************************************************************/
bool ICACHE_FLASH_ATTR
i2c_master_checkAck(uint16 id)
{
    if(i2c_master_getAck(id)){
        return FALSE;
    }else{
        return TRUE;
    }
}

/******************************************************************************
* FunctionName : i2c_master_send_ack
* Description  : response ack
* Parameters   : bus id
* Returns      : NONE
*******************************************************************************/
void ICACHE_FLASH_ATTR
i2c_master_send_ack(uint16 id)
{
    i2c_master_setAck(id, 0x0);
}
/******************************************************************************
* FunctionName : i2c_master_send_nack
* Description  : response nack
* Parameters   : bus id
* Returns      : NONE
*******************************************************************************/
void ICACHE_FLASH_ATTR
i2c_master_send_nack(uint16 id)
{
    i2c_master_setAck(id, 0x1);
}

/******************************************************************************
 * FunctionName : i2c_master_readByte
 * Description  : read Byte from i2c bus
 * Parameters   : bus id
 * Returns      : uint8 - readed value
*******************************************************************************/
uint8 ICACHE_FLASH_ATTR
i2c_master_readByte(uint16 id)
{
    uint8 retVal = 0;
    uint8 k;
    sint8 i;

    i2c_master_setDC(id, i2c[id]->last_SDA, 0);
    i2c_master_setDC(id, 1, 0);
    for (i = 7; i >= 0; i--) {
        i2c_master_setDC(id, 1, 1);
        k = i2c_master_getDC(id);
        i2c_master_setDC(id, 1, 0);
        k <<= i;
        retVal |= k;
    }
    return retVal;
}

/******************************************************************************
 * FunctionName : i2c_master_writeByte
 * Description  : write wrdata value(one byte) into i2c
 * Parameters   : bus id, uint8 wrdata - write value
 * Returns      : NONE
*******************************************************************************/
void ICACHE_FLASH_ATTR
i2c_master_writeByte(uint16 id, uint8 wrdata)
{
    uint8 dat;
    sint8 i;

    i2c_master_setDC(id, i2c[id]->last_SDA, 0);
    for (i = 7; i >= 0; i--) {
        dat = (wrdata >> i) & 1;
        i2c_master_setDC(id, dat, 0);
        i2c_master_setDC(id, dat, 1);
    }
    i2c_master_setDC(id, dat, 0);
}



#else // if defined I2C_MASTER_OLD_VERSION
/******************************************************************************
* OLD driver
* Enabled when I2C_MASTER_OLD_VERSION is defined in user_config.h
*******************************************************************************/

#define I2C_MASTER_SDA_MUX (pin_mux[sda])
#define I2C_MASTER_SCL_MUX (pin_mux[scl])
#define I2C_MASTER_SDA_GPIO (pinSDA)
#define I2C_MASTER_SCL_GPIO (pinSCL)
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
#define I2C_MASTER_SPEED 100000
#define I2C_MASTER_BUS_ID 0

LOCAL uint8 m_nLastSDA;
LOCAL uint8 m_nLastSCL;

LOCAL uint8 pinSDA = 2;
LOCAL uint8 pinSCL = 15;

/******************************************************************************
 * FunctionName : i2c_master_setDC
 * Description  : Internal used function -
 *                    set i2c SDA and SCL bit value for half clk cycle
 * Parameters   : uint8 SDA, uint8 SCL
 * Returns      : NONE
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
i2c_master_setDC(uint8 SDA, uint8 SCL)
{
    uint8 sclLevel;

    SDA	&= 0x01;
    SCL	&= 0x01;
    m_nLastSDA = SDA;
    m_nLastSCL = SCL;

    if ((0 == SDA) && (0 == SCL)) {
        I2C_MASTER_SDA_LOW_SCL_LOW();
    } else if ((0 == SDA) && (1 == SCL)) {
        I2C_MASTER_SDA_LOW_SCL_HIGH();
    } else if ((1 == SDA) && (0 == SCL)) {
        I2C_MASTER_SDA_HIGH_SCL_LOW();
    } else {
        I2C_MASTER_SDA_HIGH_SCL_HIGH();
    }
    if(1 == SCL) {
        do {
            sclLevel = GPIO_INPUT_GET(GPIO_ID_PIN(I2C_MASTER_SCL_GPIO));
        } while(sclLevel == 0);
    }
    i2c_master_wait(5);
}

/******************************************************************************
 * FunctionName : i2c_master_getDC
 * Description  : Internal used function -
 *                    get i2c SDA bit value
 * Parameters   : NONE
 * Returns      : uint8 - SDA bit value
*******************************************************************************/
LOCAL uint8 ICACHE_FLASH_ATTR
i2c_master_getDC()
{
    uint8 sda_out;
    sda_out = GPIO_INPUT_GET(GPIO_ID_PIN(I2C_MASTER_SDA_GPIO));
    return sda_out;
}

/******************************************************************************
 * FunctionName : i2c_master_init
 * Description  : initilize I2C bus to enable i2c operations
 * Parameters   : bus id
 * Returns      : NONE
*******************************************************************************/
void ICACHE_FLASH_ATTR
i2c_master_init(uint16 id)
{
    uint8 i;

    i2c_master_setDC(1, 0);

    // when SCL = 0, toggle SDA to clear up
    i2c_master_setDC(0, 0) ;
    i2c_master_setDC(1, 0) ;

    // set data_cnt to max value
    for (i = 0; i < 28; i++) {
        i2c_master_setDC(1, 0);
        i2c_master_setDC(1, 1);
    }

    // reset all
    i2c_master_stop(I2C_MASTER_BUS_ID);
    return;
}

/******************************************************************************
 * FunctionName : i2c_master_get_pin_SDA
 * Description  : returns SDA pin number
 * Parameters   : bus id
 * Returns      : SDA pin number
*******************************************************************************/
uint8 ICACHE_FLASH_ATTR
i2c_master_get_pinSDA(uint16 id){
    return pinSDA;
}

/******************************************************************************
 * FunctionName : i2c_master_get_pin_SCL
 * Description  : returns SCL pin number
 * Parameters   : bus id
 * Returns      : SCL pin number
*******************************************************************************/
uint8 ICACHE_FLASH_ATTR
i2c_master_get_pinSCL(uint16 id){
    return pinSCL;
}

/******************************************************************************
 * FunctionName : i2c_master_get_speed
 * Description  : returns configured bus speed
 * Parameters   : bus id
 * Returns      : i2c speed
*******************************************************************************/
uint32 ICACHE_FLASH_ATTR
i2c_master_get_speed(uint16 id){
    return I2C_MASTER_SPEED;
}

/******************************************************************************
 * FunctionName : i2c_master_configured
 * Description  : checks if i2c bus is configured
 * Parameters   : bus id
 * Returns      : boolean value, true if configured
*******************************************************************************/
bool ICACHE_FLASH_ATTR
i2c_master_configured(uint16 id){
    return true;
}

/******************************************************************************
 * FunctionName : i2c_master_gpio_init
 * Description  : config SDA and SCL gpio to open-drain output mode,
 *                mux and gpio num defined in i2c_master.h
 * Parameters   : bus id, uint8 sda, uint8 scl, uint32 speed 
 * Returns      : configured speed
*******************************************************************************/
uint32 ICACHE_FLASH_ATTR
i2c_master_gpio_init(uint16 id, uint8 sda, uint8 scl, uint32 speed)
{
    pinSDA = pin_num[sda];
    pinSCL = pin_num[scl];

    ETS_GPIO_INTR_DISABLE() ;
//    ETS_INTR_LOCK();

    PIN_FUNC_SELECT(I2C_MASTER_SDA_MUX, I2C_MASTER_SDA_FUNC);
    PIN_FUNC_SELECT(I2C_MASTER_SCL_MUX, I2C_MASTER_SCL_FUNC);

    GPIO_REG_WRITE(GPIO_PIN_ADDR(GPIO_ID_PIN(I2C_MASTER_SDA_GPIO)), GPIO_REG_READ(GPIO_PIN_ADDR(GPIO_ID_PIN(I2C_MASTER_SDA_GPIO))) | GPIO_PIN_PAD_DRIVER_SET(GPIO_PAD_DRIVER_ENABLE)); //open drain;
    GPIO_REG_WRITE(GPIO_ENABLE_ADDRESS, GPIO_REG_READ(GPIO_ENABLE_ADDRESS) | (1 << I2C_MASTER_SDA_GPIO));
    GPIO_REG_WRITE(GPIO_PIN_ADDR(GPIO_ID_PIN(I2C_MASTER_SCL_GPIO)), GPIO_REG_READ(GPIO_PIN_ADDR(GPIO_ID_PIN(I2C_MASTER_SCL_GPIO))) | GPIO_PIN_PAD_DRIVER_SET(GPIO_PAD_DRIVER_ENABLE)); //open drain;
    GPIO_REG_WRITE(GPIO_ENABLE_ADDRESS, GPIO_REG_READ(GPIO_ENABLE_ADDRESS) | (1 << I2C_MASTER_SCL_GPIO));

    I2C_MASTER_SDA_HIGH_SCL_HIGH();

    ETS_GPIO_INTR_ENABLE() ;
//    ETS_INTR_UNLOCK();

    i2c_master_init(I2C_MASTER_BUS_ID);

    return I2C_MASTER_SPEED;
}

/******************************************************************************
 * FunctionName : i2c_master_start
 * Description  : set i2c to send state
 * Parameters   : bus id
 * Returns      : NONE
*******************************************************************************/
void ICACHE_FLASH_ATTR
i2c_master_start(uint16 id)
{
    i2c_master_setDC(1, m_nLastSCL);
    i2c_master_setDC(1, 1);
    i2c_master_setDC(0, 1);
}

/******************************************************************************
 * FunctionName : i2c_master_stop
 * Description  : set i2c to stop sending state
 * Parameters   : bus id
 * Returns      : NONE
*******************************************************************************/
void ICACHE_FLASH_ATTR
i2c_master_stop(uint16 id)
{
    i2c_master_wait(5);

    i2c_master_setDC(0, m_nLastSCL);
    i2c_master_setDC(0, 1);
    i2c_master_setDC(1, 1);
}

/******************************************************************************
 * FunctionName : i2c_master_setAck
 * Description  : set ack to i2c bus as level value
 * Parameters   : bus id, uint8 level - 0 or 1
 * Returns      : NONE
*******************************************************************************/
void ICACHE_FLASH_ATTR
i2c_master_setAck(uint16 id, uint8 level)
{
    i2c_master_setDC(m_nLastSDA, 0);
    i2c_master_setDC(level, 0);
    i2c_master_setDC(level, 1);
    i2c_master_wait(3);
    i2c_master_setDC(level, 0);
    i2c_master_setDC(1, 0);
}

/******************************************************************************
 * FunctionName : i2c_master_getAck
 * Description  : confirm if peer send ack
 * Parameters   : bus id
 * Returns      : uint8 - ack value, 0 or 1
*******************************************************************************/
uint8 ICACHE_FLASH_ATTR
i2c_master_getAck(uint16 id)
{
    uint8 retVal;
    i2c_master_setDC(m_nLastSDA, 0);
    i2c_master_setDC(1, 0);
    i2c_master_setDC(1, 1);

    retVal = i2c_master_getDC();
    i2c_master_wait(5);
    i2c_master_setDC(1, 0);

    return retVal;
}

/******************************************************************************
* FunctionName : i2c_master_checkAck
* Description  : get dev response
* Parameters   : bus id
* Returns      : true : get ack ; false : get nack
*******************************************************************************/
bool ICACHE_FLASH_ATTR
i2c_master_checkAck(uint16 id)
{
    if(i2c_master_getAck(I2C_MASTER_BUS_ID)){
        return FALSE;
    }else{
        return TRUE;
    }
}

/******************************************************************************
* FunctionName : i2c_master_send_ack
* Description  : response ack
* Parameters   : bus id
* Returns      : NONE
*******************************************************************************/
void ICACHE_FLASH_ATTR
i2c_master_send_ack(uint16 id)
{
    i2c_master_setAck(I2C_MASTER_BUS_ID, 0x0);
}
/******************************************************************************
* FunctionName : i2c_master_send_nack
* Description  : response nack
* Parameters   : bus id
* Returns      : NONE
*******************************************************************************/
void ICACHE_FLASH_ATTR
i2c_master_send_nack(uint16 id)
{
    i2c_master_setAck(I2C_MASTER_BUS_ID, 0x1);
}

/******************************************************************************
 * FunctionName : i2c_master_readByte
 * Description  : read Byte from i2c bus
 * Parameters   : bus id
 * Returns      : uint8 - readed value
*******************************************************************************/
uint8 ICACHE_FLASH_ATTR
i2c_master_readByte(uint16 id)
{
    uint8 retVal = 0;
    uint8 k, i;

    i2c_master_wait(5);
    i2c_master_setDC(m_nLastSDA, 0);

    for (i = 0; i < 8; i++) {
        i2c_master_wait(5);
        i2c_master_setDC(1, 0);
        i2c_master_setDC(1, 1);

        k = i2c_master_getDC();
        i2c_master_wait(5);

        if (i == 7) {
            i2c_master_wait(3);   ////
        }

        k <<= (7 - i);
        retVal |= k;
    }

    i2c_master_setDC(1, 0);

    return retVal;
}

/******************************************************************************
 * FunctionName : i2c_master_writeByte
 * Description  : write wrdata value(one byte) into i2c
 * Parameters   : bus id, uint8 wrdata - write value
 * Returns      : NONE
*******************************************************************************/
void ICACHE_FLASH_ATTR
i2c_master_writeByte(uint16 id, uint8 wrdata)
{
    uint8 dat;
    sint8 i;

    i2c_master_wait(5);

    i2c_master_setDC(m_nLastSDA, 0);

    for (i = 7; i >= 0; i--) {
        dat = wrdata >> i;
        i2c_master_setDC(dat, 0);
        i2c_master_setDC(dat, 1);

        if (i == 0) {
            i2c_master_wait(3);   ////
        }

        i2c_master_setDC(dat, 0);
    }
}
#endif
