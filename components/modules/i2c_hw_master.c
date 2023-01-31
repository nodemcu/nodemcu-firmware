#include "module.h"
#include "lauxlib.h"
#include "lmem.h"
#include "platform.h"
#include "driver/i2c.h"
#include "soc/i2c_reg.h"
#include "hal/i2c_ll.h"
#include "rom/gpio.h"

#include "i2c_common.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_task.h"

#include "task/task.h"

#include <string.h>


// result data descriptor
// it's used as variable length data block, filled by the i2c driver during read
typedef struct {
  size_t len;
  uint8_t data[1];
} i2c_result_type;

// job descriptor
// contains all information for the transfer task and subsequent result task
// to process the transfer
typedef struct {
  unsigned port;
  i2c_cmd_handle_t cmd;
  unsigned to_ms;
  i2c_result_type *result;
  esp_err_t err;
  int cb_ref;
} i2c_job_desc_type;

// user data descriptor for a port
// provides buffer for the job setup and holds port-specific task & queue handles
typedef struct {
  i2c_job_desc_type job;
  TaskHandle_t xTransferTaskHandle;
  QueueHandle_t xTransferJobQueue;
} i2c_hw_master_ud_type;

// the global variables for storing userdata for each I2C port
static i2c_hw_master_ud_type i2c_hw_master_ud[I2C_NUM_MAX];

// Transfer task handle and job queue
static task_handle_t i2c_transfer_task_id;

// Transfer Task, FreeRTOS layer
// This is a fully-fledged FreeRTOS task which runs concurrently and pulls
// jobs off the queue. Jobs are forwarded to i2c_master_cmd_begin() which blocks
// this task throughout the I2C transfer.
// Posts a task to the nodemcu task system to resume.
//
static void vTransferTask( void *pvParameters )
{
  QueueHandle_t xQueue = (QueueHandle_t)pvParameters;
  i2c_job_desc_type *job;

  for (;;) {
    job = (i2c_job_desc_type *)malloc( sizeof( i2c_job_desc_type ) );
    if (!job) {
      // shut down this task in case of memory shortage
      vTaskSuspend( NULL );
    }

    // get a job descriptor
    xQueueReceive( xQueue, job, portMAX_DELAY );

    job->err = i2c_master_cmd_begin( job->port, job->cmd,
                                     job->to_ms > 0 ? job->to_ms / portTICK_PERIOD_MS : portMAX_DELAY );

    task_post_medium( i2c_transfer_task_id, (task_param_t)job );
  }
}

// Free memory of a job descriptor
//
static void free_job_memory( lua_State *L, i2c_job_desc_type *job )
{
  if (job->result)
    luaM_free( L, job->result );

  luaL_unref( L, LUA_REGISTRYINDEX, job->cb_ref);

  if (job->cmd)
    i2c_cmd_link_delete( job->cmd );
}

// Transfer Task, NodeMCU layer
// Is posted by the FreeRTOS Transfer Task and triggers the Lua callback with optional
// read result data.
//
static void i2c_transfer_task( task_param_t param, task_prio_t prio )
{
  i2c_job_desc_type *job = (i2c_job_desc_type *)param;

  lua_State *L = lua_getstate();

  if (job->cb_ref != LUA_NOREF) {
    lua_rawgeti( L, LUA_REGISTRYINDEX, job->cb_ref );
    if (job->result) {
      // all fine, deliver read data
      lua_pushlstring( L, (char *)job->result->data, job->result->len );
    } else {
      lua_pushnil( L );
    }
    lua_pushboolean( L, job->err == ESP_OK );
    luaL_pcallx(L, 2, 0);
  }

  // free all memory
  free_job_memory( L, job );
  free( job );
}


// Set up FreeRTOS task and queue
// Prepares the gory tasking stuff.
//
static int setup_rtos_task_and_queue( i2c_hw_master_ud_type *ud )
{
  // create queue
  // depth 1 allows to skip "wait for empty queue" in synchronous mode
  // consider this when increasing depth
  ud->xTransferJobQueue = xQueueCreate( 1, sizeof( i2c_job_desc_type ) );
  if (!ud->xTransferJobQueue)
    return 0;

  char pcName[configMAX_TASK_NAME_LEN+1];
  snprintf( pcName, configMAX_TASK_NAME_LEN+1, "I2C_Task_%d", ud->job.port );
  pcName[configMAX_TASK_NAME_LEN] = '\0';

  // create task with higher priority
  BaseType_t xReturned = xTaskCreate( vTransferTask,
                                      pcName,
                                      1024,
                                      (void *)ud->xTransferJobQueue,
                                      ESP_TASK_MAIN_PRIO + 1,
                                      &(ud->xTransferTaskHandle) );
  if (xReturned != pdPASS) {
    vQueueDelete( ud->xTransferJobQueue );
    ud->xTransferJobQueue = NULL;
    return 0;
  }

  return 1;
}



