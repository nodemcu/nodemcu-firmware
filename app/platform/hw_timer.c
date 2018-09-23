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
#include "platform.h"
#include "c_stdio.h"
#include "c_stdlib.h"
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

typedef struct _timer_user {
  struct _timer_user *next;
  bool autoload;
  int32_t delay;
  int32_t autoload_delay;
  os_param_t owner;
  os_param_t callback_arg;
  void (* user_hw_timer_cb)(os_param_t);
  int cb_count;
} timer_user;

static timer_user *active;
static timer_user *inactive;
static uint8_t lock_count;
static uint8_t timer_running;

#define LOCK()   do { ets_intr_lock(); lock_count++; } while (0)
#define UNLOCK()   if (--lock_count == 0) ets_intr_unlock()

#if 0
void hw_timer_debug() {
  dbg_printf("timer_running=%d\n", timer_running);
  timer_user *tu;
  for (tu = active; tu; tu = tu->next) {
    dbg_printf("Owner: 0x%x, delay=%d, autoload=%d, autoload_delay=%d, cb_count=%d\n",
        tu->owner, tu->delay, tu->autoload, tu->autoload_delay, tu->cb_count);
  }
}
#endif

static void adjust_root() {
  // Can only ge called with interrupts disabled
  // change the initial active delay so that relative stuff still works
  if (active && timer_running) {
    int32_t time_left = (RTC_REG_READ(FRC1_COUNT_ADDRESS)) & ((1 << 23) - 1);
    if (time_left > active->delay) {
      // We have missed the interrupt
      time_left -= 1 << 23;
    }
    active->delay = time_left;
  }
}

static timer_user *find_tu(os_param_t owner) {
  // Try the inactive chain first
  timer_user **p;

  LOCK();

  for (p = &inactive; *p; p = &((*p)->next)) {
    if ((*p)->owner == owner) {
      timer_user *result = *p;

      UNLOCK();

      return result;
    }
  }

  for (p = &active; *p; p = &((*p)->next)) {
    if ((*p)->owner == owner) {
      timer_user *result = *p;

      UNLOCK();

      return result;
    }
  }

  UNLOCK();
  return NULL;
}

static timer_user *find_tu_and_remove(os_param_t owner) {
  // Try the inactive chain first
  timer_user **p;

  LOCK();

  for (p = &inactive; *p; p = &((*p)->next)) {
    if ((*p)->owner == owner) {
      timer_user *result = *p;
      *p = result->next;
      result->next = NULL;

      UNLOCK();

      return result;
    }
  }

  for (p = &active; *p; p = &((*p)->next)) {
    if ((*p)->owner == owner) {
      timer_user *result = *p;

      bool need_to_reset = (result == active) && result->next;

      if (need_to_reset) {
        adjust_root();
      }

      // Increase the delay on the next element
      if (result->next) {
        result->next->delay += result->delay;
      }

      // Cut out of chain
      *p = result->next;
      result->next = NULL;

      if (need_to_reset) {
        RTC_REG_WRITE(FRC1_LOAD_ADDRESS, active->delay);
        timer_running = 1;
      }

      UNLOCK();
      return result;
    }
  }

  UNLOCK();
  return NULL;
}

static void insert_active_tu(timer_user *tu) {
  timer_user **p;

  LOCK();
  for (p = &active; *p; p = &((*p)->next)) {
    if ((*p)->delay >= tu->delay) {
      break;
    }
    tu->delay -= (*p)->delay;
  }

  if (*p) {
    (*p)->delay -= tu->delay;
  }
  tu->next = *p;
  *p = tu;

  if (*p == active) {
    // We have a new leader
    RTC_REG_WRITE(FRC1_LOAD_ADDRESS, active->delay > 0 ? active->delay : 1);
    timer_running = 1;
  }
  UNLOCK();
}

