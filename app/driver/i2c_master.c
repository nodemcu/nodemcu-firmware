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

//used only by old driver
#define i2c_master_wait os_delay_us

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
#ifdef I2C_MASTER_OLD_VERSION
    // intentionally not optimized to keep timings of old version accurate
    uint8 sclLevel;

    SDA	&= 0x01;
    SCL	&= 0x01;
    i2c[id]->last_SDA = SDA;
    i2c[id]->last_SCL = SCL;

    if ((0 == SDA) && (0 == SCL)) {
        gpio_output_set(0, 1<<i2c[id]->pin_SDA | 1<<i2c[id]->pin_SCL, 1<<i2c[id]->pin_SDA | 1<<i2c[id]->pin_SCL, 0);
    } else if ((0 == SDA) && (1 == SCL)) {
        gpio_output_set(1<<i2c[id]->pin_SCL, 1<<i2c[id]->pin_SDA, 1<<i2c[id]->pin_SDA | 1<<i2c[id]->pin_SCL, 0);
    } else if ((1 == SDA) && (0 == SCL)) {
        gpio_output_set(1<<i2c[id]->pin_SDA, 1<<i2c[id]->pin_SCL, 1<<i2c[id]->pin_SDA | 1<<i2c[id]->pin_SCL, 0);
    } else {
        gpio_output_set(1<<i2c[id]->pin_SDA | 1<<i2c[id]->pin_SCL, 0, 1<<i2c[id]->pin_SDA | 1<<i2c[id]->pin_SCL, 0);
    }
    if(1 == SCL) {
        do {
            sclLevel = GPIO_INPUT_GET(GPIO_ID_PIN(i2c[id]->pin_SCL));
        } while(0 == sclLevel);
    }
    i2c_master_wait(5);
#else
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

#endif
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
#ifdef I2C_MASTER_OLD_VERSION
    sda_out = GPIO_INPUT_GET(GPIO_ID_PIN(i2c[id]->pin_SDA));
#else
    uint32 gpio_in_mask;
    //instead of: gpio_in_mask = gpio_input_get();
    asm volatile("l16ui %[gpio_in_mask], %[gpio_in_addr], 0;" //read gpio state
                 "memw;"                         //wait for read completion
                 :[gpio_in_mask] "=r" (gpio_in_mask)
                 :[gpio_in_addr] "r" (PERIPHS_GPIO_BASEADDR + GPIO_IN_ADDRESS)
    );
    sda_out = (gpio_in_mask >> i2c[id]->pin_SDA) & 1;
#endif
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
uint16 ICACHE_FLASH_ATTR
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

#ifdef I2C_MASTER_OLD_VERSION
    i2c[id]->speed = 100000; //dummy speed for backward compatibility
#endif

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
#ifdef I2C_MASTER_OLD_VERSION
    i2c_master_wait(5);
#endif
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
#ifdef I2C_MASTER_OLD_VERSION
    i2c_master_wait(3);
#endif
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

#ifdef I2C_MASTER_OLD_VERSION
    i2c_master_wait(5);
    i2c_master_setDC(id, i2c[id]->last_SDA, 0);
    for (i = 7; i >= 0; i--) {
        i2c_master_setDC(id, 1, 0);
        i2c_master_setDC(id, 1, 1);
        k = i2c_master_getDC(id);
        k <<= i;
        retVal |= k;
    }
    i2c_master_wait(3);
    i2c_master_setDC(id, 1, 0);
#else
    i2c_master_setDC(id, i2c[id]->last_SDA, 0);
    i2c_master_setDC(id, 1, 0);
    for (i = 7; i >= 0; i--) {
        i2c_master_setDC(id, 1, 1);
        k = i2c_master_getDC(id);
        i2c_master_setDC(id, 1, 0);
        k <<= i;
        retVal |= k;
    }
#endif
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

#ifdef I2C_MASTER_OLD_VERSION
    i2c_master_wait(5);

    i2c_master_setDC(id, i2c[id]->last_SDA, 0);
    i2c_master_wait(5);

    for (i = 7; i >= 0; i--) {
        dat = wrdata >> i;
        i2c_master_setDC(id, dat, 0);
        i2c_master_wait(5);
        i2c_master_setDC(id, dat, 1);
        i2c_master_wait(5);

        if (i == 0) {
            i2c_master_wait(3);   ////
        }

        i2c_master_setDC(id, dat, 0);
        i2c_master_wait(5);
    }
#else
    i2c_master_setDC(id, i2c[id]->last_SDA, 0);
    for (i = 7; i >= 0; i--) {
        dat = (wrdata >> i) & 1;
        i2c_master_setDC(id, dat, 0);
        i2c_master_setDC(id, dat, 1);
    }
    i2c_master_setDC(id, dat, 0);
#endif
}
