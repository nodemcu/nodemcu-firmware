/* swTimer.c SDK timer suspend API
 *
 * SDK software timer API info:
 *
 * The SDK software timer API executes in a task. The priority of this task in relation to the
 * application level tasks is unknown.
 *
 * The SDK software timer uses a linked list called `os_timer_t* timer_list` to keep track of
 * all currently armed timers.
 *
 * To determine when a timer's callback should be executed, the respective timer's `timer_expire`
 *  variable is compared to the hardware counter(FRC2), then, if the timer's `timer_expire` is
 *  less than the current FRC2 count, the timer's callback is fired.
 *
 * The timers in this list are organized in an ascending order starting with the timer
 * with the lowest timer_expire.
 *
 * When a timer expires that has a timer_period greater than 0, timer_expire is changed to
 * current FRC2 + timer_period, then the timer is inserted back in to the list in the correct position.
 *
 *
 *
 * TIMER SUSPEND API:
 *
 *  Timer registry:
 * void sw_timer_register(void* timer_ptr);
 * - Adds timers to the timer registry by adding it to a queue that is later
 *   processed by timer_register_task that performs the registry maintenance
 *
 * void sw_timer_unregister(void* timer_ptr);
 * - Removes timers from the timer registry by adding it to a queue that is later
 *   processed by timer_unregister_task that performs the registry maintenance
 *
 *
 * int sw_timer_suspend(os_timer_t* timer_ptr);
 * - Suspend a single active timer or suspend all active timers.
 * - if no timer pointer is provided, timer_ptr == NULL, then all currently active timers will be suspended.
 *
 * int sw_timer_resume(os_timer_t* timer_ptr);
 * - Resume a single suspended timer or resume all suspended timers.
 * - if no timer pointer is provided, timer_ptr == NULL, then all currently suspended timers will be resumed.
 *
 *
 *
 */


#include "swTimer/swTimer.h"
#include "c_stdio.h"
#include "misc/dynarr.h"
#include "task/task.h"

/*      Settings      */
#define TIMER_REGISTRY_INITIAL_SIZE 10
#ifdef USE_SWTMR_ERROR_STRINGS
static const char* SWTMR_ERROR_STRINGS[]={
    [SWTMR_MALLOC_FAIL] = "Out of memory!",
    [SWTMR_TIMER_NOT_ARMED] = "Timer is not armed",
    [SWTMR_NULL_PTR] = "A NULL pointer was passed to timer suspend api ",

    [SWTMR_REGISTRY_NO_REGISTERED_TIMERS] = "No timers in registry",

    [SWTMR_SUSPEND_ARRAY_INITIALIZATION_FAILED] = "Suspend array init fail",
    [SWTMR_SUSPEND_ARRAY_ADD_FAILED] = "Unable to add suspended timer to array",
    [SWTMR_SUSPEND_ARRAY_REMOVE_FAILED] = "Unable to remove suspended timer from array",
    [SWTMR_SUSPEND_TIMER_ALREADY_SUSPENDED] = "Timer already suspended",
    [SWTMR_SUSPEND_TIMER_ALREADY_REARMED] = "Timer has already been re-armed",
    [SWTMR_SUSPEND_NO_SUSPENDED_TIMERS] = "No suspended timers available",
    [SWTMR_SUSPEND_TIMER_NOT_SUSPENDED] = "Timer is not suspended",

};
#endif

/*     Private Function Declarations      */
static inline bool timer_armed_check(os_timer_t* timer_ptr);
static inline int timer_do_suspend(os_timer_t* timer_ptr);
static inline int timer_do_resume_single(os_timer_t** suspended_timer_ptr);
static void timer_register_task(task_param_t param, uint8 priority);
static inline os_timer_t** timer_registry_check(os_timer_t* timer_ptr);
static inline void timer_registry_remove_unarmed(void);
static inline os_timer_t** timer_suspended_check(os_timer_t* timer_ptr);
static void timer_unregister_task(task_param_t param, uint8 priority);

/*      Private Variable Definitions      */
  static task_handle_t timer_reg_task_id = false;
  static task_handle_t timer_unreg_task_id = false;

  static dynarr_t timer_registry = {0};
  static dynarr_t suspended_timers = {0};

  typedef struct registry_queue{
    struct registry_queue* next;
    os_timer_t* timer_ptr;
  }registry_queue_t;

  static registry_queue_t* register_queue = NULL;
  static registry_queue_t* unregister_queue = NULL;