#define get_udata(L, id)                                            \
  unsigned port = id - I2C_ID_HW0;                                  \
  luaL_argcheck( L, port < I2C_NUM_MAX, 1, "invalid hardware id" ); \
  i2c_hw_master_ud_type *ud = &(i2c_hw_master_ud[port]);            \
  i2c_job_desc_type *job = &(ud->job);



static int i2c_lua_checkerr( lua_State *L, esp_err_t err ) {
  const char *msg;

  switch (err) {
  case ESP_OK: return 0;
  case ESP_FAIL: msg = "i2c command failed or NACK from slave"; break;
  case ESP_ERR_INVALID_ARG: msg = "i2c parameter error"; break;
  case ESP_ERR_INVALID_STATE: msg = "i2c driver state error"; break;
  case ESP_ERR_TIMEOUT: msg = "i2c timeout"; break;
  default: msg = "i2c unknown error"; break;
  }

  return luaL_error( L, msg );
}

// Set up userdata for a transfer
//
static void i2c_setup_ud_transfer( lua_State *L, i2c_hw_master_ud_type *ud )
{
  free_job_memory( L, &(ud->job) );
  ud->job.result = NULL;
  ud->job.cb_ref = LUA_NOREF;

  // set up an empty command link
  ud->job.cmd = i2c_cmd_link_create();
}

// Set up the HW as master interface
// Cares for I2C driver creation and initialization.
// Prepares an empty job descriptor and triggers setup of FreeRTOS stuff.
//
int li2c_hw_master_setup( lua_State *L, unsigned id, unsigned sda, unsigned scl, uint32_t speed, unsigned stretchfactor )

{
  get_udata(L, id);

  i2c_config_t cfg;
  memset( &cfg, 0, sizeof( cfg ) );
  cfg.mode = I2C_MODE_MASTER;

  luaL_argcheck( L, platform_gpio_output_exists(sda), 2, "invalid sda pin" );
  cfg.sda_io_num = sda;
  cfg.sda_pullup_en = GPIO_PULLUP_ENABLE;

  luaL_argcheck( L, platform_gpio_output_exists(scl), 3, "invalid scl pin" );
  cfg.scl_io_num = scl;
  cfg.scl_pullup_en = GPIO_PULLUP_ENABLE;

  luaL_argcheck( L, speed > 0 && speed <= 1000000, 4, "invalid speed" );
  cfg.master.clk_speed = speed;

  // init driver level
  i2c_lua_checkerr( L, i2c_param_config( port, &cfg ) );

  luaL_argcheck( L, stretchfactor > 0 , 5, "invalid stretch factor" );
  int timeoutcycles;
  i2c_lua_checkerr( L, i2c_get_timeout( port, &timeoutcycles) );
  timeoutcycles = timeoutcycles * stretchfactor;
  luaL_argcheck( L, timeoutcycles * stretchfactor <= I2C_LL_MAX_TIMEOUT, 5, "excessive stretch factor" );
  i2c_lua_checkerr( L, i2c_set_timeout( port, timeoutcycles) );
 
  i2c_lua_checkerr( L, i2c_driver_install( port, cfg.mode, 0, 0, 0 ));

  job->port = port;

  job->cmd = NULL;
  job->result = NULL;
  job->cb_ref = LUA_NOREF;
  i2c_setup_ud_transfer( L, ud );

  // kick-start transfer task
  if (!setup_rtos_task_and_queue( ud )) {
    free_job_memory( L, &(ud->job) );
    i2c_driver_delete( port );
    luaL_error( L, "rtos task creation failed" );
  }
  return timeoutcycles;
}

void li2c_hw_master_start( lua_State *L, unsigned id )
{
  get_udata(L, id);

  if (!job->cmd)
    luaL_error( L, "no commands scheduled" );

  i2c_lua_checkerr( L, i2c_master_start( job->cmd ) );
}

