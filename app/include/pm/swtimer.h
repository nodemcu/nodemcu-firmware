/*
 * swtimer.h
 *
 *  Created on: Aug 4, 2017
 *      Author: anonymous
 */

#ifndef APP_INCLUDE_PM_SWTIMER_H_
#define APP_INCLUDE_PM_SWTIMER_H_

void swtmr_cb_register(void* timer_cb_ptr, uint8 suspend_policy);

#define SWTIMER_RESUME    0 //save remaining time
#define SWTIMER_RESTART   1 //use timer_period as remaining time
#define SWTIMER_IMMEDIATE 2 //fire timer immediately after resume
#define SWTIMER_DROP      3 //disarm timer, do not resume

#if defined(TIMER_SUSPEND_ENABLE)
#define SWTIMER_REG_CB(cb_ptr, suspend_policy) do{ \
    static bool cb_ptr##_registered_flag;\
    if(!cb_ptr##_registered_flag){ \
      cb_ptr##_registered_flag = true; \
      swtmr_cb_register(cb_ptr, suspend_policy);\
    } \
}while(0);
#else
#define SWTIMER_REG_CB(...)
#endif

#endif /* APP_INCLUDE_PM_SWTIMER_H_ */