/*      Private Function Definitions     */

//NOTE: Interrupts are temporarily blocked during the execution of this function
static inline bool timer_armed_check(os_timer_t* timer_ptr){
  bool retval=FALSE;

  ETS_INTR_LOCK();

  os_timer_t* timer_list_ptr=timer_list;

  while(timer_list_ptr != NULL){
    if(timer_list_ptr == timer_ptr){
      retval = TRUE;
      break;
    }
    timer_list_ptr=timer_list_ptr->timer_next;
  }

  ETS_INTR_UNLOCK();

  return retval;
}

static inline int timer_do_suspend(os_timer_t* timer_ptr){
  if(timer_ptr == NULL){
    SWTMR_DBG("timer_ptr is NULL");
    return SWTMR_NULL_PTR;
  }
  volatile uint32 frc2_count = RTC_REG_READ(FRC2_COUNT_ADDRESS);

  if(timer_armed_check(timer_ptr) == FALSE){
    return SWTMR_TIMER_NOT_ARMED;
  }

  if(timer_suspended_check(timer_ptr) != NULL){
    return SWTMR_SUSPEND_TIMER_ALREADY_SUSPENDED;
  }

  uint32 expire_temp = 0;
  uint32 period_temp = timer_ptr->timer_period;


  if(timer_ptr->timer_expire < frc2_count)
    expire_temp = 6250; // 20 ms in ticks (1ms = 312.5 ticks)
  else
    expire_temp = timer_ptr->timer_expire - frc2_count;

  ets_timer_disarm(timer_ptr);
  timer_unregister_task((task_param_t)timer_ptr, false);

  timer_ptr->timer_expire = expire_temp;
  timer_ptr->timer_period = period_temp;

  if(suspended_timers.data_ptr == NULL){
    if(!dynarr_init(&suspended_timers, 10, sizeof(os_timer_t*))){
      return SWTMR_SUSPEND_ARRAY_INITIALIZATION_FAILED;
    }
  }

  if(!dynarr_add(&suspended_timers, &timer_ptr, sizeof(timer_ptr))){
    return SWTMR_SUSPEND_ARRAY_ADD_FAILED;
  }

  return SWTMR_OK;
}

//NOTE: Interrupts are temporarily blocked during the execution of this function
static inline int timer_do_resume_single(os_timer_t** suspended_timer_ptr){
  if(suspended_timer_ptr == NULL){
    SWTMR_DBG("suspended_timer_ptr is NULL");
    return SWTMR_NULL_PTR;
  }

  os_timer_t* timer_list_ptr = NULL;
  os_timer_t* resume_timer_ptr = *suspended_timer_ptr;
  volatile uint32 frc2_count=RTC_REG_READ(FRC2_COUNT_ADDRESS);

  //verify timer has not been rearmed
  if(timer_armed_check(resume_timer_ptr) == TRUE){
    SWTMR_DBG("Timer(%p) already rearmed, removing from array", resume_timer_ptr);
    dynarr_remove(&suspended_timers, suspended_timer_ptr);
    return SWTMR_SUSPEND_TIMER_ALREADY_REARMED;
  }

  //Prepare timer for resume
  resume_timer_ptr->timer_expire += frc2_count;

  timer_register_task((task_param_t)resume_timer_ptr, false);
  SWTMR_DBG("Removing timer(%p) from suspend array", resume_timer_ptr);

  if(!dynarr_remove(&suspended_timers, suspended_timer_ptr)){
    return SWTMR_SUSPEND_ARRAY_REMOVE_FAILED;
  }

  //This section performs the actual resume of the suspended timer

  //block interrupts since the SDK software timer linked list will be modified
  ETS_INTR_LOCK();

  timer_list_ptr = timer_list;

  while(timer_list_ptr != NULL){
    if(resume_timer_ptr->timer_expire > timer_list_ptr->timer_expire){

      if(timer_list_ptr->timer_next!=NULL){
        if(resume_timer_ptr->timer_expire < timer_list_ptr->timer_next->timer_expire){
          resume_timer_ptr->timer_next=timer_list_ptr->timer_next;
          timer_list_ptr->timer_next=resume_timer_ptr;
          break;
        }
        else{
          //next timer in timer_list
        }
      }
      else{
        timer_list_ptr->timer_next=resume_timer_ptr;
        resume_timer_ptr->timer_next=NULL;
        break;
      }
    }
    else if(timer_list_ptr == timer_list){
      resume_timer_ptr->timer_next=timer_list_ptr;
      timer_list = timer_list_ptr = resume_timer_ptr;
      break;
  }

    timer_list_ptr = timer_list_ptr->timer_next;
  }

  //we no longer need to block interrupts
  ETS_INTR_UNLOCK();

  return SWTMR_OK;
}

