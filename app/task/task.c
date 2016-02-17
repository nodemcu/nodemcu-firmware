/**
  This file encapsulates the SDK-based task handling for the NodeMCU Lua firmware.
 */
#include "task/task.h"
#include "mem.h"
#include "c_stdio.h"

#define TASK_HANDLE_MONIKER 0x68680000
#define TASK_HANDLE_MASK    0xFFF80000
#define TASK_HANDLE_UNMASK  (~TASK_HANDLE_MASK)
#define TASK_HANDLE_SHIFT   2
#define TASK_HANDLE_ALLOCATION_BRICK 4   // must be a power of 2
#define TASK_DEFAULT_QUEUE_LEN 8
#define TASK_PRIORITY_MASK  3

#define CHECK(p,v,msg) if (!(p)) { NODE_DBG ( msg ); return (v); }

/*
 * Private arrays to hold the 3 event task queues and the dispatch callbacks
 */
LOCAL os_event_t *task_Q[TASK_PRIORITY_COUNT];
LOCAL task_callback_t *task_func;
LOCAL int task_count;

LOCAL void task_dispatch (os_event_t *e) {
  task_handle_t handle = e->sig;
  if ( (handle & TASK_HANDLE_MASK) == TASK_HANDLE_MONIKER) {
    uint16 entry    = (handle & TASK_HANDLE_UNMASK) >> TASK_HANDLE_SHIFT;
    uint8  priority = handle & TASK_PRIORITY_MASK;
    if ( priority <= TASK_PRIORITY_HIGH && task_func && entry < task_count ){
      /* call the registered task handler with the specified parameter and priority */
      task_func[entry](e->par, priority);
      return;
    }
  }
  /* Invalid signals are ignored */
  NODE_DBG ( "Invalid signal issued: %08x",  handle);
}

/*
 * Initialise the task handle callback for a given priority.  This doesn't need
 * to be called explicitly as the get_id function will call this lazily.
 */
bool task_init_handler(uint8 priority, uint8 qlen) {
  if (priority <= TASK_PRIORITY_HIGH && task_Q[priority] == NULL) {
    task_Q[priority] = (os_event_t *) os_malloc( sizeof(os_event_t)*qlen );
    os_memset (task_Q[priority], 0, sizeof(os_event_t)*qlen);
    if (task_Q[priority]) {
      return system_os_task( task_dispatch, priority, task_Q[priority], qlen );
    }
  }
  return false;
}

task_handle_t task_get_id(task_callback_t t) {
  int p = TASK_PRIORITY_COUNT;
  /* Initialise and uninitialised Qs with the default Q len */
    while(p--) if (!task_Q[p]) {
    CHECK(task_init_handler( p, TASK_DEFAULT_QUEUE_LEN ), 0, "Task initialisation failed");
  }

  if ( (task_count & (TASK_HANDLE_ALLOCATION_BRICK - 1)) == 0 ) {
    /* With a brick size of 4 this branch is taken at 0, 4, 8 ... and the new size is +4 */
    task_func =(task_callback_t *) os_realloc(task_func,
                        sizeof(task_callback_t)*(task_count+TASK_HANDLE_ALLOCATION_BRICK));
    CHECK(task_func, 0 , "Malloc failure in task_get_id");
    os_memset (task_func+task_count, 0, sizeof(task_callback_t)*TASK_HANDLE_ALLOCATION_BRICK);
  }

  task_func[task_count++] = t;
  return TASK_HANDLE_MONIKER + ((task_count-1)  << TASK_HANDLE_SHIFT);
}