void li2c_hw_master_stop( lua_State *L, unsigned id )
{
  get_udata(L, id);

  if (!job->cmd)
    luaL_error( L, "no commands scheduled" );

  i2c_lua_checkerr( L, i2c_master_stop( job->cmd ) );
}

int li2c_hw_master_address( lua_State *L, unsigned id, uint16_t address, uint8_t direction, bool ack_check_en )
{
  get_udata(L, id);

  if (!job->cmd)
    luaL_error( L, "no commands scheduled" );

  i2c_lua_checkerr( L, i2c_master_write_byte( job->cmd, address << 1 | direction, ack_check_en ) );

  return 1;
}

void li2c_hw_master_write( lua_State *L, unsigned id, uint8_t data, bool ack_check_en )
{
  get_udata(L, id);

  if (!job->cmd)
    luaL_error( L, "no commands scheduled" );

  i2c_lua_checkerr( L, i2c_master_write_byte( job->cmd, data, ack_check_en ) );
}

void li2c_hw_master_read( lua_State *L, unsigned id, uint32_t len )
{
  get_udata(L, id);

  if (!job->cmd)
    luaL_error( L, "no commands scheduled" );

  if (job->result)
    luaL_error( L, "only one read per transfer supported" );

  // allocate read buffer as user data
  i2c_result_type *res = (i2c_result_type *)luaM_malloc( L, sizeof( i2c_result_type ) + len-1 );
  if (!res)
    luaL_error( L, "out of memory" );
  res->len = len;
  job->result = res;

// call i2c_master_read specifying a NACK on last byte read
  i2c_lua_checkerr( L, i2c_master_read( job->cmd, res->data,len,I2C_MASTER_LAST_NACK) );

}

// Initiate the I2C transfer
// Depending on the presence of a callback parameter, it will stay in synchronous mode
// or posts the job to the FreeRTOS queue for asynchronous processing.
//
int li2c_hw_master_transfer( lua_State *L )
{
  int stack = 0;

  unsigned id = luaL_checkinteger( L, ++stack );
  get_udata(L, id);

  if (!job->cmd)
    luaL_error( L, "no commands scheduled" );

  stack++;
  if (lua_isfunction( L, stack )) {
    lua_pushvalue( L, stack );  // copy argument (func) to the top of stack
    luaL_unref( L, LUA_REGISTRYINDEX, job->cb_ref );
    job->cb_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  } else
    stack--;

  int to_ms = luaL_optint( L, ++stack, 0 );
  if (to_ms < 0)
    to_ms = 0;
  job->to_ms = to_ms;

  if (job->cb_ref != LUA_NOREF) {
    // asynchronous mode
    xQueueSend( ud->xTransferJobQueue, job, portMAX_DELAY );

    // the transfer task should be unblocked now
    //   (i.e. in eReady state since it can receive from the queue)
    portYIELD();

    // invalidate last job, it's queued now
    job->cmd = NULL;          // don't delete link! it's used by the transfer task
    job->result = NULL;       // don't free result memory! it's used by the transfer task
    job->cb_ref = LUA_NOREF;  // don't unref! it's used by the transfer task

    // prepare the next transfer
    i2c_setup_ud_transfer( L, ud );
    return 0;

  } else {
    // synchronous mode

    // no need to wait for queue to become empty when queue depth is 1

    // note that i2c_master_cmd_begin() implements mutual exclusive access
    // if it is currently in progress from the transfer task, it will block here until 
    esp_err_t err = i2c_master_cmd_begin( job->port, job->cmd,
                                          job->to_ms > 0 ? job->to_ms / portTICK_PERIOD_MS : portMAX_DELAY );

    switch (err) {
    case ESP_OK:
      if (job->result) {
        // all fine, deliver read data
        lua_pushlstring( L, (char *)job->result->data, job->result->len );
      } else {
        lua_pushnil( L );
      }
      lua_pushboolean( L, 1 );

      // prepare the next transfer
      i2c_setup_ud_transfer( L, ud );
      return 2;

    case ESP_FAIL:
      lua_pushnil( L );
      lua_pushboolean( L, 0 );

      // prepare the next transfer
      i2c_setup_ud_transfer( L, ud );
      return 2;

    default:
      i2c_setup_ud_transfer( L, ud );
      return i2c_lua_checkerr( L, err );
    }
  }
}


void li2c_hw_master_init( lua_State *L )
{
  // prepare task id
  i2c_transfer_task_id = task_get_id( i2c_transfer_task );
}
