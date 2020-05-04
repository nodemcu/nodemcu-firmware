/**
  This file encapsulates the SDK-based task handling for the NodeMCU Lua firmware.
 */
#include "task/task.h"
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

typedef struct
{
  task_handle_t handle;
  task_param_t par;
} task_event_t;

/*
 * Private arrays to hold the 3 event task queues and the dispatch callbacks
 */
static xQueueHandle task_Q[TASK_PRIORITY_COUNT];

/* Rather than using a QueueSet (which requires queues to be empty when created)
 * we use a binary semaphore to unblock the pump whenever something is posted */
static xSemaphoreHandle pending;

bool IRAM_ATTR task_post (task_prio_t priority, task_handle_t handle, task_param_t param)
{
  if (priority >= TASK_PRIORITY_COUNT)
    return false;

  task_event_t ev = { handle, param };
  bool res = pdPASS == xQueueSendToBackFromISR (task_Q[priority], &ev, NULL);

  xSemaphoreGiveFromISR (pending, NULL);

  return res;
}


static bool next_event (task_event_t *ev, task_prio_t *prio)
{
  for (task_prio_t pr = TASK_PRIORITY_COUNT; pr != TASK_PRIORITY_LOW; --pr)
  {
    task_prio_t p = pr -1;
    if (task_Q[p] && xQueueReceive (task_Q[p], ev, 0) == pdTRUE)
    {
      *prio = p;
      return true;
    }
  }
  return false; // no events queued
}


static void dispatch (task_event_t *e, uint8_t prio) {
    ((task_callback_t)e->handle)(e->par, prio);
}


void task_init (void)
{
  pending = xSemaphoreCreateBinary ();

  // Due to the nature of the RTOS, if we ever fail to do a task_post, we're
  // up the proverbial creek without a paddle. Each queue slot only costs us
  // 8 bytes, so it's a worthwhile trade-off to reserve a bit of RAM for this
  // purpose. Trying to work around the issue in the places which post ends
  // up being *at least* as bad, so better take the hit where the benefit
  // is shared. That said, we let the user have the final say.
  const size_t q_mem = CONFIG_NODEMCU_TASK_SLOT_MEMORY;

  // Rather than blindly sizing everything the same, we try to be a little
  // bit aware of the typical uses of the queues.
  const size_t slice = q_mem / (10 * sizeof(task_event_t));
  static const int qlens[] = { 1 * slice, 5 * slice, 4 * slice };

  for (task_prio_t p = TASK_PRIORITY_LOW; p != TASK_PRIORITY_COUNT; ++p)
    task_Q[p] = xQueueCreate (qlens[p], sizeof (task_event_t));
}


void task_pump_messages (void)
{
  for (;;)
  {
    task_event_t ev;
    task_prio_t prio;
    if (next_event (&ev, &prio))
      dispatch (&ev, prio);
    else
      xSemaphoreTake (pending, portMAX_DELAY);
  }
}
