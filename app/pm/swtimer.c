/* swTimer.c SDK timer suspend API
 *
 * SDK software timer API info:
 *
 * The SDK software timer uses a linked list called `os_timer_t* timer_list` to keep track of
 * all currently armed timers.
 *
 * The SDK software timer API executes in a task. The priority of this task in relation to the
 * application level tasks is unknown (at time of writing).
 *
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
 * when using millisecond(default) timers, FRC2 resolution is 312.5 ticks per millisecond.
 *
 *
 * TIMER SUSPEND API INFO:
 *
 * Timer suspension is achieved by first finding any non-SDK timers by comparing the timer function callback pointer
 * of each timer in "timer_list" to a list of registered timer callback pointers stored in the Lua registry.
 * If a timer with a corresponding registered callback pointer is found, the timer's timer_expire field is is compared
 * to the current FRC2 count and the difference is saved along with the other timer parameters to temporary variables.
 * The timer is then disarmed and the parameters are copied back, the timer pointer is then
 * added to a separate linked list of which the head pointer is stored as a lightuserdata in the lua registry.
 *
 * Resuming the timers is achieved by first retrieving the lightuserdata holding the suspended timer list head pointer.
 * Then, starting with the beginning of the list the current FRC2 count is added back to the timer's timer_expire, then
 * the timer is manually added back to "timer_list" in an ascending order.
 * Once there are no more suspended timers, the function returns
 *
 *
 */#include "module.h"
#include "lauxlib.h"
#include "platform.h"

#include "user_interface.h"
#include "user_modules.h"

#include "c_string.h"
#include "c_stdlib.h"
#include "ctype.h"

#include "c_types.h"

//#define SWTMR_DEBUG
#if !defined(SWTMR_DBG) && defined(LUA_USE_MODULES_SWTMR_DBG)
 #define SWTMR_DEBUG
#endif

//this section specifies which lua registry to use. LUA_GLOBALSINDEX or LUA_REGISTRYINDEX
#ifdef SWTMR_DEBUG
#define SWTMR_DBG(fmt, ...) dbg_printf("\n SWTMR_DBG(%s): "fmt"\n", __FUNCTION__, ##__VA_ARGS__)
#define L_REGISTRY LUA_GLOBALSINDEX
#define CB_LIST_STR "timer_cb_ptrs"
#define SUSP_LIST_STR "suspended_tmr_LL_head"
#else
#define SWTMR_DBG(...)
#define L_REGISTRY LUA_REGISTRYINDEX
#define CB_LIST_STR "cb"
#define SUSP_LIST_STR "st"
#endif


typedef struct tmr_cb_queue{
  os_timer_func_t *tmr_cb_ptr;
  uint8 suspend_policy;
  struct tmr_cb_queue * next;
}tmr_cb_queue_t;

typedef struct cb_registry_item{
  os_timer_func_t *tmr_cb_ptr;
  uint8 suspend_policy;
}cb_registry_item_t;


/*  Internal variables  */
static tmr_cb_queue_t* register_queue = NULL;
static task_handle_t cb_register_task_id = 0; //variable to hold task id for task handler(process_cb_register_queue)

/*  Function declarations */
//void swtmr_cb_register(void* timer_cb_ptr, uint8 resume_policy);
static void add_to_reg_queue(void* timer_cb_ptr, uint8 suspend_policy);
static void process_cb_register_queue(task_param_t param, uint8 priority);


#ifdef SWTMR_DEBUG
#define push_swtmr_registry_key(L) lua_pushstring(L, "SWTMR_registry_key")
#else
#define push_swtmr_registry_key(L) lua_pushlightuserdata(L, &register_queue);
#endif

#include <pm/swtimer.h>

