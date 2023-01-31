// Module for interfacing with i2s hardware

#include <string.h>

#include "module.h"
#include "lauxlib.h"
#include "lmem.h"
#include "platform.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "esp_task.h"

#include "task/task.h"

#include "driver/i2s.h"

#include "common.h"

#define MAX_I2C_NUM 2
#define I2S_CHECK_ID(id)   if(id >= MAX_I2C_NUM) luaL_error( L, "i2s not exists" )

typedef struct {
  int ref;
  const char *data;
  size_t len;
} i2s_tx_data_t;

typedef struct {
  struct {
    TaskHandle_t taskHandle;
    QueueHandle_t queue;
  } tx;
  struct {
    TaskHandle_t taskHandle;
    QueueHandle_t queue;
  } rx;
  int cb;
  int i2s_bits_per_sample, data_bits_per_sample;
} i2s_status_t;

static task_handle_t i2s_tx_task_id, i2s_rx_task_id, i2s_disposal_task_id;

static i2s_status_t i2s_status[MAX_I2C_NUM];

// LUA
static void i2s_tx_task( task_param_t param, task_prio_t prio ) {
  int i2s_id = (int)param;
  i2s_status_t *is = &i2s_status[i2s_id];

  if (is->cb != LUA_NOREF) {
    lua_State *L = lua_getstate();
    lua_rawgeti(L, LUA_REGISTRYINDEX, is->cb);
    lua_pushinteger( L, i2s_id );
    lua_pushstring( L, "tx" );
    luaL_pcallx( L, 2, 0 );
  }
}

static void i2s_rx_task( task_param_t param, task_prio_t prio ) {
  int i2s_id = (int)param;
  i2s_status_t *is = &i2s_status[i2s_id];

  if (is->cb != LUA_NOREF) {
    lua_State *L = lua_getstate();
    lua_rawgeti(L, LUA_REGISTRYINDEX, is->cb);
    lua_pushinteger( L, i2s_id );
    lua_pushstring( L, "rx" );
    luaL_pcallx( L, 2, 0 );
  }
}

static void i2s_disposal_task( task_param_t param, task_prio_t prio ) {
  lua_State *L = lua_getstate();
  int ref = (int)param;

  luaL_unref( L, LUA_REGISTRYINDEX, ref );
}

// RTOS
static void task_I2S_rx( void *pvParameters ) {
  int i2s_id = (int)pvParameters;
  i2s_status_t *is = &i2s_status[i2s_id];

  i2s_event_t i2s_event;

  for (;;) {
    // process I2S RX events
    xQueueReceive( is->rx.queue, &i2s_event, portMAX_DELAY );
    if (i2s_event.type == I2S_EVENT_RX_DONE) {
      task_post_high( i2s_rx_task_id, i2s_id );
    }
  }
}

static void task_I2S_tx( void *pvParameters ) {
  int i2s_id = (int)pvParameters;
  i2s_status_t *is = &i2s_status[i2s_id];

  i2s_tx_data_t tx_data;

  for (;;) {
    // request new TX data
    task_post_high( i2s_tx_task_id, i2s_id );

    // get TX data from Lua task
    // just peek the item to keep it in the queue so that node_i2s_stop can find and unref it if
    // this task was deleted before posting i2s_disposal_task
    xQueuePeek( is->tx.queue, &tx_data, portMAX_DELAY );

    // write TX data to I2S, note that this call might block
    size_t dummy;
    if (is->data_bits_per_sample == is->i2s_bits_per_sample) {
      i2s_write( i2s_id, tx_data.data, tx_data.len, &dummy, portMAX_DELAY );
    } else {
      i2s_write_expand( i2s_id, tx_data.data, tx_data.len, is->data_bits_per_sample, is->i2s_bits_per_sample, &dummy, portMAX_DELAY );
    }

    // notify Lua to dispose object
    task_post_low( i2s_disposal_task_id, tx_data.ref );
    // tx data is going to be unref'ed, so finally remove it from the queue
    xQueueReceive( is->tx.queue, &tx_data, portMAX_DELAY );
  }
}