static void timer_register_task(task_param_t param, uint8 priority){
  if(timer_registry.data_ptr==NULL){
    if(!dynarr_init(&timer_registry, TIMER_REGISTRY_INITIAL_SIZE, sizeof(os_timer_t*))){
      //timer registry init Fail!
      return;
    }
  }
  os_timer_t* timer_ptr=NULL;

  if(param != false){
    timer_ptr=(os_timer_t*)param;
  }
  else{
    if(register_queue==NULL){
      /**/SWTMR_ERR("ERROR: REGISTER QUEUE EMPTY");
      return;
    }
      registry_queue_t* queue_temp=register_queue;
      register_queue = register_queue->next;

      timer_ptr = queue_temp->timer_ptr;

      c_free(queue_temp);

      if(register_queue != NULL){
//        SWTMR_DBG("register_queue not empty, posting task");
        task_post_low(timer_reg_task_id, false);
      }
  }

  if(timer_registry_check(timer_ptr) != NULL){
    /**/SWTMR_DBG("timer(%p) found in registry, returning", timer_ptr);
    return;
  }

//  SWTMR_DBG("adding timer(%p) to registry", timer_ptr);

  if(!dynarr_add(&timer_registry, &timer_ptr, sizeof(timer_ptr))){
    /**/SWTMR_ERR("Registry append failed");
    return;
  }

  return;
}

static inline os_timer_t** timer_registry_check(os_timer_t* timer_ptr){
  if(timer_registry.data_ptr==NULL){
    return NULL;
  }
  if(timer_registry.used > 0){

    os_timer_t** timer_registry_array = timer_registry.data_ptr;

    for(int i=0; i < timer_registry.used; i++){
      if(timer_registry_array[i] == timer_ptr){
        /**/SWTMR_DBG("timer(%p) is registered", timer_registry_array[i]);
        return &timer_registry_array[i];
      }
    }
  }

  return NULL;
}

static inline void timer_registry_remove_unarmed(void){
  if(timer_registry.data_ptr==NULL){
    return;
  }
  if(timer_registry.used > 0){
    os_timer_t** timer_registry_array = timer_registry.data_ptr;
    for(int i=0; i < timer_registry.used; i++){
      if(timer_armed_check(timer_registry_array[i]) == FALSE){
        timer_unregister_task((task_param_t)timer_registry_array[i], false);
      }
    }
  }
}

static inline os_timer_t** timer_suspended_check(os_timer_t* timer_ptr){
  if(suspended_timers.data_ptr==NULL){
    return NULL;
  }
  if(suspended_timers.used > 0){

    os_timer_t** suspended_timer_array = suspended_timers.data_ptr;

    for(int i=0; i < suspended_timers.used; i++){
      if(suspended_timer_array[i] == timer_ptr){
        return &suspended_timer_array[i];
      }
    }
  }

  return NULL;
}

static void timer_unregister_task(task_param_t param, uint8 priority){
  if(timer_registry.data_ptr==NULL){
    return;
  }
  os_timer_t* timer_ptr=NULL;

  if(param != false){
    timer_ptr=(os_timer_t*)param;
  }
  else{

    if(unregister_queue == NULL) {
      SWTMR_ERR("ERROR: REGISTER QUEUE EMPTY");
      return;
    }
      registry_queue_t* queue_temp = unregister_queue;
      timer_ptr = queue_temp->timer_ptr;
      unregister_queue = unregister_queue->next;
      c_free(queue_temp);
      if(unregister_queue != NULL){
//        SWTMR_DBG("unregister_queue not empty, posting task");
        task_post_low(timer_unreg_task_id, false);
      }
  }
  if(timer_armed_check(timer_ptr) == TRUE){
    return;
  }


  os_timer_t** registry_ptr=timer_registry_check(timer_ptr);
  if(registry_ptr!=NULL){
    if(!dynarr_remove(&timer_registry, registry_ptr)){
      /**/SWTMR_DBG("unable to remove timer(%p) from registry", timer_ptr);
      //timer remove FAIL
    }
  }
  else{
    //timer not in registry
  }
  return;
}