void swtmr_suspend_timers(){
  lua_State* L = lua_getstate();

  //get swtimer table
  push_swtmr_registry_key(L);
  lua_rawget(L, L_REGISTRY);

  //get cb_list table
  lua_pushstring(L, CB_LIST_STR);
  lua_rawget(L, -2);

  //check for existence of the swtimer table and the cb_list table, return if not found
  if(!lua_istable(L, -2) || !lua_istable(L, -1)){
    // not necessarily an error maybe there are legitimately no timers to suspend
    lua_pop(L, 2);
    return;
  }

  os_timer_t* suspended_timer_list_head = NULL;
  os_timer_t* suspended_timer_list_tail = NULL;

  //get suspended_timer_list table
  lua_pushstring(L, SUSP_LIST_STR);
  lua_rawget(L, -3);

  //if suspended_timer_list exists, find tail of list
  if(lua_isuserdata(L, -1)){
    suspended_timer_list_head = suspended_timer_list_tail = lua_touserdata(L, -1);
    while(suspended_timer_list_tail->timer_next != NULL){
      suspended_timer_list_tail = suspended_timer_list_tail->timer_next;
    }
  }

  lua_pop(L, 1);

  //get length of lua table containing the callback pointers
  size_t registered_cb_qty = lua_objlen(L, -1);

  //allocate a temporary array to hold the list of callback pointers
  cb_registry_item_t** cb_reg_array = c_zalloc(sizeof(cb_registry_item_t*)*registered_cb_qty);
  if(!cb_reg_array){
    luaL_error(L, "%s: unable to suspend timers, out of memory!", __func__);
    return;
  }
  uint8 index = 0;

  //convert lua table cb_list to c array
  lua_pushnil(L);
  while(lua_next(L, -2) != 0){
    if(lua_isuserdata(L, -1)){
      cb_reg_array[index] = lua_touserdata(L, -1);
    }
    lua_pop(L, 1);
    index++;
  }

  //the cb_list table is no longer needed, pop it from the stack
  lua_pop(L, 1);
  
  volatile uint32 frc2_count = RTC_REG_READ(FRC2_COUNT_ADDRESS);

  os_timer_t* timer_ptr = timer_list;

  uint32 expire_temp = 0;
  uint32 period_temp = 0;
  void* arg_temp = NULL;

  /* In this section, the SDK's timer_list is traversed to find any timers that have a registered callback pointer.
   * If a registered callback is found, the timer is suspended by saving the difference
   * between frc2_count and timer_expire then the timer is disarmed and placed into suspended_timer_list
   * so it can later be resumed.
   */
  while(timer_ptr != NULL){
    os_timer_t* next_timer = (os_timer_t*)0xffffffff;
    for(size_t i = 0; i < registered_cb_qty; i++){
      if(timer_ptr->timer_func == cb_reg_array[i]->tmr_cb_ptr){

        //current timer will be suspended, next timer's pointer will be needed to continue processing timer_list
        next_timer = timer_ptr->timer_next;

        //store timer parameters temporarily so the timer can be disarmed
        if(timer_ptr->timer_expire < frc2_count)
          expire_temp = 2; //  16 us in ticks (1 tick = ~3.2 us) (arbitrarily chosen value)
        else
          expire_temp = timer_ptr->timer_expire - frc2_count;
        period_temp = timer_ptr->timer_period;
        arg_temp = timer_ptr->timer_arg;

        if(timer_ptr->timer_period == 0 && cb_reg_array[i]->suspend_policy == SWTIMER_RESTART){
          SWTMR_DBG("Warning: suspend_policy(RESTART) is not compatible with single-shot timer(%p), changing suspend_policy to (RESUME)", timer_ptr);
          cb_reg_array[i]->suspend_policy = SWTIMER_RESUME;
        }

        //remove the timer from timer_list so we don't have to.
        os_timer_disarm(timer_ptr); 

        timer_ptr->timer_next = NULL;

        //this section determines timer behavior on resume
        if(cb_reg_array[i]->suspend_policy == SWTIMER_DROP){
          SWTMR_DBG("timer(%p) was disarmed and will not be resumed", timer_ptr);
        }
        else if(cb_reg_array[i]->suspend_policy == SWTIMER_IMMEDIATE){
          timer_ptr->timer_expire = 1;
          SWTMR_DBG("timer(%p) will fire immediately on resume", timer_ptr);
        }
        else if(cb_reg_array[i]->suspend_policy == SWTIMER_RESTART){
          timer_ptr->timer_expire = period_temp;
          SWTMR_DBG("timer(%p) will be restarted on resume", timer_ptr);
        }
        else{
          timer_ptr->timer_expire = expire_temp;
          SWTMR_DBG("timer(%p) will be resumed with remaining time", timer_ptr);
        }

        if(cb_reg_array[i]->suspend_policy != SWTIMER_DROP){
          timer_ptr->timer_period = period_temp;
          timer_ptr->timer_func = cb_reg_array[i]->tmr_cb_ptr;
          timer_ptr->timer_arg = arg_temp;

          //add timer to suspended_timer_list
          if(suspended_timer_list_head == NULL){
            suspended_timer_list_head = timer_ptr;
            suspended_timer_list_tail = timer_ptr;
          }
          else{
            suspended_timer_list_tail->timer_next = timer_ptr;
            suspended_timer_list_tail = timer_ptr;
          }
        }
      }
    }

    //if timer was suspended, timer_ptr->timer_next is invalid, use next_timer instead.
    if(next_timer != (os_timer_t*)0xffffffff){
      timer_ptr = next_timer;
    }
    else{
      timer_ptr = timer_ptr->timer_next;
    }
  }

  //tmr_cb_ptr_array is no longer needed.
  c_free(cb_reg_array);

  //add suspended_timer_list pointer to swtimer table.
  lua_pushstring(L, SUSP_LIST_STR);
  lua_pushlightuserdata(L, suspended_timer_list_head);
  lua_rawset(L, -3);

  //pop swtimer table from stack
  lua_pop(L, 1);
  return;
}

