
#include "module.h"
#include "lauxlib.h"
#include "lextra.h"
#include "lmem.h"
#include "driver/i2c.h"

#include "i2c_common.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_task.h"

#include "task/task.h"

#include <string.h>


#define DEFAULT_BUF_LEN 128
#define I2C_SLAVE_DEFAULT_RXBUF_LEN DEFAULT_BUF_LEN
#define I2C_SLAVE_DEFAULT_TXBUF_LEN DEFAULT_BUF_LEN

// user data descriptor for a port
typedef struct {
  unsigned port;
  size_t rxbuf_len;
  TaskHandle_t xReceiveTaskHandle;
  int receivedcb_ref;
} i2c_hw_slave_ud_type;

// read data descriptor
// it's used as variable length data block, filled by the i2c driver during read
typedef struct {
  size_t len;
  uint8_t data[1];
} i2c_read_type;

// job descriptor
// contains all information for the transfer task and subsequent result task
// to process the transfer
typedef struct {
  unsigned port;
  esp_err_t err;
  int receivedcb_ref;
  i2c_read_type *read_data;
} i2c_job_desc_type;

// the global variables for storing userdata for each I2C port
static i2c_hw_slave_ud_type i2c_hw_slave_ud[I2C_NUM_MAX];

// Receive task handle and job queue
static task_handle_t i2c_receive_task_id;


// Receive Task, FreeRTOS layer
// This is a fully-fledged FreeRTOS task which runs concurrently and waits
// for data from the I2C master using i2c_slave_read_buffer which blocks this task.
// Received data is posted as a job to the NodeMCU layer task.
//
static void vReceiveTask( void *pvParameters )
{
  i2c_hw_slave_ud_type *ud = (i2c_hw_slave_ud_type *)pvParameters;
  i2c_job_desc_type *job;
  i2c_read_type *read_data;

  for (;;) {
    job = (i2c_job_desc_type *)malloc( sizeof( i2c_job_desc_type ) );
    read_data = (i2c_read_type *)malloc( sizeof( i2c_read_type ) + ud->rxbuf_len-1 );
    
    if (!job || !read_data) {
      // shut down this task in case of memory shortage
      vTaskSuspend( NULL );
    }
    job->read_data = read_data;

    int size = i2c_slave_read_buffer( ud->port, job->read_data->data, ud->rxbuf_len, portMAX_DELAY );
    if (size >= 0) {
      job->read_data->len = size;
      job->err = 0;
    } else {
      job->read_data->len = 0;
      job->err = size;
    }

    job->port = ud->port;
    job->receivedcb_ref = ud->receivedcb_ref;

    task_post_medium( i2c_receive_task_id, (task_param_t)job );
  }
}

// Receive Task, NodeMCU layer
// Is posted by the FreeRTOS layer and triggers the Lua callback with
// read result data.
//
static void i2c_receive_task( task_param_t param, task_prio_t prio )
{
  i2c_job_desc_type *job = (i2c_job_desc_type *)param;

  lua_State *L = lua_getstate();

  if (job->receivedcb_ref != LUA_NOREF) {
    lua_rawgeti( L, LUA_REGISTRYINDEX, job->receivedcb_ref );
    lua_pushinteger( L, job->err );
    if (job->read_data->len) {
      // all fine, deliver read data
      lua_pushlstring( L, (char *)job->read_data->data, job->read_data->len );
    } else {
      lua_pushnil( L );
    }
    lua_call(L, 2, 0);
  }

  // free all memory
  free( job->read_data );
  free( job );
}


static int i2c_lua_checkerr( lua_State *L, esp_err_t err ) {
  const char *msg;

  switch (err) {
  case ESP_OK: return 0;
  case ESP_FAIL: msg = "command failed"; break;
  case ESP_ERR_INVALID_ARG: msg = "parameter error"; break;
  case ESP_ERR_INVALID_STATE: msg = "driver state error"; break;
  case ESP_ERR_TIMEOUT: msg = "timeout"; break;
  default: msg = "unknown error"; break;
  }

  return luaL_error( L, msg );
}


#define get_udata(L)                                                \
  unsigned port = luaL_checkint( L, 1 ) - I2C_ID_HW0;               \
  luaL_argcheck( L, port < I2C_NUM_MAX, 1, "invalid hardware id" ); \
  i2c_hw_slave_ud_type *ud = &(i2c_hw_slave_ud[port]);


// Set up FreeRTOS task and queue
// Prepares the gory tasking stuff.
//
static int setup_rtos_task( i2c_hw_slave_ud_type *ud, unsigned port )
{

  char pcName[configMAX_TASK_NAME_LEN+1];
  snprintf( pcName, configMAX_TASK_NAME_LEN+1, "I2C_Slave_%d", port );
  pcName[configMAX_TASK_NAME_LEN] = '\0';

  // create task with higher priority
  BaseType_t xReturned = xTaskCreate( vReceiveTask,
                                      pcName,
                                      1024,
                                      (void *)ud,
                                      ESP_TASK_MAIN_PRIO + 1,
                                      &(ud->xReceiveTaskHandle) );
  if (xReturned != pdPASS) {
    return 0;
  }

  return 1;
}

