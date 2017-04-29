// Module for interfacing with i2s hardware

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

#define MAX_I2C_NUM 2
#define I2S_CHECK_ID(id)   if(id >= MAX_I2C_NUM) luaL_error( L, "i2s not exists" )

typedef struct {
  xTaskHandle taskHandle;
  QueueHandle_t *event_queue;
  int event_cb;
} i2s_status_t;

typedef struct {
  i2s_event_t event;
  i2s_status_t* status;
} i2s_event_post_type;

static task_handle_t i2s_event_task_id;

static i2s_status_t i2s_status[MAX_I2C_NUM];

// LUA
static void i2s_event_task( task_param_t param, task_prio_t prio ) {
  i2s_event_post_type *post = (i2s_event_post_type *)param;

  lua_State *L = lua_getstate();
  int event_cb = post->status->event_cb;
  if(event_cb == LUA_NOREF) {
    free( post );
    return;
  }
  lua_rawgeti(L, LUA_REGISTRYINDEX, event_cb);
  if(post->event.type == I2S_EVENT_TX_DONE) 
    lua_pushstring(L, "sent");
  else if(post->event.type == I2S_EVENT_RX_DONE) 
    lua_pushstring(L, "data");
  else
    lua_pushstring(L, "error");
  lua_pushinteger(L, post->event.size);
  free( post );
  lua_call(L, 2, 0);
}

// RTOS
static void task_I2S( void *pvParameters ){
  i2s_status_t* is = (i2s_status_t*)pvParameters;
  
  i2s_event_post_type *post = NULL;

  for (;;){
    if(post == NULL) {
      post = (i2s_event_post_type *)malloc( sizeof( i2s_event_post_type ) );
      post->status = is;
    }

    if( xQueueReceive( is->event_queue, &(post->event), 3 * portTICK_PERIOD_MS ) == pdTRUE ){
    task_post_high( i2s_event_task_id, (task_param_t)post );
    post = NULL;
    }
  }
}

// Lua: start( i2s_id, {}, callback )
static int node_i2s_start( lua_State *L )
{
  int i2s_id = luaL_checkinteger( L, 1 );
  I2S_CHECK_ID( i2s_id );
  
  luaL_checkanytable (L, 2);

  i2s_config_t i2s_config;
  i2s_pin_config_t pin_config;
  
  lua_getfield (L, 2, "mode");
  i2s_config.mode = luaL_optint(L, -1, I2S_MODE_MASTER | I2S_MODE_TX);
  lua_getfield (L, 2, "rate");
  i2s_config.sample_rate = luaL_optint(L, -1, 44100);
  lua_getfield (L, 2, "bits");
  i2s_config.bits_per_sample = 16;
  lua_getfield (L, 2, "channel");
  i2s_config.channel_format = luaL_optint(L, -1, I2S_CHANNEL_FMT_RIGHT_LEFT);
  lua_getfield (L, 2, "format");
  i2s_config.communication_format = luaL_optint(L, -1, I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB);
  lua_getfield (L, 2, "buffer_count");
  i2s_config.dma_buf_count = luaL_optint(L, -1, 2);
  lua_getfield (L, 2, "buffer_len");
  i2s_config.dma_buf_len = luaL_optint(L, -1, i2s_config.sample_rate / 100);
  i2s_config.intr_alloc_flags = ESP_INTR_FLAG_LEVEL2;
  
  lua_getfield (L, 2, "bck_pin");
  pin_config.bck_io_num = luaL_optint(L, -1, I2S_PIN_NO_CHANGE);
  lua_getfield (L, 2, "ws_pin");
  pin_config.ws_io_num = luaL_optint(L, -1, I2S_PIN_NO_CHANGE);
  lua_getfield (L, 2, "data_out_pin");
  pin_config.data_out_num  = luaL_optint(L, -1, I2S_PIN_NO_CHANGE);
  lua_getfield (L, 2, "data_in_pin");
  pin_config.data_in_num = luaL_optint(L, -1, I2S_PIN_NO_CHANGE);
  
  lua_settop(L, 3);
  i2s_status[i2s_id].event_cb = luaL_ref(L, LUA_REGISTRYINDEX);

  esp_err_t err = i2s_driver_install(i2s_id, &i2s_config, i2s_config.dma_buf_count, &i2s_status[i2s_id].event_queue);
  if(err != ESP_OK)
    luaL_error( L, "i2s can not start" );
  i2s_set_pin(i2s_id, &pin_config);

  char pcName[5];
  snprintf( pcName, 5, "I2S%d", i2s_id );
  pcName[4] = '\0';
  xTaskCreate(&task_I2S, pcName, 2048, &i2s_status[i2s_id], ESP_TASK_MAIN_PRIO + 1, &i2s_status[i2s_id].taskHandle);

  return 0;
}

// Lua: stop( i2s_id )
static int node_i2s_stop( lua_State *L )
{
  int i2s_id = luaL_checkinteger( L, 1 );
  I2S_CHECK_ID( i2s_id );
  
  i2s_driver_uninstall(i2s_id);
  i2s_status[i2s_id].event_queue = NULL;
  
  if(i2s_status[i2s_id].taskHandle != NULL) {
    vTaskDelete(i2s_status[i2s_id].taskHandle);
    i2s_status[i2s_id].taskHandle = NULL;
  }
  
  if(i2s_status[i2s_id].event_cb != LUA_NOREF)  {
    luaL_unref(L, LUA_REGISTRYINDEX, i2s_status[i2s_id].event_cb);
    i2s_status[i2s_id].event_cb = LUA_NOREF;
  }
  
  return 0;
}