void swtmr_resume_timers(){
  lua_State* L = lua_getstate();
  
  //get swtimer table
  push_swtmr_registry_key(L);
  lua_rawget(L, L_REGISTRY);

  //get suspended_timer_list lightuserdata
  lua_pushstring(L, SUSP_LIST_STR);
  lua_rawget(L, -2);
  
  //check for existence of swtimer table and the suspended_timer_list pointer userdata, return if not found
  if(!lua_istable(L, -2) || !lua_isuserdata(L, -1)){
    // not necessarily an error maybe there are legitimately no timers to resume
    lua_pop(L, 2);
    return;
  }

  os_timer_t* suspended_timer_list_ptr = lua_touserdata(L, -1);
  lua_pop(L, 1); //pop suspended timer list userdata from stack

  //since timers will be resumed, the suspended_timer_list lightuserdata can be cleared from swtimer table
  lua_pushstring(L, SUSP_LIST_STR);
  lua_pushnil(L);
  lua_rawset(L, -3);
  

  lua_pop(L, 1); //pop swtimer table from stack

  volatile uint32 frc2_count = RTC_REG_READ(FRC2_COUNT_ADDRESS);

  //this section does the actual resuming of the suspended timer(s)
  while(suspended_timer_list_ptr != NULL){
    os_timer_t* timer_list_ptr = timer_list;

    //the pointer to next suspended timer must be saved, the current suspended timer will be removed from the list
    os_timer_t* next_suspended_timer_ptr = suspended_timer_list_ptr->timer_next;

    suspended_timer_list_ptr->timer_expire += frc2_count;

    //traverse timer_list to determine where to insert suspended timer
    while(timer_list_ptr != NULL){
      if(suspended_timer_list_ptr->timer_expire > timer_list_ptr->timer_expire){
        if(timer_list_ptr->timer_next != NULL){
          //current timer is not at tail of timer_list
          if(suspended_timer_list_ptr->timer_expire < timer_list_ptr->timer_next->timer_expire){
            //insert suspended timer between current timer and next timer
            suspended_timer_list_ptr->timer_next = timer_list_ptr->timer_next;
            timer_list_ptr->timer_next = suspended_timer_list_ptr;
            break; //timer resumed exit while loop
          }
          else{
            //suspended timer expire is larger than next timer
          }
        }
        else{
          //current timer is at tail of timer_list and suspended timer expire is greater then current timer
          //append timer to end of timer_list
          timer_list_ptr->timer_next = suspended_timer_list_ptr;
          suspended_timer_list_ptr->timer_next = NULL;
          break; //timer resumed exit while loop
        }
      }
      else if(timer_list_ptr == timer_list){
        //insert timer at head of list
        suspended_timer_list_ptr->timer_next = timer_list_ptr;
        timer_list = timer_list_ptr = suspended_timer_list_ptr;
        break; //timer resumed exit while loop
      }
      //suspended timer expire is larger than next timer
      //timer not resumed, next timer in timer_list
      timer_list_ptr = timer_list_ptr->timer_next;
    }
    //timer was resumed, next suspended timer
    suspended_timer_list_ptr = next_suspended_timer_ptr;
  }
  return;
}

