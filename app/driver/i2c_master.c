/******************************************************************************
 * Copyright 2013-2014 Espressif Systems (Wuxi)
 *
 * FileName: i2c_master.c
 *
 * Description: i2c master API
 *
 * Modification history:
 *     2014/3/12, v1.0 create this file.
*******************************************************************************/
#include "ets_sys.h"
#include "osapi.h"
#include "gpio.h"
#include "user_interface.h"
#include "cpu_esp8266.h"
#include "driver/i2c_master.h"
#include "pin_map.h"

#define CPU_CYCLES_BETWEEN_DELAYS 80
#define MIN_SPEED 25000
#define GPIO16_SCL_MAX_SPEED 400000
#define GPIO16_RTC_REG_MASK 0xfffffffe

static i2c_master_state i2c[NUM_I2C];

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
  i2c[id].cycles_delay = system_get_cpu_freq() * 500000 / i2c[id].speed - CPU_CYCLES_BETWEEN_DELAYS;
  if(i2c[id].cycles_delay < 0){
      i2c[id].cycles_delay = 0;
  }
}

/******************************************************************************
 * FunctionName : i2c_master_setDC
 * Description  : Internal used function -
 *                    set i2c SDA and SCL bit value for half clk cycle
 * Parameters   : bus id, uint8 SDA, uint8 SCL
 * Returns      : NONE
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
i2c_master_setDC(uint16 id, uint8 SDA, uint8 SCL)
{
#ifdef I2C_MASTER_OLD_VERSION
    uint8 sclLevel;

    SDA	&= 0x01;
    SCL	&= 0x01;
    i2c[id].m_nLastSDA = SDA;
    i2c[id].m_nLastSCL = SCL;

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
#else
    uint32 cycles_start;
    uint32 cycles_curr;
    uint32 this_SDA_SCL_set_mask = (SDA << i2c[id].pinSDA) | (SCL << i2c[id].pinSCL);
    uint32 this_SDA_SCL_clear_mask = i2c[id].pin_SDA_SCL_mask ^ this_SDA_SCL_set_mask;
    i2c[id].m_nLastSDA = SDA;
    i2c[id].m_nLastSCL = SCL;

    if(i2c[id].cycles_delay > 0){//
        asm volatile("rsr %0, ccount":"=a"(cycles_start)); //Cycle count register
        do{
            asm volatile("rsr %0, ccount":"=a"(cycles_curr));
        } while (cycles_curr - cycles_start <  i2c[id].cycles_delay);
    }
    if(16 == i2c[id].pinSCL){ //if GPIO16
        WRITE_PERI_REG(RTC_GPIO_OUT,
                       (READ_PERI_REG(RTC_GPIO_OUT) & (uint32)GPIO16_RTC_REG_MASK) | (uint32)SCL);
        GPIO_REG_WRITE(GPIO_OUT_W1TC_ADDRESS, this_SDA_SCL_clear_mask); //SCL is shifted out of mask so no worry
        GPIO_REG_WRITE(GPIO_OUT_W1TS_ADDRESS, this_SDA_SCL_set_mask);
    }
    else{
        GPIO_REG_WRITE(GPIO_OUT_W1TC_ADDRESS, this_SDA_SCL_clear_mask);
        GPIO_REG_WRITE(GPIO_OUT_W1TS_ADDRESS, this_SDA_SCL_set_mask);
        if(1 == SCL) { //clock stretching
            //instead of: while(!((gpio_input_get() >> pinSCL) & 1)) {}
            asm volatile("l_wait:"
                         "l16ui %0, %[gpio_in_addr], 0;" //read gpio state into register %0
                         "memw;"                         //wait for read completion
                         "bnall %0, %[gpio_SCL_mask], l_wait;" // test if SCL bit not set
                         ::[gpio_SCL_mask] "r" (i2c[id].pin_SCL_mask),
                         [gpio_in_addr] "r" (PERIPHS_GPIO_BASEADDR + GPIO_IN_ADDRESS)
            );
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
LOCAL uint8 ICACHE_FLASH_ATTR
i2c_master_getDC(uint16 id)
{
    uint8 sda_out;
#ifdef I2C_MASTER_OLD_VERSION
    sda_out = GPIO_INPUT_GET(GPIO_ID_PIN(I2C_MASTER_SDA_GPIO));
#else
    uint32 gpio_in_mask;
    //instead of: gpio_in_mask = gpio_input_get();
    asm volatile("l16ui %[gpio_in_mask], %[gpio_in_addr], 0;" //read gpio state
                 "memw;"                         //wait for read completion
                 :[gpio_in_mask] "=r" (gpio_in_mask)
                 :[gpio_in_addr] "r" (PERIPHS_GPIO_BASEADDR + GPIO_IN_ADDRESS)
    );
    sda_out = (gpio_in_mask >> i2c[id].pinSDA) & 1;
#endif
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
 * FunctionName : i2c_master_get_pinSDA
 * Description  : returns SDA pin number
 * Parameters   : bus id
 * Returns      : SDA pin number
*******************************************************************************/
uint8 ICACHE_FLASH_ATTR
i2c_master_get_pinSDA(uint16 id){
    return i2c[id].pinSDA;
}

