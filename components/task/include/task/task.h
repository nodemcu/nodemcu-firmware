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

/* Regular task posting is non-blocking, best effort. This version can be used
 * where blocking-until-slot-is-available functionality is needed.
 */
bool task_post_block(task_prio_t priority, task_handle_t handle, task_param_t param);

/* When posting NodeMCU tasks from an ISR, this version MUST be used,
 * and vice versa.
 * Doing otherwise breaks assumptions made by the FreeRTOS kernel in terms of
 * concurrency safety, and may lead to obscure and hard-to-pinpoint bugs.
 * While a specific hardware port may be implemented in a manner which makes
 * it concurrency safe to call FreeRTOS xxxFromISR functions from any context,
 * doing so would still lead to scheduling oddities as the xxxFromISR
 * functions do not perform task switching whereas the non-FromISR versions
 * do. In practical terms, that means that even if a higher priority FreeRTOS
 * task was unblocked, it would not get scheduled at that point.
 */
bool task_post_isr(task_prio_t priority, task_handle_t handle, task_param_t param);

#define task_post_low(handle,param)    task_post(TASK_PRIORITY_LOW,    handle, param)
#define task_post_medium(handle,param) task_post(TASK_PRIORITY_MEDIUM, handle, param)
#define task_post_high(handle,param)   task_post(TASK_PRIORITY_HIGH,   handle, param)

#define task_post_isr_low(handle,param)    task_post_isr(TASK_PRIORITY_LOW,    handle, param)
#define task_post_isr_medium(handle,param) task_post_isr(TASK_PRIORITY_MEDIUM, handle, param)
#define task_post_isr_high(handle,param)   task_post_isr(TASK_PRIORITY_HIGH,   handle, param)

#define task_post_block_low(handle,param)    task_post_block(TASK_PRIORITY_LOW,    handle, param)
#define task_post_block_medium(handle,param) task_post_block(TASK_PRIORITY_MEDIUM, handle, param)
#define task_post_block_high(handle,param)   task_post_block(TASK_PRIORITY_HIGH,   handle, param)

typedef void (*task_callback_t)(task_param_t param, task_prio_t prio);

task_handle_t task_get_id(task_callback_t t);

/* Init, must be called before any posting happens */
void task_init (void);

/* RTOS loop to pump task messages until infinity */
void task_pump_messages (void);

#endif