//this function registers a timer callback pointer in a lua table
void swtmr_cb_register(void* timer_cb_ptr, uint8 suspend_policy){
  lua_State* L = lua_getstate();
  if(!L){
    //Lua has not started yet, therefore L_REGISTRY is not available.
    //add timer cb to queue for later processing after Lua has started
    add_to_reg_queue(timer_cb_ptr, suspend_policy);
    return;
  }
  if(timer_cb_ptr){
    size_t cb_list_last_idx = 0;

    push_swtmr_registry_key(L);
    lua_rawget(L, L_REGISTRY);

    if(!lua_istable(L, -1)){
      //swtmr does not exist, create and add to registry
      lua_pop(L, 1);
      lua_newtable(L);//push new table for swtmr.timer_cb_list
      // add swtimer table to L_REGISTRY
      push_swtmr_registry_key(L);
      lua_pushvalue(L, -2);
      lua_rawset(L, L_REGISTRY);
    }

    lua_pushstring(L, CB_LIST_STR);
    lua_rawget(L, -2);

    if(lua_istable(L, -1)){
      //cb_list exists, get length of list
      cb_list_last_idx = lua_objlen(L, -1);
    }
    else{
      //cb_list does not exist in swtmr, create and add to swtmr
      lua_pop(L, 1);// pop nil value from stack
      lua_newtable(L);//create new table for swtmr.timer_cb_list
      lua_pushstring(L, CB_LIST_STR); //push name for the new table onto the stack
      lua_pushvalue(L, -2); //push table to top of stack
      lua_rawset(L, -4); //pop table and name from stack and register in swtmr
    }

    //append new timer cb ptr to table
    lua_pushnumber(L, cb_list_last_idx+1);
    cb_registry_item_t* reg_item = lua_newuserdata(L, sizeof(cb_registry_item_t));
    reg_item->tmr_cb_ptr = timer_cb_ptr;
    reg_item->suspend_policy = suspend_policy;
    lua_rawset(L, -3);

    //clear items pushed onto stack by this function
    lua_pop(L, 2);
  }
  return;
}

//this function adds the timer cb ptr to a queue for later registration after lua has started
static void add_to_reg_queue(void* timer_cb_ptr, uint8 suspend_policy){
  if(!timer_cb_ptr)
    return;
  tmr_cb_queue_t* queue_temp = c_zalloc(sizeof(tmr_cb_queue_t));
  if(!queue_temp){
    //it's boot time currently and we're already out of memory, something is very wrong...
    dbg_printf("\n\t%s:out of memory, rebooting.", __FUNCTION__);
    system_restart();
  }
  queue_temp->tmr_cb_ptr = timer_cb_ptr;
  queue_temp->suspend_policy = suspend_policy;
  queue_temp->next = NULL;

  if(register_queue == NULL){
    register_queue = queue_temp;
  }
  else{
    tmr_cb_queue_t* queue_ptr = register_queue;
    while(queue_ptr->next != NULL){
      queue_ptr = queue_ptr->next;
    }
    queue_ptr->next = queue_temp;
  }
  if(!cb_register_task_id){
  cb_register_task_id = task_get_id(process_cb_register_queue);//get task id from task interface
  task_post_low(cb_register_task_id, false); //post task to process next item in queue
  }
  return;
}

static void process_cb_register_queue(task_param_t param, uint8 priority)
{
  if(!lua_getstate()){
    SWTMR_DBG("L== NULL, Lua not yet started! posting task");
    task_post_low(cb_register_task_id, false); //post task to process next item in queue
    return;
  }
  while(register_queue != NULL){
    tmr_cb_queue_t* register_queue_ptr = register_queue;
    void* cb_ptr_tmp = register_queue_ptr->tmr_cb_ptr;
    swtmr_cb_register(cb_ptr_tmp, register_queue_ptr->suspend_policy);
    register_queue = register_queue->next;
    c_free(register_queue_ptr);
  }
  return;
}