/******************************************************************************
 * FunctionName : i2c_master_get_pinSCL
 * Description  : returns SCL pin number
 * Parameters   : bus id
 * Returns      : SCL pin number
*******************************************************************************/
uint8 ICACHE_FLASH_ATTR
i2c_master_get_pinSCL(uint16 id){
    return i2c[id].pinSCL;
}

/******************************************************************************
 * FunctionName : i2c_master_configured
 * Description  : returns readiness of i2c bus
                  by checking if speed paramter is set
 * Parameters   : bus id
 * Returns      : boolean value, true if configured
*******************************************************************************/
bool ICACHE_FLASH_ATTR
i2c_master_configured(uint16 id){
    return (i2c[id].speed > 0);
}

/******************************************************************************
 * FunctionName : i2c_master_gpio_init
 * Description  : config SDA and SCL gpio to open-drain output mode,
 *                mux and gpio num defined in i2c_master.h
 * Parameters   : bus id
 * Returns      : configured speed
*******************************************************************************/
uint32 ICACHE_FLASH_ATTR
i2c_master_gpio_init(uint16 id, uint8 sda, uint8 scl, uint32 speed)
{
    i2c[id].pinSDA = pin_num[sda];
    i2c[id].pinSCL = pin_num[scl];
    i2c[id].pin_SDA_mask = (1 << i2c[id].pinSDA);
    i2c[id].pin_SCL_mask = (1 << i2c[id].pinSCL);
    i2c[id].pin_SDA_SCL_mask = (1 << i2c[id].pinSDA) | (1 << i2c[id].pinSCL);
    i2c[id].speed = speed;

    if(i2c[id].speed < MIN_SPEED){
        i2c[id].speed = MIN_SPEED;
    }
    if(16 == i2c[id].pinSDA & i2c[id].speed > GPIO16_SCL_MAX_SPEED){
       i2c[id].speed = GPIO16_SCL_MAX_SPEED;
    }

    i2c_master_set_DC_delay(id); // recalibrate clock

    ETS_GPIO_INTR_DISABLE() ;
//    ETS_INTR_LOCK();

#ifdef I2C_MASTER_OLD_VERSION
    i2c[id].speed = 100000;

    PIN_FUNC_SELECT(I2C_MASTER_SDA_MUX, I2C_MASTER_SDA_FUNC);
    PIN_FUNC_SELECT(I2C_MASTER_SCL_MUX, I2C_MASTER_SCL_FUNC);

    GPIO_REG_WRITE(GPIO_PIN_ADDR(GPIO_ID_PIN(I2C_MASTER_SDA_GPIO)), GPIO_REG_READ(GPIO_PIN_ADDR(GPIO_ID_PIN(I2C_MASTER_SDA_GPIO))) | GPIO_PIN_PAD_DRIVER_SET(GPIO_PAD_DRIVER_ENABLE)); //open drain;
    GPIO_REG_WRITE(GPIO_ENABLE_ADDRESS, GPIO_REG_READ(GPIO_ENABLE_ADDRESS) | (1 << I2C_MASTER_SDA_GPIO));
    GPIO_REG_WRITE(GPIO_PIN_ADDR(GPIO_ID_PIN(I2C_MASTER_SCL_GPIO)), GPIO_REG_READ(GPIO_PIN_ADDR(GPIO_ID_PIN(I2C_MASTER_SCL_GPIO))) | GPIO_PIN_PAD_DRIVER_SET(GPIO_PAD_DRIVER_ENABLE)); //open drain;
    GPIO_REG_WRITE(GPIO_ENABLE_ADDRESS, GPIO_REG_READ(GPIO_ENABLE_ADDRESS) | (1 << I2C_MASTER_SCL_GPIO));

    I2C_MASTER_SDA_HIGH_SCL_HIGH();
#else
    if(16 == i2c[id].pinSCL){ //if GPIO16
        WRITE_PERI_REG(PAD_XPD_DCDC_CONF,
                      (READ_PERI_REG(PAD_XPD_DCDC_CONF) & 0xffffffbc) | (uint32)0x1); 	// mux configuration for XPD_DCDC to output rtc_gpio0
        WRITE_PERI_REG(RTC_GPIO_CONF,
                      (READ_PERI_REG(RTC_GPIO_CONF) & (uint32)0xfffffffe) | (uint32)0x0);	//mux configuration for out enable
        WRITE_PERI_REG(RTC_GPIO_ENABLE,
                      (READ_PERI_REG(RTC_GPIO_ENABLE) & (uint32)0xfffffffe) | (uint32)0x1);	//out enable
    }
    else{
        PIN_FUNC_SELECT(pin_mux[scl], pin_func[scl]);
        GPIO_REG_WRITE(GPIO_PIN_ADDR(GPIO_ID_PIN(i2c[id].pinSCL)),
                      GPIO_REG_READ(GPIO_PIN_ADDR(GPIO_ID_PIN(i2c[id].pinSCL))) | GPIO_PIN_PAD_DRIVER_SET(GPIO_PAD_DRIVER_ENABLE)); //open drain;
        GPIO_REG_WRITE(GPIO_ENABLE_ADDRESS, GPIO_REG_READ(GPIO_ENABLE_ADDRESS) | i2c[id].pin_SCL_mask);
        gpio_output_set(i2c[id].pin_SCL_mask, 0, i2c[id].pin_SCL_mask, 0);
    }

    PIN_FUNC_SELECT(pin_mux[sda], pin_func[sda]);
    GPIO_REG_WRITE(GPIO_PIN_ADDR(GPIO_ID_PIN(i2c[id].pinSDA)),
                   GPIO_REG_READ(GPIO_PIN_ADDR(GPIO_ID_PIN(i2c[id].pinSDA))) | GPIO_PIN_PAD_DRIVER_SET(GPIO_PAD_DRIVER_ENABLE)); //open drain;
    GPIO_REG_WRITE(GPIO_ENABLE_ADDRESS, GPIO_REG_READ(GPIO_ENABLE_ADDRESS) | i2c[id].pin_SDA_mask);
    gpio_output_set(i2c[id].pin_SDA_mask, 0, i2c[id].pin_SDA_mask, 0);
#endif

    ETS_GPIO_INTR_ENABLE() ;
//    ETS_INTR_UNLOCK();

    i2c_master_init(id);
    return i2c[id].speed;
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
    i2c_master_setDC(id, 1, i2c[id].m_nLastSCL);
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
    i2c_master_setDC(id, 0, i2c[id].m_nLastSCL);
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
    i2c_master_setDC(id, i2c[id].m_nLastSDA, 0);
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
    i2c_master_setDC(id, i2c[id].m_nLastSDA, 0);
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
#endif
    i2c_master_setDC(id, i2c[id].m_nLastSDA, 0);
    for (i = 7; i >= 0; i--) {
        i2c_master_setDC(id, 1, 0);
        i2c_master_setDC(id, 1, 1);
        k = i2c_master_getDC(id);
        k <<= i;
        retVal |= k;
    }
#ifdef I2C_MASTER_OLD_VERSION
    i2c_master_wait(3);
#endif
    i2c_master_setDC(id, 1, 0);
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
    i2c_master_wait(3);
#endif
    i2c_master_setDC(id, i2c[id].m_nLastSDA, 0);
    for (i = 7; i >= 0; i--) {
        dat = (wrdata >> i) & 1;
        i2c_master_setDC(id, dat, 0);
        i2c_master_setDC(id, dat, 1);
    }
#ifdef I2C_MASTER_OLD_VERSION
    i2c_master_wait(3);
#endif
    i2c_master_setDC(id, dat, 0);
}