// Lua: start( i2s_id, {}, callback )
static int node_i2s_start( lua_State *L )
{
  int i2s_id = luaL_checkinteger( L, 1 );
  I2S_CHECK_ID( i2s_id );
  i2s_status_t *is = &i2s_status[i2s_id];

  int top = lua_gettop( L );

  luaL_checktable (L, 2);

  i2s_config_t i2s_config;
  memset( &i2s_config, 0, sizeof( i2s_config ) );
  i2s_pin_config_t pin_config;
  memset( &pin_config, 0, sizeof( pin_config ) );

  // temporarily copy option table to top of stack for opt_ functions
  lua_pushvalue(L, 2);
  i2s_config.mode = opt_checkint(L, "mode", I2S_MODE_MASTER | I2S_MODE_TX);
  i2s_config.sample_rate = opt_checkint_range(L, "rate", 44100, 1000, MAX_INT);
  //
  is->data_bits_per_sample   = opt_checkint(L, "bits", 16);
  is->i2s_bits_per_sample    = is->data_bits_per_sample < I2S_BITS_PER_SAMPLE_16BIT ? I2S_BITS_PER_SAMPLE_16BIT : is->data_bits_per_sample;
  i2s_config.bits_per_sample = is->i2s_bits_per_sample;
  //
  i2s_config.channel_format = opt_checkint(L, "channel", I2S_CHANNEL_FMT_RIGHT_LEFT);
  i2s_config.communication_format = opt_checkint(L, "format", I2S_COMM_FORMAT_STAND_I2S);
  i2s_config.dma_desc_num = opt_checkint_range(L, "buffer_count", 2, 2, 128);
  i2s_config.dma_frame_num = opt_checkint_range(L, "buffer_len", i2s_config.sample_rate / 100, 8, 1024);
  i2s_config.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
  //
  pin_config.bck_io_num = opt_checkint(L, "bck_pin", I2S_PIN_NO_CHANGE);
  pin_config.ws_io_num = opt_checkint(L, "ws_pin", I2S_PIN_NO_CHANGE);
  pin_config.data_out_num  = opt_checkint(L, "data_out_pin", I2S_PIN_NO_CHANGE);
  pin_config.data_in_num = opt_checkint(L, "data_in_pin", I2S_PIN_NO_CHANGE);
  //
  i2s_dac_mode_t dac_mode = opt_checkint_range(L, "dac_mode", I2S_DAC_CHANNEL_DISABLE, 0, I2S_DAC_CHANNEL_MAX-1);
  //
  adc1_channel_t adc1_channel = opt_checkint_range(L, "adc1_channel", ADC1_CHANNEL_MAX, ADC1_CHANNEL_0, ADC1_CHANNEL_MAX);

  // handle optional callback functions TX and RX
  lua_settop( L, top );
  if (lua_isfunction( L, 3 )) {
    lua_pushvalue( L, 3 );
    is->cb = luaL_ref( L, LUA_REGISTRYINDEX );
  } else {
    is->cb = LUA_NOREF;
  }

  esp_err_t err;
  if (i2s_config.mode & I2S_MODE_RX) {
    err = i2s_driver_install(i2s_id, &i2s_config, i2s_config.dma_desc_num, &is->rx.queue);
  } else {
    err = i2s_driver_install(i2s_id, &i2s_config, i2s_config.dma_desc_num, NULL);
  }
  if (err != ESP_OK)
    luaL_error( L, "i2s can not start" );

  if (dac_mode != I2S_DAC_CHANNEL_DISABLE) {
    if (i2s_set_dac_mode( dac_mode ) != ESP_OK)
      luaL_error( L, "error setting dac mode" );
  }
  if (adc1_channel != ADC1_CHANNEL_MAX) {
    if (i2s_set_adc_mode( ADC_UNIT_1, adc1_channel ) != ESP_OK)
      luaL_error( L, "error setting adc1 mode" );
  }

  if (i2s_set_pin(i2s_id, &pin_config) != ESP_OK)
    luaL_error( L, "error setting pins" );


  if (i2s_config.mode & I2S_MODE_TX) {
    // prepare TX task
    char pcName[20];
    snprintf( pcName, sizeof(pcName), "I2S_tx_%d", i2s_id );
    pcName[7] = '\0';
    if ((is->tx.queue = xQueueCreate( 2, sizeof( i2s_tx_data_t ) )) == NULL)
      return luaL_error( L, "cannot create queue" );
    xTaskCreate(task_I2S_tx, pcName, 2048, (void *)i2s_id, ESP_TASK_MAIN_PRIO + 1, &is->tx.taskHandle);
  }

  if (i2s_config.mode & I2S_MODE_RX) {
    // prepare RX task
    char pcName[20];
    snprintf( pcName, sizeof(pcName), "I2S_rx_%d", i2s_id );
    pcName[7] = '\0';
    xTaskCreate(task_I2S_rx, pcName, 1024, (void *)i2s_id, ESP_TASK_MAIN_PRIO + 1, &is->rx.taskHandle);
  }

  return 0;
}