#ifdef SWTMR_DEBUG
int print_timer_list(lua_State* L){
   push_swtmr_registry_key(L);
   lua_rawget(L, L_REGISTRY);
   lua_pushstring(L, CB_LIST_STR);
   lua_rawget(L, -2);
   if(!lua_istable(L, -2) || !lua_istable(L, -1)){
     lua_pop(L, 2);
     return 0;
   }
   os_timer_t* suspended_timer_list_head = NULL;
   os_timer_t* suspended_timer_list_tail = NULL;
   lua_pushstring(L, SUSP_LIST_STR);
   lua_rawget(L, -3);
   if(lua_isuserdata(L, -1)){
     suspended_timer_list_head = suspended_timer_list_tail = lua_touserdata(L, -1);
     while(suspended_timer_list_tail->timer_next != NULL){
       suspended_timer_list_tail = suspended_timer_list_tail->timer_next;
     }
   }
   lua_pop(L, 1);
   size_t registered_cb_qty = lua_objlen(L, -1);
   cb_registry_item_t** cb_reg_array = c_zalloc(sizeof(cb_registry_item_t*)*registered_cb_qty);
   if(!cb_reg_array){
     luaL_error(L, "%s: unable to suspend timers, out of memory!", __func__);
     return 0;
   }
   uint8 index = 0;
   lua_pushnil(L);
   while(lua_next(L, -2) != 0){
     if(lua_isuserdata(L, -1)){
       cb_reg_array[index] = lua_touserdata(L, -1);
     }
     lua_pop(L, 1);
     index++;
   }
   lua_pop(L, 1);


  os_timer_t* timer_list_ptr = timer_list;
  dbg_printf("\n\tCurrent FRC2: %u\n", RTC_REG_READ(FRC2_COUNT_ADDRESS));
  dbg_printf("\ttimer_list:\n");
  while(timer_list_ptr != NULL){
    bool registered_flag = FALSE;
    for(int i=0; i < registered_cb_qty; i++){
      if(timer_list_ptr->timer_func == cb_reg_array[i]->tmr_cb_ptr){
        registered_flag = TRUE;
        break;
      }
    }
    dbg_printf("\tptr:%p\tcb:%p\texpire:%8u\tperiod:%8u\tnext:%p\t%s\n",
        timer_list_ptr, timer_list_ptr->timer_func, timer_list_ptr->timer_expire, timer_list_ptr->timer_period, timer_list_ptr->timer_next, registered_flag ? "Registered" : "");
    timer_list_ptr = timer_list_ptr->timer_next;
  }

  c_free(cb_reg_array);
  lua_pop(L, 1);

  return 0;
}

int print_susp_timer_list(lua_State* L){
  push_swtmr_registry_key(L);
  lua_rawget(L, L_REGISTRY);

  if(!lua_istable(L, -1)){
    return luaL_error(L, "swtmr table not found!");
  }

  lua_pushstring(L, SUSP_LIST_STR);
  lua_rawget(L, -2);

  if(!lua_isuserdata(L, -1)){
    return luaL_error(L, "swtmr.suspended_list userdata not found!");
  }

  os_timer_t* susp_timer_list_ptr = lua_touserdata(L, -1);
  dbg_printf("\n\tsuspended_timer_list:\n");
  while(susp_timer_list_ptr != NULL){
    dbg_printf("\tptr:%p\tcb:%p\texpire:%8u\tperiod:%8u\tnext:%p\n",susp_timer_list_ptr, susp_timer_list_ptr->timer_func, susp_timer_list_ptr->timer_expire, susp_timer_list_ptr->timer_period, susp_timer_list_ptr->timer_next);
    susp_timer_list_ptr = susp_timer_list_ptr->timer_next;
  }
  return 0;
}

int suspend_timers_lua(lua_State* L){
  swtmr_suspend_timers();
  return 0;
}

int resume_timers_lua(lua_State* L){
  swtmr_resume_timers();
  return 0;
}

static const LUA_REG_TYPE test_swtimer_debug_map[] = {
    { LSTRKEY( "timer_list" ),   LFUNCVAL( print_timer_list ) },
    { LSTRKEY( "susp_timer_list" ),   LFUNCVAL( print_susp_timer_list ) },
    { LSTRKEY( "suspend" ),      LFUNCVAL( suspend_timers_lua ) },
    { LSTRKEY( "resume" ),      LFUNCVAL( resume_timers_lua ) },
    { LNILKEY, LNILVAL }
};

NODEMCU_MODULE(SWTMR_DBG, "SWTMR_DBG", test_swtimer_debug_map, NULL);

#endif