// Lua: read( i2s_id, bytes[, wait_ms] )
static int node_i2s_read( lua_State *L )
{
  int i2s_id = luaL_checkinteger( L, 1 );
  I2S_CHECK_ID( i2s_id );
  
  size_t bytes = luaL_checkinteger( L, 2 );
  int wait_ms = luaL_optint(L, 3, 0);
  char * data = luaM_malloc( L, bytes );
  size_t read = i2s_read_bytes(i2s_id, data, bytes, wait_ms / portTICK_RATE_MS);
  lua_pushlstring(L, data, read); 
  luaM_free(L, data);
  
  return 1;
}

// Lua: write( i2s_id, data[, wait_ms] )
static int node_i2s_write( lua_State *L )
{
  int i2s_id = luaL_checkinteger( L, 1 );
  I2S_CHECK_ID( i2s_id );
  
  size_t bytes;
  char *data;
  if( lua_istable( L, 2 ) ) {
    bytes = lua_objlen( L, 2 );
    data = (char *)luaM_malloc( L, bytes );
    for( int i = 0; i < bytes; i ++ ) {
      lua_rawgeti( L, 2, i + 1 );
      data[i] = (uint8_t)luaL_checkinteger( L, -1 );
    }
  } else {
		data = (char *) luaL_checklstring(L, 2, &bytes);
  }
  int wait_ms = luaL_optint(L, 3, 0);
  size_t wrote = i2s_write_bytes(i2s_id, data, bytes, wait_ms / portTICK_RATE_MS);
  if( lua_istable( L, 2 ) )
    luaM_free(L, data);
  lua_pushinteger( L, ( lua_Integer ) wrote );

  return 1;
}

// Module function map
static const LUA_REG_TYPE i2s_map[] =
{
  { LSTRKEY( "start" ),       LFUNCVAL( node_i2s_start ) },
  { LSTRKEY( "stop" ),        LFUNCVAL( node_i2s_stop ) },
  { LSTRKEY( "read" ),        LFUNCVAL( node_i2s_read ) },
  { LSTRKEY( "write" ),       LFUNCVAL( node_i2s_write ) },
  { LSTRKEY( "FORMAT_I2S" ),  LNUMVAL( I2S_COMM_FORMAT_I2S  ) },
  { LSTRKEY( "FORMAT_I2S_MSB" ),  LNUMVAL( I2S_COMM_FORMAT_I2S_MSB ) },
  { LSTRKEY( "FORMAT_I2S_LSB" ),  LNUMVAL( I2S_COMM_FORMAT_I2S_LSB ) },
  { LSTRKEY( "FORMAT_PCM" ),  LNUMVAL( I2S_COMM_FORMAT_PCM ) },
  { LSTRKEY( "FORMAT_PCM_SHORT" ),  LNUMVAL( I2S_COMM_FORMAT_PCM_SHORT ) },
  { LSTRKEY( "FORMAT_PCM_LONG" ),  LNUMVAL( I2S_COMM_FORMAT_PCM_LONG ) },
  
  { LSTRKEY( "CHANNEL_RIGHT_LEFT" ), LNUMVAL( I2S_CHANNEL_FMT_RIGHT_LEFT ) },
  { LSTRKEY( "CHANNEL_ALL_LEFT" ), LNUMVAL( I2S_CHANNEL_FMT_ALL_LEFT ) },
  { LSTRKEY( "CHANNEL_ONLY_LEFT" ), LNUMVAL( I2S_CHANNEL_FMT_ONLY_LEFT ) },
  { LSTRKEY( "CHANNEL_ALL_RIGHT" ), LNUMVAL( I2S_CHANNEL_FMT_ALL_RIGHT ) },
  { LSTRKEY( "CHANNEL_ONLY_RIGHT" ), LNUMVAL( I2S_CHANNEL_FMT_ONLY_RIGHT ) },
  
  { LSTRKEY( "MODE_MASTER" ), LNUMVAL( I2S_MODE_MASTER ) },
  { LSTRKEY( "MODE_SLAVE" ), LNUMVAL( I2S_MODE_SLAVE ) },
  { LSTRKEY( "MODE_TX" ), LNUMVAL( I2S_MODE_TX ) },
  { LSTRKEY( "MODE_RX" ), LNUMVAL( I2S_MODE_RX ) },
  { LSTRKEY( "MODE_DAC_BUILT_IN" ), LNUMVAL( I2S_MODE_DAC_BUILT_IN ) },

  { LNILKEY, LNILVAL }
};

int luaopen_i2s( lua_State *L ) {
  for(int i2s_id = 0; i2s_id < MAX_I2C_NUM; i2s_id++) {
    i2s_status[i2s_id].event_queue = NULL;
    i2s_status[i2s_id].taskHandle = NULL;
    i2s_status[i2s_id].event_cb = LUA_NOREF;
  }
  i2s_event_task_id = task_get_id( i2s_event_task );
  return 0;
}

NODEMCU_MODULE(I2S, "i2s", i2s_map, luaopen_i2s);