/*      Global Function Definitions      */

#if defined(SWTMR_DEBUG)

void swtmr_print_registry(void){
  volatile uint32 frc2_count = RTC_REG_READ(FRC2_COUNT_ADDRESS);
  uint32 time_till_fire=0;
  uint32 time=system_get_time();

  timer_registry_remove_unarmed();
  time=system_get_time()-time;

  /**/SWTMR_DBG("registry_remove_unarmed_timers() took %u us", time);

  os_timer_t** timer_array=timer_registry.data_ptr;

  c_printf("\n  array used(%u)/size(%u)\ttotal size(bytes)=%u\n FRC2 COUNT %u (in ms)\n",
      timer_registry.used, timer_registry.array_size, timer_registry.array_size * timer_registry.data_size, (uint32)(frc2_count/312.5));

  for(int i=0;i<timer_registry.used;i++){
     time_till_fire=(timer_array[i]->timer_expire/312.5)-(frc2_count/312.5);
     c_printf("\ttimer_registry[%u]=%p\ttimer_expire:%u\ttimer_period:%u\ttime till fire:%u\n", i, timer_array[i], (uint32)(timer_array[i]->timer_expire/312.5), (uint32)(timer_array[i]->timer_period/312.5), time_till_fire);
   }

  return;
}

void swtmr_print_suspended(void){
  uint32 period, time_left;
  os_timer_t** susp_timer_array = suspended_timers.data_ptr;

  c_printf("\n array used(%u)/size(%u)\ttotal size(bytes)=%u\n",
      suspended_timers.used, suspended_timers.array_size, suspended_timers.array_size * suspended_timers.data_size);

  for(int i=0;i<suspended_timers.used;i++){
     c_printf("  suspended_timer[%u]\ttimer_ptr:%p\ttime_left:%u ms\tperiod:%u ms\n",
         i, susp_timer_array[i], (uint32)(susp_timer_array[i]->timer_expire/312.5), (uint32)(susp_timer_array[i]->timer_period/312.5));
   }
  return;
}

void swtmr_print_timer_list(void){
  volatile uint32 frc2_count=RTC_REG_READ(FRC2_COUNT_ADDRESS);
  os_timer_t* timer_list_ptr=NULL;
  uint32 time_till_fire=0;
  char print_buffer[2048];

  c_printf("\n\ttimer_list contents:\ncurrent FRC2 count:%u\n", frc2_count);
  c_printf(" %-10s  %-10s  %-10s  %-10s  %-10s  %-10s  %-10s\n", "ptr", "expire", "period", "func", "arg", "fire(tick)", "fire(ms)");

  ETS_INTR_LOCK();

  timer_list_ptr=timer_list;

  while(timer_list_ptr != NULL){
    time_till_fire=(timer_list_ptr->timer_expire - frc2_count) / 312.5;

  c_printf(" %-10p  %-10u  %-10u  %-10p  %-10p  %-10u  %-10u\n",
        timer_list_ptr, (uint32)(timer_list_ptr->timer_expire),
        (uint32)(timer_list_ptr->timer_period ), timer_list_ptr->timer_func,
        timer_list_ptr->timer_arg, (timer_list_ptr->timer_expire - frc2_count), time_till_fire);

    timer_list_ptr=timer_list_ptr->timer_next;
  }
  ETS_INTR_UNLOCK();
  return;
}

#endif

int sw_timer_suspend(os_timer_t* timer_ptr){
  int return_value = SWTMR_OK;

  if(timer_ptr != NULL){
    // Timer pointer was provided, suspending specified timer

    return_value = timer_do_suspend(timer_ptr);

    if(return_value != SWTMR_OK){
      return return_value;
    }
  }
  else{
    //timer pointer not found, suspending all timers

    if(timer_registry.data_ptr == NULL){
      return SWTMR_REGISTRY_NO_REGISTERED_TIMERS;
    }

    timer_registry_remove_unarmed();

    os_timer_t** tmr_reg_arr = timer_registry.data_ptr;
    os_timer_t* temp_ptr = tmr_reg_arr[0];

    while(temp_ptr != NULL){
      return_value = timer_do_suspend(temp_ptr);

      if(return_value != SWTMR_OK){
        return return_value;
      }

      temp_ptr = tmr_reg_arr[0];
    }
  }
  return return_value;
}