/******************************************************************************
* FunctionName : platform_hw_timer_arm_ticks
* Description  : set a trigger timer delay for this timer.
* Parameters   : os_param_t owner
*                uint32 ticks :
* Returns      : true if it worked
*******************************************************************************/
bool ICACHE_RAM_ATTR platform_hw_timer_arm_ticks(os_param_t owner, uint32_t ticks)
{
  timer_user *tu = find_tu_and_remove(owner);

  if (!tu) {
    return false;
  }

  tu->delay = ticks;
  tu->autoload_delay = ticks;

  LOCK();
  adjust_root();
  insert_active_tu(tu);
  UNLOCK();

  return true;
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
  return platform_hw_timer_arm_ticks(owner, US_TO_RTC_TIMER_TICKS(microseconds));
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
  timer_user *tu = find_tu(owner);
  if (!tu) {
    return false;
  }
  tu->callback_arg = arg;
  tu->user_hw_timer_cb = user_hw_timer_cb_set;
  return true;
}

static void ICACHE_RAM_ATTR hw_timer_isr_cb(void *arg)
{
  bool keep_going = true;
  adjust_root();
  timer_running = 0;

  while (keep_going && active) {
    keep_going = false;

    timer_user *fired = active;
    active = fired->next;
    if (fired->autoload) {
      fired->delay = fired->autoload_delay;
      insert_active_tu(fired);
      if (active->delay <= 0) {
        keep_going = true;
      }
    } else {
      fired->next = inactive;
      inactive = fired;
      if (active) {
        active->delay += fired->delay;
        if (active->delay <= 0) {
          keep_going = true;
        }
      }
    }
    if (fired->user_hw_timer_cb) {
      fired->cb_count++;
      (*(fired->user_hw_timer_cb))(fired->callback_arg);
    }
  }
  if (active && !timer_running) {
    RTC_REG_WRITE(FRC1_LOAD_ADDRESS, active->delay);
    timer_running = 1;
  }
}

static void ICACHE_RAM_ATTR hw_timer_nmi_cb(void)
{
  hw_timer_isr_cb(NULL);
}

/******************************************************************************
* FunctionName : platform_hw_timer_get_delay_ticks
* Description  : figure out how long since th last timer interrupt
* Parameters   : os_param_t owner
* Returns      : the number of ticks
* This is actually the last timer interrupt and not the interrupt for your
* timer. This may cause problems.
*******************************************************************************/
uint32_t ICACHE_RAM_ATTR platform_hw_timer_get_delay_ticks(os_param_t owner)
{
  return 0;
  //return (- RTC_REG_READ(FRC1_COUNT_ADDRESS)) & ((1 << 23) - 1);
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
  timer_user *tu = find_tu_and_remove(owner);

  if (!tu) {
    tu = (timer_user *) c_malloc(sizeof(*tu));
    if (!tu) {
      return false;
    }
    memset(tu, 0, sizeof(*tu));
    tu->owner = owner;
  }

  tu->autoload = autoload;

  if (!active && !inactive) {
    RTC_REG_WRITE(FRC1_CTRL_ADDRESS,
		  DIVIDED_BY_16 | FRC1_ENABLE_TIMER | TM_EDGE_INT);
    ETS_FRC_TIMER1_INTR_ATTACH(hw_timer_isr_cb, NULL);

    TM1_EDGE_INT_ENABLE();
    ETS_FRC1_INTR_ENABLE();
  }

  LOCK();
  tu->next = inactive;
  inactive = tu;
  UNLOCK();

  return true;
}

/******************************************************************************
* FunctionName : platform_hw_timer_close
* Description  : ends use of the hardware isr timer
* Parameters   : os_param_t owner
* Returns      : true if it worked
*******************************************************************************/
bool ICACHE_RAM_ATTR platform_hw_timer_close(os_param_t owner)
{
  timer_user *tu = find_tu_and_remove(owner);

  if (tu) {
    LOCK();
    tu->next = inactive;
    inactive = tu;
    UNLOCK();
  }

  // This will never actually run....
  if (!active && !inactive) {
    /* Set no reload mode */
    RTC_REG_WRITE(FRC1_CTRL_ADDRESS,
                  DIVIDED_BY_16 | TM_EDGE_INT);

    TM1_EDGE_INT_DISABLE();
    ETS_FRC1_INTR_DISABLE();
  }

  return true;
}

