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

//#define DEBUG_HW_TIMER
//#undef NODE_DBG
//#define NODE_DBG dbg_printf

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

/*
 * This represents a single user of the timer functionality. It is keyed by the owner
 * field.
 */
typedef struct _timer_user {
  struct _timer_user *next;
  bool autoload;
  int32_t delay; // once on the active list, this is difference in delay from the preceding element
  int32_t autoload_delay;
  uint32_t expected_interrupt_time;
  os_param_t owner;
  os_param_t callback_arg;
  void (* user_hw_timer_cb)(os_param_t);
#ifdef DEBUG_HW_TIMER
  int cb_count;
#endif
} timer_user;

/*
 * There are two lists of timer_user blocks. The active list are those which are waiting
 * for timeouts to happen, and the inactive list contains idle blocks. Unfortunately
 * there isn't a way to clean up the inactive blocks as some modules call the
 * close method from interrupt level.
 */
static timer_user *active;
static timer_user *inactive;

/*
 * There are a fair number of places when interrupts need to be disabled as many of
 * the methods can be called from interrupt level. The lock/unlock calls support
 * multiple LOCKs and then the same number of UNLOCKs are required to re-enable
 * interrupts. This is imolemeted by counting the number of times that lock is called.
 */
static uint8_t lock_count;
static uint8_t timer_running;

static uint32_t time_next_expiry;
static int32_t last_timer_load;

#define LOCK()   do { ets_intr_lock(); lock_count++; } while (0)
#define UNLOCK()   if (--lock_count == 0) ets_intr_unlock()

/*
 * To start a timer, you write to FRCI_LOAD_ADDRESS, and that starts the counting
 * down. When it reaches zero, the interrupt fires -- but the counting continues.
 * The counter is 23 bits wide. The current value of the counter can be read
 * at FRC1_COUNT_ADDRESS. The unit is 200ns, and so it takes somewhat over a second
 * to wrap the counter.
 */

#ifdef DEBUG_HW_TIMER
void ICACHE_RAM_ATTR hw_timer_debug() {
  dbg_printf("timer_running=%d\n", timer_running);
  timer_user *tu;
  for (tu = active; tu; tu = tu->next) {
    dbg_printf("Owner: 0x%x, delay=%d, autoload=%d, autoload_delay=%d, cb_count=%d\n",
        tu->owner, tu->delay, tu->autoload, tu->autoload_delay, tu->cb_count);
  }
}
#endif

static void ICACHE_RAM_ATTR set_timer(int delay, const char *caller) {
  if (delay < 1) {
    delay = 1;
  }
  int32_t time_left = (RTC_REG_READ(FRC1_COUNT_ADDRESS)) & ((1 << 23) - 1);
  RTC_REG_WRITE(FRC1_LOAD_ADDRESS, delay);

  if (time_left > last_timer_load) {
    // We have missed the interrupt
    time_left -= 1 << 23;
  }
  NODE_DBG("%s(%x): time_next=%d, left=%d (load=%d), delay=%d  => %d\n", caller, active->owner, time_next_expiry, time_left, last_timer_load, delay, time_next_expiry - time_left + delay);
  time_next_expiry = time_next_expiry - time_left + delay;
  last_timer_load = delay;

  timer_running = 1;
}

static void ICACHE_RAM_ATTR adjust_root() {
  // Can only ge called with interrupts disabled
  // change the initial active delay so that relative stuff still works
  // Also, set the last_timer_load to be now
  int32_t time_left = (RTC_REG_READ(FRC1_COUNT_ADDRESS)) & ((1 << 23) - 1);
  if (time_left > last_timer_load) {
    // We have missed the interrupt
    time_left -= 1 << 23;
  }

  if (active && timer_running) {
    active->delay = time_left;
  }

  if (active) {
    NODE_DBG("adjust(%x): time_left=%d (last_load=%d)\n", active->owner, time_left, last_timer_load);
  } else {
    NODE_DBG("adjust: time_left=%d (last_load=%d)\n", time_left, last_timer_load);
  }

  last_timer_load = time_left;
}
/*
 * Find the timer_user block for this owner. This just returns
 * a pointer to the block, or NULL.
 */
static timer_user * ICACHE_RAM_ATTR find_tu(os_param_t owner) {
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

/*
 * Find the timer_user block for this owner. This just returns
 * a pointer to the block, or NULL. If it finds the block, then it is
 * removed from whichever chain it is on. Note that this may require
 * triggering a timer.
 */
static timer_user * ICACHE_RAM_ATTR find_tu_and_remove(os_param_t owner) {
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
        set_timer(active->delay, "find_tu");
      }

      UNLOCK();
      return result;
    }
  }

  UNLOCK();
  return NULL;
}

/*
 * This inserts a timer_user block into the active chain. This is a sightly
 * complex process as it can involve triggering a timer load.
 */
static void ICACHE_RAM_ATTR insert_active_tu(timer_user *tu) {
  timer_user **p;

  LOCK();

  tu->expected_interrupt_time = time_next_expiry - last_timer_load + tu->delay;

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

  if (tu == active) {
    // We have a new leader
    set_timer(active->delay, "insert_active");
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

  NODE_DBG("arm(%x): ticks=%d\n", owner, ticks);

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
  NODE_DBG("set-CB(%x): %x, %x\n", tu->owner, tu->user_hw_timer_cb, tu->callback_arg);
  return true;
}

/*
 * This is the timer ISR. It has to find the timer that was running and trigger the callback
 * for that timer. By this stage, the next timer may have expired as well, and so the process
 * iterates. Note that if there is an autoload timer, then it should be restarted immediately.
 * Also, the callbacks typically do re-arm the timer, so we have to be careful not to
 * assume that nothing changes during the callback.
 */
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
      fired->expected_interrupt_time += fired->autoload_delay;
      fired->delay = fired->expected_interrupt_time - (time_next_expiry - last_timer_load);
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
#ifdef DEBUG_HW_TIMER
      fired->cb_count++;
#endif
      NODE_DBG("CB(%x): %x, %x\n", fired->owner, fired->user_hw_timer_cb, fired->callback_arg);
      (*(fired->user_hw_timer_cb))(fired->callback_arg);
    }
  }
  if (active && !timer_running) {
    set_timer(active->delay, "isr");
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
*******************************************************************************/
uint32_t ICACHE_RAM_ATTR platform_hw_timer_get_delay_ticks(os_param_t owner)
{
  timer_user *tu = find_tu(owner);
  if (!tu) {
    return 0;
  }

  LOCK();
  adjust_root();
  UNLOCK();
  int ret = (time_next_expiry - last_timer_load) - tu->expected_interrupt_time;

  if (ret < 0) {
    NODE_DBG("delay ticks = %d, last_timer_load=%d, tu->expected_int=%d, next_exp=%d\n", ret, last_timer_load, tu->expected_interrupt_time, time_next_expiry);
  }

  return ret < 0 ? 0 : ret;
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

