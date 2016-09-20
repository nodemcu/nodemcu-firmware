/**
  This file encapsulates the SDK-based task handling for the NodeMCU Lua firmware.
 */
#include "task/task.h"
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

#define TASK_HANDLE_MONIKER 0x68680000
#define TASK_HANDLE_MASK    0xFFF80000
#define TASK_HANDLE_UNMASK  (~TASK_HANDLE_MASK)
#define TASK_HANDLE_ALLOCATION_BRICK 4   // must be a power of 2
#define TASK_DEFAULT_QUEUE_LEN 8

#define CHECK(p,v,msg) if (!(p)) { NODE_DBG ( msg ); return (v); }

#ifndef NODE_DBG
# define NODE_DBG(...) do{}while(0)
#endif

typedef struct
{
  task_handle_t sig;
  task_param_t par;
} task_event_t;

/*
 * Private arrays to hold the 3 event task queues and the dispatch callbacks
 */
static xQueueHandle task_Q[TASK_PRIORITY_COUNT];

/* Rather than using a QueueSet (which requires queues to be empty when created)
 * we use a binary semaphore to unblock the pump whenever something is posted */
static xSemaphoreHandle pending;

static task_callback_t *task_func;
static int task_count;


/*
 * Initialise the task handle callback for a given priority.  This doesn't need
 * to be called explicitly as the get_id function will call this lazily.
 */
bool task_init_handler(task_prio_t priority, uint8 qlen) {
  if (priority >= TASK_PRIORITY_COUNT)
    return false;

  if (task_Q[priority] == NULL)
  {
    task_Q[priority] = xQueueCreate (qlen, sizeof (task_event_t));
    return task_Q[priority] != NULL;
  }
  else
    return false;
}


task_handle_t task_get_id(task_callback_t t) {
  /* Initialise any uninitialised Qs with the default Q len */
  for (task_prio_t p = TASK_PRIORITY_LOW; p != TASK_PRIORITY_COUNT; ++p)
  {
    if (!task_Q[p]) {
      CHECK(task_init_handler( p, TASK_DEFAULT_QUEUE_LEN ), 0, "Task initialisation failed");
    }
  }

  if ( (task_count & (TASK_HANDLE_ALLOCATION_BRICK - 1)) == 0 ) {
    /* With a brick size of 4 this branch is taken at 0, 4, 8 ... and the new size is +4 */
    task_func =(task_callback_t *)realloc(
      task_func,
      sizeof(task_callback_t)*(task_count+TASK_HANDLE_ALLOCATION_BRICK));

    CHECK(task_func, 0 , "Malloc failure in task_get_id");
    memset (task_func+task_count, 0, sizeof(task_callback_t)*TASK_HANDLE_ALLOCATION_BRICK);
  }

  task_func[task_count] = t;
  return TASK_HANDLE_MONIKER | task_count++;
}


bool task_post (task_prio_t priority, task_handle_t handle, task_param_t param)
{
  if (priority >= TASK_PRIORITY_COUNT ||
      !task_Q[priority] ||
      (handle & TASK_HANDLE_MASK) != TASK_HANDLE_MONIKER)
    return false;

  task_event_t ev = { handle, param };
  bool res = pdPASS == xQueueSendToBackFromISR (task_Q[priority], &ev, NULL);

  if (pending) /* only need to raise semaphore if it's been initialised */
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
  task_handle_t handle = e->sig;
  if ( (handle & TASK_HANDLE_MASK) == TASK_HANDLE_MONIKER) {
    uint16_t entry = (handle & TASK_HANDLE_UNMASK);
    if ( task_func && entry < task_count ){
      /* call the registered task handler with the specified parameter and priority */
      task_func[entry](e->par, prio);
      return;
    }
  }
  /* Invalid signals are ignored */
  NODE_DBG ( "Invalid signal issued: %08x",  handle);
}


void task_pump_messages (void)
{
  vSemaphoreCreateBinary (pending);
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