int sw_timer_resume(os_timer_t* timer_ptr){

  if(suspended_timers.data_ptr == NULL){
    return SWTMR_SUSPEND_NO_SUSPENDED_TIMERS;
  }

  os_timer_t** suspended_tmr_array = suspended_timers.data_ptr;
  os_timer_t** suspended_timer_ptr = NULL;
  int retval=SWTMR_OK;

  if(timer_ptr!=NULL){
    suspended_timer_ptr = timer_suspended_check(timer_ptr);
    if(suspended_timer_ptr == NULL){
      //timer not suspended
      return SWTMR_SUSPEND_TIMER_NOT_SUSPENDED;
    }

    retval = timer_do_resume_single(suspended_timer_ptr);
    if(retval != SWTMR_OK){
      return retval;
    }
  }
  else{
    suspended_timer_ptr = &suspended_tmr_array[0];

    while(suspended_timers.used > 0){
      retval = timer_do_resume_single(suspended_timer_ptr);
      if(retval != SWTMR_OK){
        SWTMR_ERR("Unable to continue resuming timers, error(%u)", retval);
        return retval;
      }
      suspended_timer_ptr = &suspended_tmr_array[0];
    }
  }
  return SWTMR_OK;
}

void sw_timer_register(void* timer_ptr){
  if(timer_ptr == NULL){
    SWTMR_DBG("error: timer_ptr is NULL");
    return;
  }

  registry_queue_t* queue_temp = c_zalloc(sizeof(registry_queue_t));

  if(queue_temp == NULL){
    SWTMR_ERR("MALLOC FAIL! req:%u, free:%u", sizeof(registry_queue_t), system_get_free_heap_size());
    return;
  }
  queue_temp->timer_ptr = timer_ptr;

  if(register_queue == NULL){
    register_queue = queue_temp;

    if(timer_reg_task_id == false) timer_reg_task_id = task_get_id(timer_register_task);
    task_post_low(timer_reg_task_id, false);
//    SWTMR_DBG("queue empty, adding timer(%p) to queue and posting task", timer_ptr);
  }
  else{
    registry_queue_t* register_queue_tail = register_queue;

    while(register_queue_tail->next != NULL){
      register_queue_tail = register_queue_tail->next;
    }

    register_queue_tail->next = queue_temp;
//    SWTMR_DBG("queue NOT empty, appending timer(%p) to queue", timer_ptr);
  }

  return;
}

void sw_timer_unregister(void* timer_ptr){
  if(timer_ptr == NULL){
    SWTMR_DBG("error: timer_ptr is NULL");
    return;
  }

  registry_queue_t* queue_temp = c_zalloc(sizeof(registry_queue_t));

  if(queue_temp == NULL){
    SWTMR_ERR("MALLOC FAIL! req:%u, free:%u", sizeof(registry_queue_t), system_get_free_heap_size());
    return;
  }
  queue_temp->timer_ptr=timer_ptr;

  if(unregister_queue == NULL){
    unregister_queue = queue_temp;
    if(timer_unreg_task_id==false) timer_unreg_task_id=task_get_id(timer_unregister_task);
    task_post_low(timer_unreg_task_id, false);
//    SWTMR_DBG("queue empty, adding timer(%p) to queue and posting task", timer_ptr);
  }
  else{
    registry_queue_t* unregister_queue_tail=unregister_queue;
    while(unregister_queue_tail->next != NULL){
      unregister_queue_tail=unregister_queue_tail->next;
    }
    unregister_queue_tail->next = queue_temp;
//    SWTMR_DBG("queue NOT empty, appending timer(%p) to queue", timer_ptr);
  }

  return;
}

const char* swtmr_errorcode2str(int error_value){
#ifdef USE_SWTMR_ERROR_STRINGS
  if(SWTMR_ERROR_STRINGS[error_value] == NULL){
    SWTMR_ERR("ERRORCEPTION!");
    return NULL;
  }
  else{
    return SWTMR_ERROR_STRINGS[error_value];
  }
#else
  SWTMR_ERR("error(%u)", error_value);
  return "ERROR! for more info, use debug build";
#endif

}

bool swtmr_suspended_test(os_timer_t* timer_ptr){
  os_timer_t** test_var = timer_suspended_check(timer_ptr);
  if(test_var == NULL){
    return false;
  }
  return true;
}