// Lua: stop( i2s_id )
static int node_i2s_stop( lua_State *L )
{
  int i2s_id = luaL_checkinteger( L, 1 );
  I2S_CHECK_ID( i2s_id );
  i2s_status_t *is = &i2s_status[i2s_id];

  if (is->tx.taskHandle != NULL) {
    vTaskDelete( is->tx.taskHandle );
    is->tx.taskHandle = NULL;
  }
  if (is->tx.queue != NULL) {
    // unlink pending tx jobs from the queue
    while (uxQueueMessagesWaiting( is->tx.queue ) > 0) {
      i2s_tx_data_t tx_data;
      xQueueReceive( is->tx.queue, &tx_data, portMAX_DELAY );
      luaL_unref( L, LUA_REGISTRYINDEX, tx_data.ref );
    }
    vQueueDelete( is->tx.queue );
    is->tx.queue = NULL;
  }

  if (is->rx.taskHandle != NULL) {
    vTaskDelete( is->rx.taskHandle );
    is->rx.taskHandle = NULL;
  }

  // the rx queue is created and destroyed by the I2S driver
  i2s_driver_uninstall(i2s_id);

  if (is->cb != LUA_NOREF) {
    luaL_unref( L, LUA_REGISTRYINDEX, is->cb );
    is->cb = LUA_NOREF;
  }

  return 0;
}

// Lua: read( i2s_id, bytes[, wait_ms] )
static int node_i2s_read( lua_State *L )
{
  int i2s_id = luaL_checkinteger( L, 1 );
  I2S_CHECK_ID( i2s_id );

  const size_t bytes = luaL_checkinteger( L, 2 );
  int wait_ms = luaL_optint(L, 3, 0);
  char * data = luaM_malloc( L, bytes );
  size_t read;
  if (i2s_read(i2s_id, data, bytes, &read, wait_ms / portTICK_PERIOD_MS) != ESP_OK)
    return luaL_error( L, "I2S driver error" );

  lua_pushlstring(L, data, read);
  luaM_freemem(L, data, bytes);

  return 1;
}

// Lua: write( i2s_id, data )
static int node_i2s_write( lua_State *L )
{
  int stack = 0;

  int i2s_id = luaL_checkinteger( L, ++stack );
  I2S_CHECK_ID( i2s_id );

  i2s_tx_data_t tx_data;
  tx_data.data = (const char *)luaL_checklstring( L, ++stack, &tx_data.len );
  lua_pushvalue( L, stack );
  tx_data.ref = luaL_ref( L, LUA_REGISTRYINDEX );

  // post this to the TX task
  xQueueSendToBack( i2s_status[i2s_id].tx.queue, &tx_data, portMAX_DELAY );

  return 0;
}

