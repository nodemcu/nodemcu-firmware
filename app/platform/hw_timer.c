/******************************************************************************
* Copyright 2013-2014 Espressif Systems (Wuxi)
*
* FileName: hw_timer.c
*
* Description: hw_timer driver
*
* Modification history:
*     2014/5/1, v1.0 create this file.
*     
* Adapted for NodeMCU 2016
* 
* The owner parameter should be a unique value per module using this API
* It could be a pointer to a bit of data or code 
* e.g.   #define OWNER    ((os_param_t) module_init)   
* where module_init is a function. For builtin modules, it might be 
* a small numeric value that is known not to clash.
*******************************************************************************/
#include "ets_sys.h"
#include "os_type.h"
#include "osapi.h"

#include "hw_timer.h"

#define FRC1_ENABLE_TIMER  BIT7
#define FRC1_AUTO_LOAD  BIT6

//TIMER PREDIVIDED MODE
typedef enum {
    DIVIDED_BY_1 = 0,		//timer clock
    DIVIDED_BY_16 = 4,	//divided by 16
    DIVIDED_BY_256 = 8,	//divided by 256
} TIMER_PREDIVIDED_MODE;

typedef enum {			//timer interrupt mode
    TM_LEVEL_INT = 1,	// level interrupt
    TM_EDGE_INT   = 0,	//edge interrupt
} TIMER_INT_MODE;

static os_param_t the_owner;
static os_param_t callback_arg;
static void (* user_hw_timer_cb)(os_param_t);

#define VERIFY_OWNER(owner)  if (owner != the_owner) { if (the_owner) { return 0; } the_owner = owner; }

/******************************************************************************
* FunctionName : platform_hw_timer_arm_ticks
* Description  : set a trigger timer delay for this timer.
* Parameters   : os_param_t owner
*                uint32 ticks :
* Returns      : true if it worked
*******************************************************************************/
bool ICACHE_RAM_ATTR platform_hw_timer_arm_ticks(os_param_t owner, uint32_t ticks)
{
  VERIFY_OWNER(owner);
  RTC_REG_WRITE(FRC1_LOAD_ADDRESS, ticks);

  return 1;
}

/******************************************************************************
* FunctionName : platform_hw_timer_arm_us
* Description  : set a trigger timer delay for this timer.
* Parameters   : os_param_t owner
*                uint32 microseconds :
* in autoload mode
*                         50 ~ 0x7fffff;  for FRC1 source.
*                         100 ~ 0x7fffff;  for NMI source.
* in non autoload mode:
*                         10 ~ 0x7fffff;
* Returns      : true if it worked
*******************************************************************************/
bool ICACHE_RAM_ATTR platform_hw_timer_arm_us(os_param_t owner, uint32_t microseconds)
{
  VERIFY_OWNER(owner);
  RTC_REG_WRITE(FRC1_LOAD_ADDRESS, US_TO_RTC_TIMER_TICKS(microseconds));

  return 1;
}

/******************************************************************************
* FunctionName : platform_hw_timer_set_func
* Description  : set the func, when trigger timer is up.
* Parameters   : os_param_t owner
*                void (* user_hw_timer_cb_set)(os_param_t):
                        timer callback function
*	         os_param_t arg
* Returns      : true if it worked
*******************************************************************************/
bool platform_hw_timer_set_func(os_param_t owner, void (* user_hw_timer_cb_set)(os_param_t), os_param_t arg)
{
  VERIFY_OWNER(owner);
  callback_arg = arg;
  user_hw_timer_cb = user_hw_timer_cb_set;
  return 1;
}

static void ICACHE_RAM_ATTR hw_timer_isr_cb(void *arg)
{
  if (user_hw_timer_cb != NULL) {
    (*(user_hw_timer_cb))(callback_arg);
  }
}

static void ICACHE_RAM_ATTR hw_timer_nmi_cb(void)
{
  if (user_hw_timer_cb != NULL) {
    (*(user_hw_timer_cb))(callback_arg);
  }
}

/******************************************************************************
* FunctionName : platform_hw_timer_get_delay_ticks
* Description  : figure out how long since th last timer interrupt
* Parameters   : os_param_t owner
* Returns      : the number of ticks
*******************************************************************************/
uint32_t ICACHE_RAM_ATTR platform_hw_timer_get_delay_ticks(os_param_t owner)
{
  VERIFY_OWNER(owner);

  return (- RTC_REG_READ(FRC1_COUNT_ADDRESS)) & ((1 << 23) - 1);
}

/******************************************************************************
* FunctionName : platform_hw_timer_init
* Description  : initialize the hardware isr timer
* Parameters   : os_param_t owner
* FRC1_TIMER_SOURCE_TYPE source_type:
*                         FRC1_SOURCE,    timer use frc1 isr as isr source.
*                         NMI_SOURCE,     timer use nmi isr as isr source.
* bool autoload:
*                         0,  not autoload,
*                         1,  autoload mode,
* Returns      : true if it worked
*******************************************************************************/
bool platform_hw_timer_init(os_param_t owner, FRC1_TIMER_SOURCE_TYPE source_type, bool autoload)
{
  VERIFY_OWNER(owner);
  if (autoload) {
    RTC_REG_WRITE(FRC1_CTRL_ADDRESS,
		  FRC1_AUTO_LOAD | DIVIDED_BY_16 | FRC1_ENABLE_TIMER | TM_EDGE_INT);
  } else {
    RTC_REG_WRITE(FRC1_CTRL_ADDRESS,
		  DIVIDED_BY_16 | FRC1_ENABLE_TIMER | TM_EDGE_INT);
  }

  if (source_type == NMI_SOURCE) {
    ETS_FRC_TIMER1_NMI_INTR_ATTACH(hw_timer_nmi_cb);
  } else {
    ETS_FRC_TIMER1_INTR_ATTACH(hw_timer_isr_cb, NULL);
  }

  TM1_EDGE_INT_ENABLE();
  ETS_FRC1_INTR_ENABLE();

  return 1;
}

/******************************************************************************
* FunctionName : platform_hw_timer_close
* Description  : ends use of the hardware isr timer
* Parameters   : os_param_t owner
* Returns      : true if it worked
*******************************************************************************/
bool ICACHE_RAM_ATTR platform_hw_timer_close(os_param_t owner)
{
  VERIFY_OWNER(owner);

  /* Set no reload mode */
  RTC_REG_WRITE(FRC1_CTRL_ADDRESS,
		DIVIDED_BY_16 | TM_EDGE_INT);

  TM1_EDGE_INT_DISABLE();
  ETS_FRC1_INTR_DISABLE();

  user_hw_timer_cb = NULL;

  the_owner = 0;

  return 1;
}

