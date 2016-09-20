#ifndef _TASK_H_
#define _TASK_H_

#include <stdint.h>
#include <stdbool.h>

/* use LOW / MEDIUM / HIGH since it isn't clear from the docs which is higher */

typedef enum {
  TASK_PRIORITY_LOW,
  TASK_PRIORITY_MEDIUM,
  TASK_PRIORITY_HIGH,
  TASK_PRIORITY_COUNT
} task_prio_t;

typedef uint32_t task_handle_t;
typedef intptr_t task_param_t;

/*
* Signals are a 32-bit number of the form header:14; count:18. The header
* is just a fixed fingerprint and the count is allocated serially by the
* task_get_id() function.
*/
bool task_post(task_prio_t priority, task_handle_t handle, task_param_t param);

#define task_post_low(handle,param)    task_post(TASK_PRIORITY_LOW,    handle, param)
#define task_post_medium(handle,param) task_post(TASK_PRIORITY_MEDIUM, handle, param)
#define task_post_high(handle,param)   task_post(TASK_PRIORITY_HIGH,   handle, param)

typedef void (*task_callback_t)(task_param_t param, task_prio_t prio);

bool task_init_handler(task_prio_t priority, uint8 qlen);
task_handle_t task_get_id(task_callback_t t);

/* RTOS loop to pump task messages until infinity */
void task_pump_messages (void);

#endif