// Lua: mute( i2s_id )
static int node_i2s_mute( lua_State *L )
{
  int stack = 0;

  int i2s_id = luaL_checkinteger( L, ++stack );
  I2S_CHECK_ID( i2s_id );

  if (i2s_zero_dma_buffer( i2s_id ) != ESP_OK)
    return luaL_error( L, "I2S driver error" );

  return 0;
}


// Module function map
LROT_BEGIN(i2s, NULL, 0)
  LROT_FUNCENTRY( start, node_i2s_start )
  LROT_FUNCENTRY( stop,  node_i2s_stop )
  LROT_FUNCENTRY( read,  node_i2s_read )
  LROT_FUNCENTRY( write, node_i2s_write )
  LROT_FUNCENTRY( mute,  node_i2s_mute )

  LROT_NUMENTRY( FORMAT_I2S_STAND, I2S_COMM_FORMAT_STAND_I2S )
  LROT_NUMENTRY( FORMAT_I2S_MSB,   I2S_COMM_FORMAT_STAND_MSB )
  LROT_NUMENTRY( FORMAT_PCM_SHORT, I2S_COMM_FORMAT_STAND_PCM_SHORT )
  LROT_NUMENTRY( FORMAT_PCM_LONG,  I2S_COMM_FORMAT_STAND_PCM_LONG )

  LROT_NUMENTRY( CHANNEL_RIGHT_LEFT, I2S_CHANNEL_FMT_RIGHT_LEFT )
  LROT_NUMENTRY( CHANNEL_ALL_LEFT,   I2S_CHANNEL_FMT_ALL_LEFT )
  LROT_NUMENTRY( CHANNEL_ONLY_LEFT,  I2S_CHANNEL_FMT_ONLY_LEFT )
  LROT_NUMENTRY( CHANNEL_ALL_RIGHT,  I2S_CHANNEL_FMT_ALL_RIGHT )
  LROT_NUMENTRY( CHANNEL_ONLY_RIGHT, I2S_CHANNEL_FMT_ONLY_RIGHT )

  LROT_NUMENTRY( MODE_MASTER,       I2S_MODE_MASTER )
  LROT_NUMENTRY( MODE_SLAVE,        I2S_MODE_SLAVE )
  LROT_NUMENTRY( MODE_TX,           I2S_MODE_TX )
  LROT_NUMENTRY( MODE_RX,           I2S_MODE_RX )
  LROT_NUMENTRY( MODE_DAC_BUILT_IN, I2S_MODE_DAC_BUILT_IN )
  LROT_NUMENTRY( MODE_ADC_BUILT_IN, I2S_MODE_ADC_BUILT_IN )
  LROT_NUMENTRY( MODE_PDM,          I2S_MODE_PDM )

  LROT_NUMENTRY( DAC_CHANNEL_DISABLE, I2S_DAC_CHANNEL_DISABLE )
  LROT_NUMENTRY( DAC_CHANNEL_RIGHT,   I2S_DAC_CHANNEL_RIGHT_EN )
  LROT_NUMENTRY( DAC_CHANNEL_LEFT,    I2S_DAC_CHANNEL_LEFT_EN )
  LROT_NUMENTRY( DAC_CHANNEL_BOTH,    I2S_DAC_CHANNEL_BOTH_EN )
LROT_END(i2s, NULL, 0)

int luaopen_i2s( lua_State *L ) {
  for(int i2s_id = 0; i2s_id < MAX_I2C_NUM; i2s_id++) {
    i2s_status_t *is = &i2s_status[i2s_id];
    is->tx.queue = NULL;
    is->tx.taskHandle = NULL;
    is->rx.queue = NULL;
    is->rx.taskHandle = NULL;
    is->cb = LUA_NOREF;
  }
  i2s_tx_task_id = task_get_id( i2s_tx_task );
  i2s_rx_task_id = task_get_id( i2s_rx_task );
  i2s_disposal_task_id = task_get_id( i2s_disposal_task );
  return 0;
}

NODEMCU_MODULE(I2S, "i2s", i2s, luaopen_i2s);
