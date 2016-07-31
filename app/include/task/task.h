#ifndef _TASK_H_
#define _TASK_H_

#include "ets_sys.h"
#include "osapi.h"
#include "os_type.h"
#include "user_interface.h"

/* use LOW / MEDIUM / HIGH since it isn't clear from the docs which is higher */

#define TASK_PRIORITY_LOW     0
#define TASK_PRIORITY_MEDIUM  1
#define TASK_PRIORITY_HIGH    2
#define TASK_PRIORITY_COUNT   3

/*
* Signals are a 32-bit number of the form header:14; count:16, priority:2.  The header
* is just a fixed fingerprint and the count is allocated serially by the task get_id()
* function.
*/
#define task_post(priority,handle,param) system_os_post(priority, ((handle) | priority), param)
#define task_post_low(handle,param)    task_post(TASK_PRIORITY_LOW,    handle, param)
#define task_post_medium(handle,param) task_post(TASK_PRIORITY_MEDIUM, handle, param)
#define task_post_high(handle,param)   task_post(TASK_PRIORITY_HIGH,   handle, param)

#define task_handle_t os_signal_t
#define task_param_t os_param_t

typedef void (*task_callback_t)(task_param_t param, uint8 prio);

bool task_init_handler(uint8 priority, uint8 qlen);
task_handle_t task_get_id(task_callback_t t);

#endif