// Set up the HW as master interface
// Cares for I2C driver creation and initialization.
//
static int li2c_slave_setup( lua_State *L )
{
  get_udata(L);
  ud->port = port;
  int stack = 1;

  luaL_checktable( L, ++stack );
  lua_settop( L, stack );

  i2c_config_t cfg;
  memset( &cfg, 0, sizeof( cfg ) );
  cfg.mode = I2C_MODE_SLAVE;

  lua_getfield( L, stack, "sda" );
  int sda = luaL_optint( L , -1, -1 );
  luaL_argcheck( L, GPIO_IS_VALID_OUTPUT_GPIO(sda), stack, "invalid sda pin" );
  cfg.sda_io_num = (gpio_num_t)sda;
  cfg.sda_pullup_en = GPIO_PULLUP_ENABLE;

  lua_getfield( L, stack, "scl" );
  int scl = luaL_optint( L , -1, -1 );
  luaL_argcheck( L, GPIO_IS_VALID_OUTPUT_GPIO(scl), stack, "invalid scl pin" );
  cfg.scl_io_num = (gpio_num_t)scl;
  cfg.scl_pullup_en = GPIO_PULLUP_ENABLE;

  lua_getfield( L, stack, "10bit" );
  bool en_10bit = luaL_optbool( L , -1, false );
  cfg.slave.addr_10bit_en = en_10bit ? 1 : 0;

  lua_getfield( L, stack, "addr" );
  int slave_addr = luaL_optint( L , -1, -1 );
  if (en_10bit)
    luaL_argcheck( L, slave_addr >= 0 && slave_addr < 1<<10, stack, "invalid slave address" );
  else
    luaL_argcheck( L, slave_addr >= 0 && slave_addr < 1<<7, stack, "invalid slave address" );
  cfg.slave.slave_addr = slave_addr;

  lua_getfield( L, stack, "rxbuf_len" );
  int rxbuf_len = luaL_optint( L , -1, I2C_SLAVE_DEFAULT_RXBUF_LEN );
  luaL_argcheck( L, rxbuf_len >= 0, stack, "invalid rxbuf_len" );
  ud->rxbuf_len = rxbuf_len;

  lua_getfield( L, stack, "txbuf_len" );
  int txbuf_len = luaL_optint( L , -1, I2C_SLAVE_DEFAULT_TXBUF_LEN );
  luaL_argcheck( L, rxbuf_len >= 0, stack, "invalid rxbuf_len" );

  i2c_lua_checkerr( L, i2c_param_config( port, &cfg ) );
  i2c_lua_checkerr( L, i2c_driver_install( port, cfg.mode, rxbuf_len, txbuf_len, 0 ));

  ud->receivedcb_ref = LUA_NOREF;

  if (!setup_rtos_task( ud, port )) {
    i2c_driver_delete( port );
    luaL_error( L, "rtos task creation failed" );
  }

  return 0;
}

// Write to slave send buffer
//
static int li2c_slave_send( lua_State *L )
{
  get_udata(L);
  ud = ud;

  const char *pdata;
  size_t datalen, i;
  int numdata;
  uint8_t byte;
  uint32_t wrote = 0;
  unsigned argn;

  if( lua_gettop( L ) < 2 )
    return luaL_error( L, "wrong arg type" );
  for( argn = 2; argn <= lua_gettop( L ); argn ++ )
  {
    // lua_isnumber() would silently convert a string of digits to an integer
    // whereas here strings are handled separately.
    if( lua_type( L, argn ) == LUA_TNUMBER )
    {
      numdata = ( int )luaL_checkinteger( L, argn );
      if( numdata < 0 || numdata > 255 )
        return luaL_error( L, "wrong arg range" );
      byte = (uint8_t)numdata;
      if( i2c_slave_write_buffer( port, &byte, 1, portMAX_DELAY ) < 0 )
        break;
      wrote ++;
    }
    else if( lua_istable( L, argn ) )
    {
      datalen = lua_objlen( L, argn );
      for( i = 0; i < datalen; i ++ )
      {
        lua_rawgeti( L, argn, i + 1 );
        numdata = ( int )luaL_checkinteger( L, -1 );
        lua_pop( L, 1 );
        if( numdata < 0 || numdata > 255 )
          return luaL_error( L, "wrong arg range" );
        byte = (uint8_t)numdata;
        if( i2c_slave_write_buffer( port, &byte, 1, portMAX_DELAY ) < 0 )
          break;
      }
      wrote += i;
      if( i < datalen )
        break;
    }
    else
    {
      pdata = luaL_checklstring( L, argn, &datalen );
      if( i2c_slave_write_buffer( port, (uint8_t *)pdata, datalen, portMAX_DELAY ) < 0 )
        break;
      wrote += datalen;
    }
  }
  lua_pushinteger( L, wrote );
  return 1;
}

// Register or unregister an event callback handler
//
static int li2c_slave_on( lua_State *L )
{
  enum events{
    ON_RECEIVE = 0
  };
  const char *const eventnames[] = {"receive", NULL};

  get_udata(L);
  int stack = 1;

  int event = luaL_checkoption(L, ++stack, NULL, eventnames);

  switch (event) {
  case ON_RECEIVE:
    luaL_unref( L, LUA_REGISTRYINDEX, ud->receivedcb_ref );

    ++stack;
    if (lua_isfunction( L, stack )) {
      lua_pushvalue( L, stack );  // copy argument (func) to the top of stack
      ud->receivedcb_ref = luaL_ref( L, LUA_REGISTRYINDEX );
    }
    break;
  default:
    break;
  }

  return 0;
}


LROT_BEGIN(li2c_slave, NULL, 0)
  LROT_FUNCENTRY( on,    li2c_slave_on )
  LROT_FUNCENTRY( setup, li2c_slave_setup )
  LROT_FUNCENTRY( send,  li2c_slave_send )
LROT_END(li2c_slave, NULL, 0)


void li2c_hw_slave_init( lua_State *L )
{
  // prepare task id
  i2c_receive_task_id = task_get_id( i2c_receive_task );
}
